#include "can_task.h"

#include <Arduino.h>

#include "config.h"
#include "driver/twai.h"
#include "firmware/update_handler.h"
#include "freertos/queue.h"
#include "oi_can.h"

#include "managers/can_interval_manager.h"
#include "managers/device_connection.h"
#include "managers/device_discovery.h"
#include "managers/spot_values_manager.h"
#include "models/can_command.h"
#include "models/can_event.h"
#include "models/can_types.h"
#include "protocols/sdo_protocol.h"
#include "utils/can_utils.h"
#include "utils/string_utils.h"

// External declarations for globals defined in main.cpp
extern QueueHandle_t canCommandQueue;
extern QueueHandle_t canEventQueue;
extern Config config;

// CAN I/O queues
QueueHandle_t canTxQueue = nullptr;
QueueHandle_t sdoResponseQueue = nullptr;

#define DBG_OUTPUT_PORT Serial

// ============================================================================
// Queue Initialization
// ============================================================================

void initCanQueues() {
  if (canTxQueue == nullptr) {
    canTxQueue = xQueueCreate(CAN_TX_QUEUE_SIZE, sizeof(twai_message_t));
    if (canTxQueue == nullptr) {
      DBG_OUTPUT_PORT.println("[CAN Task] Failed to create canTxQueue");
    }
  }
  if (sdoResponseQueue == nullptr) {
    sdoResponseQueue = xQueueCreate(SDO_RESPONSE_QUEUE_SIZE, sizeof(twai_message_t));
    if (sdoResponseQueue == nullptr) {
      DBG_OUTPUT_PORT.println("[CAN Task] Failed to create sdoResponseQueue");
    }
  }
}

// ============================================================================
// Command Handler Functions
// ============================================================================

void handleStartScanCommand(const CANCommand& cmd) {
  DBG_OUTPUT_PORT.printf("[CAN Task] Starting scan %d-%d\n", cmd.data.scan.start, cmd.data.scan.end);
  bool scanStarted = OICan::StartContinuousScan(cmd.data.scan.start, cmd.data.scan.end);

  if (scanStarted) {
    CANEvent evt;
    evt.type = EVT_SCAN_STATUS;
    evt.data.scanStatus.active = true;
    xQueueSend(canEventQueue, &evt, 0);
  } else {
    DBG_OUTPUT_PORT.println("[CAN Task] Scan failed to start - device busy");
    CANEvent evt;
    evt.type = EVT_ERROR;
    safeCopyString(evt.data.error.message,
                   "Cannot start scan - device is busy. Please wait or disconnect from the current device.");
    xQueueSend(canEventQueue, &evt, 0);
  }
}

void handleStopScanCommand(const CANCommand& cmd) {
  DBG_OUTPUT_PORT.println("[CAN Task] Stopping scan");
  DeviceDiscovery::instance().stopContinuousScan();

  CANEvent evt;
  evt.type = EVT_SCAN_STATUS;
  evt.data.scanStatus.active = false;
  xQueueSend(canEventQueue, &evt, 0);
}

void handleConnectCommand(const CANCommand& cmd) {
  DBG_OUTPUT_PORT.printf("[CAN Task] Connecting to node %d\n", cmd.data.connect.nodeId);

  // Stop scanning if active (prevents duplicate device events during connection)
  if (DeviceDiscovery::instance().isScanActive()) {
    DBG_OUTPUT_PORT.println("[CAN Task] Stopping scan before connecting");
    DeviceDiscovery::instance().stopContinuousScan();

    // Notify frontend that scan has stopped
    CANEvent evt;
    evt.type = EVT_SCAN_STATUS;
    evt.data.scanStatus.active = false;
    xQueueSend(canEventQueue, &evt, 0);
  }

  // Stop spot values (parameter IDs are device-specific)
  if (SpotValuesManager::instance().isActive()) {
    DBG_OUTPUT_PORT.println("[CAN Task] Stopping spot values before connecting");
    SpotValuesManager::instance().stop();

    CANEvent evt;
    evt.type = EVT_SPOT_VALUES_STATUS;
    evt.data.spotValuesStatus.active = false;
    xQueueSend(canEventQueue, &evt, 0);
  }

  // Clear interval messages when switching devices
  CanIntervalManager::instance().clearAllIntervals();

  OICan::Init(cmd.data.connect.nodeId, config.getBaudRateEnum(), config.getCanTXPin(), config.getCanRXPin());
}

