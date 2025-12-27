#include "can_task.h"
#include "freertos/queue.h"
#include "models/can_command.h"
#include "models/can_event.h"
#include "models/interval_messages.h"
#include "utils/string_utils.h"
#include "oi_can.h"
#include "config.h"
#include <vector>
#include <map>
#include <deque>
#include <Arduino.h>

// External declarations for globals and functions defined in main.cpp
extern QueueHandle_t canCommandQueue;
extern QueueHandle_t canEventQueue;
extern Config config;

// Spot values globals
extern std::vector<int> spotValuesParamIds;
extern uint32_t spotValuesInterval;
extern uint32_t lastSpotValuesTime;
extern std::map<int, double> latestSpotValues;

// Interval messages globals
extern std::vector<IntervalCanMessage> intervalCanMessages;

// CAN IO interval global
extern CanIoInterval canIoInterval;

// Spot values request queue
extern std::deque<int> spotValuesRequestQueue;

// Helper functions from main.cpp
extern void reloadSpotValuesQueue();
extern void processSpotValuesQueue();
extern void flushSpotValuesBatch();
extern void buildCanIoMessage(uint8_t* msg, uint16_t pot, uint16_t pot2, uint8_t canio,
                               uint8_t counter, uint16_t cruisespeed, uint8_t regenpreset, bool useCrc);

#define DBG_OUTPUT_PORT Serial

// ============================================================================
// Command Handler Functions
// ============================================================================

void handleStartScanCommand(const CANCommand& cmd) {
  DBG_OUTPUT_PORT.printf("[CAN Task] Starting scan %d-%d\n", cmd.data.scan.start, cmd.data.scan.end);
  bool scanStarted = OICan::StartContinuousScan(cmd.data.scan.start, cmd.data.scan.end);

  if (scanStarted) {
    // Send scan status event if scan actually started
    CANEvent evt;
    evt.type = EVT_SCAN_STATUS;
    evt.data.scanStatus.active = true;
    xQueueSend(canEventQueue, &evt, 0);
  } else {
    // Send error event if scan failed to start
    DBG_OUTPUT_PORT.println("[CAN Task] Scan failed to start - device busy");
    CANEvent evt;
    evt.type = EVT_ERROR;
    safeCopyString(evt.data.error.message, "Cannot start scan - device is busy. Please wait or disconnect from the current device.");
    xQueueSend(canEventQueue, &evt, 0);
  }
}

void handleStopScanCommand(const CANCommand& cmd) {
  DBG_OUTPUT_PORT.println("[CAN Task] Stopping scan");
  OICan::StopContinuousScan();

  // Send scan status event
  CANEvent evt;
  evt.type = EVT_SCAN_STATUS;
  evt.data.scanStatus.active = false;
  xQueueSend(canEventQueue, &evt, 0);
}

void handleConnectCommand(const CANCommand& cmd) {
  DBG_OUTPUT_PORT.printf("[CAN Task] Connecting to node %d\n", cmd.data.connect.nodeId);

  // Clear interval messages when switching devices
  if (!intervalCanMessages.empty()) {
    DBG_OUTPUT_PORT.printf("[CAN Task] Clearing %d interval message(s) on device switch\n", intervalCanMessages.size());
    intervalCanMessages.clear();
  }

  OICan::BaudRate baud = config.getCanSpeed() == 0 ? OICan::Baud125k : (config.getCanSpeed() == 1 ? OICan::Baud250k : OICan::Baud500k);
  OICan::Init(cmd.data.connect.nodeId, baud, config.getCanTXPin(), config.getCanRXPin());
  // Connected event will be sent via ConnectionReadyCallback when device reaches IDLE state
}

void handleSetNodeIdCommand(const CANCommand& cmd) {
  DBG_OUTPUT_PORT.printf("[CAN Task] Setting node ID to %d\n", cmd.data.setNodeId.nodeId);

  // Clear interval messages when switching devices
  if (!intervalCanMessages.empty()) {
    DBG_OUTPUT_PORT.printf("[CAN Task] Clearing %d interval message(s) on node ID change\n", intervalCanMessages.size());
    intervalCanMessages.clear();
  }

  OICan::BaudRate baud = config.getCanSpeed() == 0 ? OICan::Baud125k : (config.getCanSpeed() == 1 ? OICan::Baud250k : OICan::Baud500k);
  OICan::Init(cmd.data.setNodeId.nodeId, baud, config.getCanTXPin(), config.getCanRXPin());

  // Send node ID set event
  CANEvent evt;
  evt.type = EVT_NODE_ID_SET;
  evt.data.nodeIdSet.id = OICan::GetNodeId();
  evt.data.nodeIdSet.speed = OICan::GetBaudRate();
  xQueueSend(canEventQueue, &evt, 0);
}