void handleSetNodeIdCommand(const CANCommand& cmd) {
  DBG_OUTPUT_PORT.printf("[CAN Task] Setting node ID to %d\n", cmd.data.setNodeId.nodeId);

  // Clear interval messages when switching devices
  CanIntervalManager::instance().clearAllIntervals();

  OICan::Init(cmd.data.setNodeId.nodeId, config.getBaudRateEnum(), config.getCanTXPin(), config.getCanRXPin());

  CANEvent evt;
  evt.type = EVT_NODE_ID_SET;
  evt.data.nodeIdSet.id = DeviceConnection::instance().getNodeId();
  evt.data.nodeIdSet.speed = DeviceConnection::instance().getBaudRate();
  xQueueSend(canEventQueue, &evt, 0);
}

void handleGetNodeIdCommand(const CANCommand& cmd) {
  CANEvent evt;
  evt.type = EVT_NODE_ID_INFO;
  evt.data.nodeIdInfo.id = DeviceConnection::instance().getNodeId();
  evt.data.nodeIdInfo.speed = DeviceConnection::instance().getBaudRate();
  xQueueSend(canEventQueue, &evt, 0);
}

void handleSetDeviceNameCommand(const CANCommand& cmd) {
  bool success = DeviceDiscovery::instance().saveDeviceName(cmd.data.setDeviceName.serial, cmd.data.setDeviceName.name,
                                                            cmd.data.setDeviceName.nodeId);

  CANEvent evt;
  evt.type = EVT_DEVICE_NAME_SET;
  evt.data.deviceNameSet.success = success;
  safeCopyString(evt.data.deviceNameSet.serial, cmd.data.setDeviceName.serial);
  safeCopyString(evt.data.deviceNameSet.name, cmd.data.setDeviceName.name);
  xQueueSend(canEventQueue, &evt, 0);
}

void handleStartSpotValuesCommand(const CANCommand& cmd) {
  SpotValuesManager::instance().start(cmd.data.spotValues.interval, cmd.data.spotValues.paramIds,
                                      cmd.data.spotValues.paramCount);

  CANEvent evt;
  evt.type = EVT_SPOT_VALUES_STATUS;
  evt.data.spotValuesStatus.active = true;
  evt.data.spotValuesStatus.interval = cmd.data.spotValues.interval;
  evt.data.spotValuesStatus.paramCount = cmd.data.spotValues.paramCount;
  xQueueSend(canEventQueue, &evt, 0);
}

void handleStopSpotValuesCommand(const CANCommand& cmd) {
  SpotValuesManager::instance().stop();

  CANEvent evt;
  evt.type = EVT_SPOT_VALUES_STATUS;
  evt.data.spotValuesStatus.active = false;
  xQueueSend(canEventQueue, &evt, 0);
}

void handleDeleteDeviceCommand(const CANCommand& cmd) {
  bool success = DeviceDiscovery::instance().deleteDevice(cmd.data.deleteDevice.serial);

  CANEvent evt;
  evt.type = EVT_DEVICE_DELETED;
  evt.data.deviceDeleted.success = success;
  safeCopyString(evt.data.deviceDeleted.serial, cmd.data.deleteDevice.serial);
  xQueueSend(canEventQueue, &evt, 0);
}

void handleRenameDeviceCommand(const CANCommand& cmd) {
  bool success =
      DeviceDiscovery::instance().saveDeviceName(cmd.data.renameDevice.serial, cmd.data.renameDevice.name, -1);

  CANEvent evt;
  evt.type = EVT_DEVICE_RENAMED;
  evt.data.deviceRenamed.success = success;
  safeCopyString(evt.data.deviceRenamed.serial, cmd.data.renameDevice.serial);
  safeCopyString(evt.data.deviceRenamed.name, cmd.data.renameDevice.name);
  xQueueSend(canEventQueue, &evt, 0);
}

void handleSendCanMessageCommand(const CANCommand& cmd) {
  bool success = OICan::SendCanMessage(cmd.data.sendCanMessage.canId, cmd.data.sendCanMessage.data,
                                       cmd.data.sendCanMessage.dataLength);

  CANEvent evt;
  evt.type = EVT_CAN_MESSAGE_SENT;
  evt.data.canMessageSent.success = success;
  evt.data.canMessageSent.canId = cmd.data.sendCanMessage.canId;
  xQueueSend(canEventQueue, &evt, 0);
}

void handleStartCanIntervalCommand(const CANCommand& cmd) {
  CanIntervalManager::instance().startInterval(cmd.data.startCanInterval.intervalId, cmd.data.startCanInterval.canId,
                                               cmd.data.startCanInterval.data, cmd.data.startCanInterval.dataLength,
                                               cmd.data.startCanInterval.intervalMs);

  CANEvent evt;
  evt.type = EVT_CAN_INTERVAL_STATUS;
  evt.data.canIntervalStatus.active = true;
  safeCopyString(evt.data.canIntervalStatus.intervalId, cmd.data.startCanInterval.intervalId);
  evt.data.canIntervalStatus.intervalMs = cmd.data.startCanInterval.intervalMs;
  xQueueSend(canEventQueue, &evt, 0);
}

void handleStopCanIntervalCommand(const CANCommand& cmd) {
  bool found = CanIntervalManager::instance().hasInterval(cmd.data.stopCanInterval.intervalId);
  CanIntervalManager::instance().stopInterval(cmd.data.stopCanInterval.intervalId);

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
  CanIntervalManager::instance().startCanIoInterval(
      cmd.data.startCanIoInterval.canId, cmd.data.startCanIoInterval.pot, cmd.data.startCanIoInterval.pot2,
      cmd.data.startCanIoInterval.canio, cmd.data.startCanIoInterval.cruisespeed,
      cmd.data.startCanIoInterval.regenpreset, cmd.data.startCanIoInterval.intervalMs,
      cmd.data.startCanIoInterval.useCrc);

  CANEvent evt;
  evt.type = EVT_CANIO_INTERVAL_STATUS;
  evt.data.canIoIntervalStatus.active = true;
  evt.data.canIoIntervalStatus.intervalMs = cmd.data.startCanIoInterval.intervalMs;
  xQueueSend(canEventQueue, &evt, 0);
}

void handleStopCanIoIntervalCommand(const CANCommand& cmd) {
  CanIntervalManager::instance().stopCanIoInterval();

  CANEvent evt;
  evt.type = EVT_CANIO_INTERVAL_STATUS;
  evt.data.canIoIntervalStatus.active = false;
  evt.data.canIoIntervalStatus.intervalMs = 0;
  xQueueSend(canEventQueue, &evt, 0);
}

void handleUpdateCanIoFlagsCommand(const CANCommand& cmd) {
  CanIntervalManager::instance().updateCanIoFlags(
      cmd.data.updateCanIoFlags.pot, cmd.data.updateCanIoFlags.pot2, cmd.data.updateCanIoFlags.canio,
      cmd.data.updateCanIoFlags.cruisespeed, cmd.data.updateCanIoFlags.regenpreset);
}

// ============================================================================
// Periodic Task Functions
// ============================================================================

void processSpotValuesSequence() {
  SpotValuesManager& spotMgr = SpotValuesManager::instance();

  if (spotMgr.isActive()) {
    // Check if it's time to reload the queue
    if ((millis() - spotMgr.getLastCollectionTime()) >= spotMgr.getInterval()) {
      spotMgr.updateLastCollectionTime(millis());
      spotMgr.reloadQueue();
    }
    // Always process queue and responses
    spotMgr.processQueue();
  }
}

// ============================================================================
// Command Dispatch Function
// ============================================================================