void handleGetNodeIdCommand(const CANCommand& cmd) {
  CANEvent evt;
  evt.type = EVT_NODE_ID_INFO;
  evt.data.nodeIdInfo.id = OICan::GetNodeId();
  evt.data.nodeIdInfo.speed = OICan::GetBaudRate();
  xQueueSend(canEventQueue, &evt, 0);
}

void handleSetDeviceNameCommand(const CANCommand& cmd) {
  bool success = OICan::SaveDeviceName(cmd.data.setDeviceName.serial, cmd.data.setDeviceName.name, cmd.data.setDeviceName.nodeId);
  CANEvent evt;
  evt.type = EVT_DEVICE_NAME_SET;
  evt.data.deviceNameSet.success = success;
  safeCopyString(evt.data.deviceNameSet.serial, cmd.data.setDeviceName.serial);
  safeCopyString(evt.data.deviceNameSet.name, cmd.data.setDeviceName.name);
  xQueueSend(canEventQueue, &evt, 0);
}

void handleStartSpotValuesCommand(const CANCommand& cmd) {
  // Start streaming spot values
  spotValuesInterval = cmd.data.spotValues.interval;
  spotValuesParamIds.clear();
  for(int i = 0; i < cmd.data.spotValues.paramCount; i++) {
    spotValuesParamIds.push_back(cmd.data.spotValues.paramIds[i]);
  }
  lastSpotValuesTime = millis();

  // Load initial request queue
  reloadSpotValuesQueue();

  CANEvent evt;
  evt.type = EVT_SPOT_VALUES_STATUS;
  evt.data.spotValuesStatus.active = true;
  evt.data.spotValuesStatus.interval = spotValuesInterval;
  evt.data.spotValuesStatus.paramCount = spotValuesParamIds.size();
  xQueueSend(canEventQueue, &evt, 0);
}

void handleStopSpotValuesCommand(const CANCommand& cmd) {
  // Flush any remaining batched values before stopping
  flushSpotValuesBatch();

  spotValuesParamIds.clear();
  spotValuesRequestQueue.clear(); // Clear the request queue
  latestSpotValues.clear(); // Clear persistent cache

  CANEvent evt;
  evt.type = EVT_SPOT_VALUES_STATUS;
  evt.data.spotValuesStatus.active = false;
  xQueueSend(canEventQueue, &evt, 0);
}

void handleDeleteDeviceCommand(const CANCommand& cmd) {
  bool success = OICan::DeleteDevice(cmd.data.deleteDevice.serial);
  CANEvent evt;
  evt.type = EVT_DEVICE_DELETED;
  evt.data.deviceDeleted.success = success;
  safeCopyString(evt.data.deviceDeleted.serial, cmd.data.deleteDevice.serial);
  xQueueSend(canEventQueue, &evt, 0);
}

void handleRenameDeviceCommand(const CANCommand& cmd) {
  bool success = OICan::SaveDeviceName(cmd.data.renameDevice.serial, cmd.data.renameDevice.name, -1);
  CANEvent evt;
  evt.type = EVT_DEVICE_RENAMED;
  evt.data.deviceRenamed.success = success;
  safeCopyString(evt.data.deviceRenamed.serial, cmd.data.renameDevice.serial);
  safeCopyString(evt.data.deviceRenamed.name, cmd.data.renameDevice.name);
  xQueueSend(canEventQueue, &evt, 0);
}

void handleSendCanMessageCommand(const CANCommand& cmd) {
  bool success = OICan::SendCanMessage(
    cmd.data.sendCanMessage.canId,
    cmd.data.sendCanMessage.data,
    cmd.data.sendCanMessage.dataLength
  );

  CANEvent evt;
  evt.type = EVT_CAN_MESSAGE_SENT;
  evt.data.canMessageSent.success = success;
  evt.data.canMessageSent.canId = cmd.data.sendCanMessage.canId;
  xQueueSend(canEventQueue, &evt, 0);
}