void dispatchCommand(const CANCommand& cmd) {
  switch (cmd.type) {
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
    // Task 34: Commands handled via SDO protocol layer (not dispatched here)
    case CMD_SAVE_TO_FLASH:
    case CMD_LOAD_FROM_FLASH:
    case CMD_LOAD_DEFAULTS:
    case CMD_START_DEVICE:
    case CMD_STOP_DEVICE:
    case CMD_RESET_DEVICE:
    case CMD_SET_VALUE:
    case CMD_CLEAR_CAN_MAP:
    case CMD_GET_CAN_MAPPINGS:
    case CMD_ADD_CAN_MAPPING:
    case CMD_REMOVE_CAN_MAPPING:
    case CMD_LIST_ERRORS:
      // These commands are processed directly in oi_can via SDO protocol
      // They use canTxQueue/sdoResponseQueue, not the command dispatch
      DBG_OUTPUT_PORT.printf("[CAN Task] Command %d should use SDO protocol layer\n", cmd.type);
      break;
  }
}

// ============================================================================
// TWAI Driver Initialization
// ============================================================================

static bool configureTwaiDriver(BaudRate baud, int txPin, int rxPin, const twai_filter_config_t& filter) {
  twai_general_config_t g_config = {.mode = TWAI_MODE_NORMAL,
                                    .tx_io = static_cast<gpio_num_t>(txPin),
                                    .rx_io = static_cast<gpio_num_t>(rxPin),
                                    .clkout_io = TWAI_IO_UNUSED,
                                    .bus_off_io = TWAI_IO_UNUSED,
                                    .tx_queue_len = 30,
                                    .rx_queue_len = 30,
                                    .alerts_enabled = TWAI_ALERT_NONE,
                                    .clkout_divider = 0,
                                    .intr_flags = 0};

  twai_stop();
  twai_driver_uninstall();

  twai_timing_config_t t_config;
  switch (baud) {
    case Baud125k:
      t_config = TWAI_TIMING_CONFIG_125KBITS();
      break;
    case Baud250k:
      t_config = TWAI_TIMING_CONFIG_250KBITS();
      break;
    case Baud500k:
      t_config = TWAI_TIMING_CONFIG_500KBITS();
      break;
  }

  if (twai_driver_install(&g_config, &t_config, &filter) == ESP_OK) {
    DBG_OUTPUT_PORT.println("[CAN Driver] TWAI driver installed");
  } else {
    DBG_OUTPUT_PORT.println("[CAN Driver] Failed to install TWAI driver");
    return false;
  }

  if (twai_start() == ESP_OK) {
    DBG_OUTPUT_PORT.println("[CAN Driver] TWAI driver started");
    return true;
  } else {
    DBG_OUTPUT_PORT.println("[CAN Driver] Failed to start TWAI driver");
    return false;
  }
}

bool initCanBusScanning(BaudRate baud, int txPin, int rxPin) {
  DBG_OUTPUT_PORT.println("[CAN Driver] Initializing CAN bus for scanning (SDO + bootloader filter)");

  // Dual filter mode for standard 11-bit IDs:
  // - Filter 0 (bits [31:21]): Bootloader response 0x7DE (exact match)
  // - Filter 1 (bits [15:5]): SDO response range 0x580-0x5FF
  //
  // SDO range: 0x580-0x5FF has common prefix 0b1011 (upper 4 bits),
  // lower 7 bits vary, so we mask those out
  twai_filter_config_t filter = {.acceptance_code =
                                     (uint32_t)(SDO_RESPONSE_BASE_ID << 5) | (uint32_t)(BOOTLOADER_RESPONSE_ID << 21),
                                 .acceptance_mask = (uint32_t)(0x7F << 5) | 0x1F | (uint32_t)(0x1F << 16),
                                 .single_filter = false};

  return configureTwaiDriver(baud, txPin, rxPin, filter);
}

bool initCanBusForDevice(uint8_t nodeId, BaudRate baud, int txPin, int rxPin) {
  DBG_OUTPUT_PORT.printf("[CAN Driver] Initializing CAN bus for device (nodeId=%d)\n", nodeId);

  uint16_t id = SDO_RESPONSE_BASE_ID + nodeId;

  twai_filter_config_t filter = {.acceptance_code = (uint32_t)(id << 5) | (uint32_t)(BOOTLOADER_RESPONSE_ID << 21),
                                 .acceptance_mask = 0x001F001F,
                                 .single_filter = false};

  return configureTwaiDriver(baud, txPin, rxPin, filter);
}

// ============================================================================
// CAN TX Queue Processing
// ============================================================================

static void processTxQueueInternal(int maxFrames) {
  twai_message_t txframe;

  for (int i = 0; i < maxFrames; i++) {
    if (xQueueReceive(canTxQueue, &txframe, 0) == pdTRUE) {
      esp_err_t result = twai_transmit(&txframe, pdMS_TO_TICKS(10));
      if (result != ESP_OK) {
        DBG_OUTPUT_PORT.printf("[CAN TX] Failed to transmit frame ID 0x%lX: err=%d\n",
                               (unsigned long)txframe.identifier, result);
      }
      printCanTx(&txframe);
    } else {
      break;  // Queue empty
    }
  }
}

void processTxQueue() {
  // Process up to a few frames per iteration to avoid blocking
  processTxQueueInternal(5);
}

// ============================================================================
// CAN Message Reception and Processing
// ============================================================================

void receiveAndProcessCanMessages() {
  twai_message_t rxframe;

  if (twai_receive(&rxframe, 0) == ESP_OK) {
    printCanRx(&rxframe);

    if (rxframe.identifier == BOOTLOADER_RESPONSE_ID) {
      FirmwareUpdateHandler::instance().processResponse(&rxframe);
    } else if (rxframe.identifier >= SDO_RESPONSE_BASE_ID && rxframe.identifier <= SDO_RESPONSE_MAX_ID) {
      uint8_t nodeId = rxframe.identifier & 0x7F;
      DeviceDiscovery::instance().updateLastSeenByNodeId(nodeId, millis());

      // Route all SDO responses to the queue for processing by scan or device connection
      if (sdoResponseQueue != nullptr) {
        xQueueSend(sdoResponseQueue, &rxframe, 0);
      }
    } else {
      DBG_OUTPUT_PORT.printf("Received unwanted frame %" PRIu32 "\r\n", rxframe.identifier);
    }
  }
}

void processFirmwareUpdateState() {
  if (FirmwareUpdateHandler::instance().getState() == FirmwareUpdateHandler::REQUEST_JSON) {
    DeviceConnection& conn = DeviceConnection::instance();

    // If device connection is idle, reset firmware update
    if (conn.getState() == DeviceConnection::IDLE) {
      FirmwareUpdateHandler::instance().reset();
      return;
    }

    // If device is in error state, reset firmware update
    if (conn.getState() == DeviceConnection::ERROR) {
      FirmwareUpdateHandler::instance().reset();
      return;
    }

    // Otherwise, wait for serial acquisition to complete
    // The DeviceConnection state machine handles the actual communication
  }
}

// ============================================================================
// CAN Task Main Function
// ============================================================================

void canTask(void* parameter) {
  DBG_OUTPUT_PORT.println("[CAN Task] Started");

  CANCommand cmd;

  while (true) {
    // Process commands from queue
    if (xQueueReceive(canCommandQueue, &cmd, 0) == pdTRUE) {
      dispatchCommand(cmd);
    }

    // Process CAN TX queue (frames from SDO protocol layer)
    processTxQueue();

    // Periodic tasks
    processSpotValuesSequence();
    CanIntervalManager::instance().sendPendingMessages();
    CanIntervalManager::instance().sendCanIoMessage();

    // CAN message reception and routing
    receiveAndProcessCanMessages();

    // Device connection state machine
    DeviceConnection::instance().processConnection();

    // Device scanning
    DeviceDiscovery::instance().processScan();

    // Firmware update state handling
    processFirmwareUpdateState();

    // Small delay to prevent task starvation
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