void handleStartCanIntervalCommand(const CANCommand& cmd) {
  // Check if interval ID already exists and remove it
  for (auto it = intervalCanMessages.begin(); it != intervalCanMessages.end(); ) {
    if (it->id == cmd.data.startCanInterval.intervalId) {
      it = intervalCanMessages.erase(it);
    } else {
      ++it;
    }
  }

  // Add new interval message
  IntervalCanMessage msg;
  msg.id = String(cmd.data.startCanInterval.intervalId);
  msg.canId = cmd.data.startCanInterval.canId;
  msg.dataLength = cmd.data.startCanInterval.dataLength;
  for (uint8_t i = 0; i < msg.dataLength; i++) {
    msg.data[i] = cmd.data.startCanInterval.data[i];
  }
  msg.intervalMs = cmd.data.startCanInterval.intervalMs;
  msg.lastSentTime = millis();
  intervalCanMessages.push_back(msg);

  DBG_OUTPUT_PORT.printf("[CAN Task] Started interval message: ID=%s, CAN=0x%03lX, Interval=%lums\n",
                         msg.id.c_str(), (unsigned long)msg.canId, (unsigned long)msg.intervalMs);

  CANEvent evt;
  evt.type = EVT_CAN_INTERVAL_STATUS;
  evt.data.canIntervalStatus.active = true;
  safeCopyString(evt.data.canIntervalStatus.intervalId, cmd.data.startCanInterval.intervalId);
  evt.data.canIntervalStatus.intervalMs = cmd.data.startCanInterval.intervalMs;
  xQueueSend(canEventQueue, &evt, 0);
}

void handleStopCanIntervalCommand(const CANCommand& cmd) {
  bool found = false;
  for (auto it = intervalCanMessages.begin(); it != intervalCanMessages.end(); ) {
    if (it->id == cmd.data.stopCanInterval.intervalId) {
      DBG_OUTPUT_PORT.printf("[CAN Task] Stopped interval message: ID=%s\n", it->id.c_str());
      it = intervalCanMessages.erase(it);
      found = true;
    } else {
      ++it;
    }
  }

  if (found) {
    CANEvent evt;
    evt.type = EVT_CAN_INTERVAL_STATUS;
    evt.data.canIntervalStatus.active = false;
    safeCopyString(evt.data.canIntervalStatus.intervalId, cmd.data.stopCanInterval.intervalId);
    evt.data.canIntervalStatus.intervalMs = 0;
    xQueueSend(canEventQueue, &evt, 0);
  }
}

void handleStartCanIoIntervalCommand(const CANCommand& cmd) {
  // Update CAN IO interval state
  canIoInterval.active = true;
  canIoInterval.canId = cmd.data.startCanIoInterval.canId;
  canIoInterval.pot = cmd.data.startCanIoInterval.pot;
  canIoInterval.pot2 = cmd.data.startCanIoInterval.pot2;
  canIoInterval.canio = cmd.data.startCanIoInterval.canio;
  canIoInterval.cruisespeed = cmd.data.startCanIoInterval.cruisespeed;
  canIoInterval.regenpreset = cmd.data.startCanIoInterval.regenpreset;
  canIoInterval.intervalMs = cmd.data.startCanIoInterval.intervalMs;
  canIoInterval.useCrc = cmd.data.startCanIoInterval.useCrc;
  canIoInterval.lastSentTime = millis();
  // Start with counter=1 to avoid matching the last message from a previous session
  // This prevents ERR_CANCOUNTER if the last message before restart was also counter=0
  canIoInterval.sequenceCounter = 1;

  DBG_OUTPUT_PORT.printf("[CAN Task] Started CAN IO interval: CAN=0x%03lX, canio=0x%02X, Interval=%lums\n",
                         (unsigned long)canIoInterval.canId, canIoInterval.canio, (unsigned long)canIoInterval.intervalMs);

  // Send status event
  CANEvent evt;
  evt.type = EVT_CANIO_INTERVAL_STATUS;
  evt.data.canIoIntervalStatus.active = true;
  evt.data.canIoIntervalStatus.intervalMs = canIoInterval.intervalMs;
  xQueueSend(canEventQueue, &evt, 0);
}

void handleStopCanIoIntervalCommand(const CANCommand& cmd) {
  canIoInterval.active = false;
  DBG_OUTPUT_PORT.println("[CAN Task] Stopped CAN IO interval");

  // Send status event
  CANEvent evt;
  evt.type = EVT_CANIO_INTERVAL_STATUS;
  evt.data.canIoIntervalStatus.active = false;
  evt.data.canIoIntervalStatus.intervalMs = 0;
  xQueueSend(canEventQueue, &evt, 0);
}

void handleUpdateCanIoFlagsCommand(const CANCommand& cmd) {
  if (canIoInterval.active) {
    canIoInterval.pot = cmd.data.updateCanIoFlags.pot;
    canIoInterval.pot2 = cmd.data.updateCanIoFlags.pot2;
    canIoInterval.canio = cmd.data.updateCanIoFlags.canio;
    canIoInterval.cruisespeed = cmd.data.updateCanIoFlags.cruisespeed;
    canIoInterval.regenpreset = cmd.data.updateCanIoFlags.regenpreset;
    DBG_OUTPUT_PORT.printf("[CAN Task] Updated CAN IO flags (canio=0x%02X)\n", canIoInterval.canio);
  } else {
    DBG_OUTPUT_PORT.println("[CAN Task] Ignoring update - CAN IO interval not active");
  }
}

// ============================================================================
// Periodic Task Functions
// ============================================================================

void processSpotValuesSequence() {
  if (!spotValuesParamIds.empty()) {
    // Check if it's time to reload the queue
    if ((millis() - lastSpotValuesTime) >= spotValuesInterval) {
      lastSpotValuesTime = millis();
      reloadSpotValuesQueue();
    }
    // Always process queue and responses
    processSpotValuesQueue();
  }
}

void sendIntervalMessages() {
  uint32_t currentTime = millis();
  for (auto& msg : intervalCanMessages) {
    if ((currentTime - msg.lastSentTime) >= msg.intervalMs) {
      msg.lastSentTime = currentTime;
      OICan::SendCanMessage(msg.canId, msg.data, msg.dataLength);
    }
  }
}

void sendCanIoIntervalMessage() {
  if (!canIoInterval.active) {
    return;
  }

  uint32_t currentTime = millis();
  if ((currentTime - canIoInterval.lastSentTime) >= canIoInterval.intervalMs) {
    canIoInterval.lastSentTime = currentTime;

    // Build the CAN IO message with current state and sequence counter
    // useCrc from user setting: false for counter-only mode (controlcheck=0), true for CRC mode (controlcheck=1)
    uint8_t msgData[8];
    buildCanIoMessage(msgData, canIoInterval.pot, canIoInterval.pot2, canIoInterval.canio,
                      canIoInterval.sequenceCounter, canIoInterval.cruisespeed, canIoInterval.regenpreset, canIoInterval.useCrc);

    // Send the message
    OICan::SendCanMessage(canIoInterval.canId, msgData, 8);

    // Increment sequence counter (0-3)
    canIoInterval.sequenceCounter = (canIoInterval.sequenceCounter + 1) & 0x03;
  }
}

// ============================================================================
// Command Dispatch Function
// ============================================================================

void dispatchCommand(const CANCommand& cmd) {
  switch(cmd.type) {
    case CMD_START_SCAN:
      handleStartScanCommand(cmd);
      break;

    case CMD_STOP_SCAN:
      handleStopScanCommand(cmd);
      break;

    case CMD_CONNECT:
      handleConnectCommand(cmd);
      break;

    case CMD_SET_NODE_ID:
      handleSetNodeIdCommand(cmd);
      break;

    case CMD_GET_NODE_ID:
      handleGetNodeIdCommand(cmd);
      break;

    case CMD_SET_DEVICE_NAME:
      handleSetDeviceNameCommand(cmd);
      break;

    case CMD_START_SPOT_VALUES:
      handleStartSpotValuesCommand(cmd);
      break;

    case CMD_STOP_SPOT_VALUES:
      handleStopSpotValuesCommand(cmd);
      break;

    case CMD_DELETE_DEVICE:
      handleDeleteDeviceCommand(cmd);
      break;

    case CMD_RENAME_DEVICE:
      handleRenameDeviceCommand(cmd);
      break;

    case CMD_SEND_CAN_MESSAGE:
      handleSendCanMessageCommand(cmd);
      break;

    case CMD_START_CAN_INTERVAL:
      handleStartCanIntervalCommand(cmd);
      break;

    case CMD_STOP_CAN_INTERVAL:
      handleStopCanIntervalCommand(cmd);
      break;

    case CMD_START_CANIO_INTERVAL:
      handleStartCanIoIntervalCommand(cmd);
      break;

    case CMD_STOP_CANIO_INTERVAL:
      handleStopCanIoIntervalCommand(cmd);
      break;

    case CMD_UPDATE_CANIO_FLAGS:
      handleUpdateCanIoFlagsCommand(cmd);
      break;
  }
}

void canTask(void* parameter) {
  DBG_OUTPUT_PORT.println("[CAN Task] Started");

  CANCommand cmd;

  while(true) {
    // Process commands from queue
    if (xQueueReceive(canCommandQueue, &cmd, 0) == pdTRUE) {
      dispatchCommand(cmd);
    }

    // Periodic tasks
    processSpotValuesSequence();
    sendIntervalMessages();
    sendCanIoIntervalMessage();

    // Run CAN processing loop
    OICan::Loop();

    // Small delay to prevent task starvation
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
