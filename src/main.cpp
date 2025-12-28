#include <WiFi.h>
#include <WiFiClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <Ticker.h>
#include <StreamString.h>
#include <LittleFS.h>
#include <time.h>
#include <map>
#include <set>
#include <deque>
#include <string>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "oi_can.h"
#include "config.h"
#include "models/can_types.h"
#include "models/can_command.h"
#include "models/can_event.h"
#include "models/interval_messages.h"
#include "utils/string_utils.h"
#include "utils/websocket_helpers.h"
#include "utils/can_io_utils.h"
#include "managers/device_storage.h"
#include "main.h"
#include "can_task.h"
#include "websocket_handlers.h"
#include "http_handlers.h"
#include "wifi_setup.h"

#define INVERTER_PORT UART_NUM_1
#define INVERTER_RX 16
#define INVERTER_TX 17
#define UART_TIMEOUT (100 / portTICK_PERIOD_MS)
#define UART_MESSBUF_SIZE 100


// FreeRTOS Queue handles
QueueHandle_t canCommandQueue = nullptr;
QueueHandle_t canEventQueue = nullptr;

const char* host = "inverter";

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
Config config;

// Spot values streaming (controlled by CAN task)
std::vector<int> spotValuesParamIds; // Parameter IDs to monitor
uint32_t spotValuesInterval = 1000; // Default 1000ms
uint32_t lastSpotValuesTime = 0; // Last time we collected spot values

// Spot values batching to prevent WebSocket queue overflow
std::map<int, double> spotValuesBatch; // Accumulated spot values (map auto-replaces duplicates)
std::map<int, double> latestSpotValues; // Persistent cache of latest values (never cleared)

// Interval CAN message sending
std::vector<IntervalCanMessage> intervalCanMessages;

// CAN IO interval message sending
CanIoInterval canIoInterval = {false, 0x3F, 0, 0, 0, 0, 0, 100, 0, 0, false};

// Device locking for multi-client support
std::map<uint8_t, uint32_t> deviceLocks; // nodeId -> WebSocket client ID
std::map<uint32_t, uint8_t> clientDevices; // WebSocket client ID -> nodeId

// Firmware update progress tracking
static int lastReportedPage = -1;
int totalUpdatePages = 0;  // Non-static so http_handlers.cpp can access it
static bool updateWasInProgress = false;

// WebSocket broadcast helper
void broadcastToWebSocket(const char* event, const char* data) {
  JsonDocument doc;
  doc["event"] = event;
  JsonDocument dataDoc;
  deserializeJson(dataDoc, data);
  doc["data"] = dataDoc;

  String output;
  serializeJson(doc, output);
  ws.textAll(output);
}

void broadcastDeviceDiscovery(uint8_t nodeId, const char* serial, uint32_t lastSeen) {
  JsonDocument doc;
  doc["event"] = "deviceDiscovered";

  JsonObject data = doc["data"].to<JsonObject>();
  data["nodeId"] = nodeId;
  data["serial"] = serial;
  data["lastSeen"] = lastSeen;

  // Look up device name from devices.json
  JsonDocument savedDoc;
  if (DeviceStorage::loadDevices(savedDoc)) {
    if (savedDoc.containsKey("devices")) {
      JsonObject devices = savedDoc["devices"].as<JsonObject>();
      if (devices.containsKey(serial)) {
        JsonObject device = devices[serial].as<JsonObject>();
        if (device.containsKey("name")) {
          data["name"] = device["name"].as<String>();
        }
      }
    }
  }

  String output;
  serializeJson(doc, output);
  ws.textAll(output);
  DBG_OUTPUT_PORT.printf("Broadcast device discovery: %s\n", output.c_str());
}

// Queue-based spot values collection (asynchronous, non-blocking)
std::deque<int> spotValuesRequestQueue; // Queue of pending parameter requests

// Flush accumulated spot values batch to WebSocket
void flushSpotValuesBatch() {
  if (spotValuesBatch.empty()) {
    return;
  }

  // Build event with all accumulated values
  CANEvent evt;
  evt.type = EVT_SPOT_VALUES;
  evt.data.spotValues.timestamp = millis();

  // Build JSON string with all batched values
  JsonDocument doc;
  for (const auto& pair : spotValuesBatch) {
    doc[String(pair.first)] = pair.second;
  }
  serializeJson(doc, evt.data.spotValues.valuesJson);

  xQueueSend(canEventQueue, &evt, 0);

  // Clear the batch
  spotValuesBatch.clear();
}

// Process spot values queue - called every loop iteration
void processSpotValuesQueue() {
  // Try to send one request from queue (if rate limit allows)
  if (!spotValuesRequestQueue.empty()) {
    int paramId = spotValuesRequestQueue.front();

    // Try to send request (non-blocking, respects rate limit)
    if (OICan::RequestValue(paramId)) {
      // Request sent successfully, remove from queue
      spotValuesRequestQueue.pop_front();
    }
    // If request failed (rate limit or TX queue full), leave it in queue and try next iteration
  }

  // Try to receive and accumulate responses in batch
  int responseParamId;
  double value;

  if (OICan::TryGetValueResponse(responseParamId, value, 0)) {
    // Got a response - add to batch (map auto-replaces if param already exists)
    spotValuesBatch[responseParamId] = value;
    // Also update persistent cache for getParamValues
    latestSpotValues[responseParamId] = value;
  }
}

// Reload the request queue with all parameter IDs
void reloadSpotValuesQueue() {
  if (!OICan::IsIdle()) {
    return;
  }

  // Flush any accumulated values from previous cycle at user-requested interval
  flushSpotValuesBatch();

  // Clear existing queue and reload with all parameters
  spotValuesRequestQueue.clear();
  for (int paramId : spotValuesParamIds) {
    spotValuesRequestQueue.push_back(paramId);
  }
}

// WebSocket event handler
void onWebSocketEvent(AsyncWebSocket* server, AsyncWebSocketClient* client, AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    DBG_OUTPUT_PORT.printf("WebSocket client #%lu connected from %s\n", (unsigned long)client->id(), client->remoteIP().toString().c_str());

    // Send current scanning status
    JsonDocument doc;
    doc["event"] = "scanStatus";
    doc["data"]["active"] = OICan::IsContinuousScanActive();
    String output;
    serializeJson(doc, output);
    client->text(output);

    // Send saved devices
    String devices = OICan::GetSavedDevices();
    JsonDocument devicesMsg;
    devicesMsg["event"] = "savedDevices";
    JsonDocument devicesData;
    deserializeJson(devicesData, devices);
    devicesMsg["data"] = devicesData;
    String devicesOutput;
    serializeJson(devicesMsg, devicesOutput);
    client->text(devicesOutput);

  } else if (type == WS_EVT_DISCONNECT) {
    DBG_OUTPUT_PORT.printf("WebSocket client #%lu disconnected\n", (unsigned long)client->id());

    // Release any device lock held by this client
    uint32_t clientId = client->id();
    if (clientDevices.count(clientId) > 0) {
      uint8_t nodeId = clientDevices[clientId];
      deviceLocks.erase(nodeId);
      clientDevices.erase(clientId);
      DBG_OUTPUT_PORT.printf("Released device lock for node %d (client #%lu disconnected)\n", nodeId, (unsigned long)clientId);
      
      // Notify other clients that the device is now available
      JsonDocument doc;
      doc["event"] = "deviceUnlocked";
      doc["data"]["nodeId"] = nodeId;
      String output;
      serializeJson(doc, output);
      ws.textAll(output);
    }

  } else if (type == WS_EVT_DATA) {
    AwsFrameInfo* info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0; // null terminate
      String message = (char*)data;

      DBG_OUTPUT_PORT.printf("WebSocket message: %s\n", message.c_str());

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, message);
      if (error) {
        DBG_OUTPUT_PORT.printf("JSON parse error: %s\n", error.c_str());
        return;
      }

      // Dispatch to WebSocket handler
      dispatchWebSocketMessage(client, doc);
    }
  }
}

void setup(void){
  DBG_OUTPUT_PORT.begin(115200);

  // Initialize status LED (NeoPixel)
  StatusLED::instance().begin();
  statusLEDOff();

  //Start SPI Flash file system
  LittleFS.begin(false, "/littlefs", 10, "littlefs");

  //WIFI INIT
  WiFiSetup::initialize();

  MDNS.begin(host);

  config.load();

  if (config.getCanEnablePin() > 0) {
    pinMode(config.getCanEnablePin(), OUTPUT);
    digitalWrite(config.getCanEnablePin(), LOW);
  }

  // Initialize CAN transceiver shutdown and standby pins (canipulator environments)
  #ifdef CAN0_SHUTDOWN_PIN
    pinMode(CAN0_SHUTDOWN_PIN, OUTPUT);
    digitalWrite(CAN0_SHUTDOWN_PIN, LOW);
    DBG_OUTPUT_PORT.printf("CAN0 shutdown pin %d set to LOW\n", CAN0_SHUTDOWN_PIN);
  #endif

  #ifdef CAN0_STANDBY_PIN
    pinMode(CAN0_STANDBY_PIN, OUTPUT);
    digitalWrite(CAN0_STANDBY_PIN, LOW);
    DBG_OUTPUT_PORT.printf("CAN0 standby pin %d set to LOW\n", CAN0_STANDBY_PIN);
  #endif

  #ifdef CAN1_SHUTDOWN_PIN
    pinMode(CAN1_SHUTDOWN_PIN, OUTPUT);
    digitalWrite(CAN1_SHUTDOWN_PIN, LOW);
    DBG_OUTPUT_PORT.printf("CAN1 shutdown pin %d set to LOW\n", CAN1_SHUTDOWN_PIN);
  #endif

  #ifdef CAN1_STANDBY_PIN
    pinMode(CAN1_STANDBY_PIN, OUTPUT);
    digitalWrite(CAN1_STANDBY_PIN, LOW);
    DBG_OUTPUT_PORT.printf("CAN1 standby pin %d set to LOW\n", CAN1_STANDBY_PIN);
  #endif

  // Initialize CAN bus at startup
  DBG_OUTPUT_PORT.println("Initializing CAN bus...");
  OICan::BaudRate baud = config.getCanSpeed() == 0 ? OICan::Baud125k : (config.getCanSpeed() == 1 ? OICan::Baud250k : OICan::Baud500k);
  OICan::InitCAN(baud, config.getCanTXPin(), config.getCanRXPin());

  // Create FreeRTOS queues
  canCommandQueue = xQueueCreate(10, sizeof(CANCommand));
  canEventQueue = xQueueCreate(20, sizeof(CANEvent));

  if (canCommandQueue == nullptr || canEventQueue == nullptr) {
    DBG_OUTPUT_PORT.println("ERROR: Failed to create queues!");
    return;
  }

  DBG_OUTPUT_PORT.println("Queues created successfully");

  // Update device discovery callback to post events
  OICan::SetDeviceDiscoveryCallback([](uint8_t nodeId, const char* serial, uint32_t lastSeen) {
    CANEvent evt;
    evt.type = EVT_DEVICE_DISCOVERED;
    evt.data.deviceDiscovered.nodeId = nodeId;
    safeCopyString(evt.data.deviceDiscovered.serial, serial);
    evt.data.deviceDiscovered.lastSeen = lastSeen;
    evt.data.deviceDiscovered.name[0] = '\0'; // Will be filled from devices.json

    // Look up name from devices.json
    JsonDocument doc;
    if (DeviceStorage::loadDevices(doc)) {
      if (doc.containsKey("devices") && doc["devices"].containsKey(serial)) {
        const char* name = doc["devices"][serial]["name"];
        if (name) {
          safeCopyString(evt.data.deviceDiscovered.name, name);
        }
      }
    }

    xQueueSend(canEventQueue, &evt, 0);
  });

  // Setup scan progress callback to post events
  OICan::SetScanProgressCallback([](uint8_t currentNode, uint8_t startNode, uint8_t endNode) {
    CANEvent evt;
    evt.type = EVT_SCAN_PROGRESS;
    evt.data.scanProgress.currentNode = currentNode;
    evt.data.scanProgress.startNode = startNode;
    evt.data.scanProgress.endNode = endNode;
    xQueueSend(canEventQueue, &evt, 0);
  });

  // Setup connection ready callback to post events when device is truly connected (IDLE state)
  OICan::SetConnectionReadyCallback([](uint8_t nodeId, const char* serial) {
    DBG_OUTPUT_PORT.printf("[Callback] Connection ready - node %d, serial %s\n", nodeId, serial);
    CANEvent evt;
    evt.type = EVT_CONNECTED;
    evt.data.connected.nodeId = nodeId;
    safeCopyString(evt.data.connected.serial, serial);
    xQueueSend(canEventQueue, &evt, 0);
  });

  // Setup JSON download progress callback
  OICan::SetJsonDownloadProgressCallback([](int bytesReceived) {
    JsonDocument doc;
    doc["event"] = "jsonProgress";
    doc["data"]["bytesReceived"] = bytesReceived;
    doc["data"]["complete"] = (bytesReceived == 0);
    doc["data"]["totalBytes"] = OICan::GetJsonTotalSize(); // Add total size if known
    String output;
    serializeJson(doc, output);
    ws.textAll(output);
  });

  // Spawn CAN task
  // On dual-core ESP32: Pin to Core 0 (WiFi/WebSocket runs on Core 1)
  // On single-core ESP32-C3: FreeRTOS will time-slice both tasks on Core 0
#if CONFIG_FREERTOS_UNICORE
  xTaskCreate(
    canTask,           // Task function
    "CAN_Task",        // Name
    8192,              // Stack size (bytes)
    nullptr,           // Parameters
    1,                 // Priority (1 = low, higher than idle)
    nullptr            // Task handle
  );
  DBG_OUTPUT_PORT.println("CAN task spawned (single-core mode)");
#else
  xTaskCreatePinnedToCore(
    canTask,           // Task function
    "CAN_Task",        // Name
    8192,              // Stack size (bytes)
    nullptr,           // Parameters
    1,                 // Priority (1 = low, higher than idle)
    nullptr,           // Task handle
    0                  // Core 0
  );
  DBG_OUTPUT_PORT.println("CAN task spawned on Core 0 (dual-core mode)");
#endif

  // WebSocket setup
  ws.onEvent(onWebSocketEvent);
  server.addHandler(&ws);

  //SERVER INIT
  ArduinoOTA.setHostname(host);
  ArduinoOTA.begin();

  // Register all HTTP routes
  registerHttpRoutes(server);

  server.begin();

  MDNS.addService("http", "tcp", 80);
}

// Firmware update progress monitoring
void processFirmwareUpdateProgress() {
  bool updateInProgress = OICan::IsUpdateInProgress();

  if (updateInProgress) {
    updateWasInProgress = true;
    int currentPage = OICan::GetCurrentUpdatePage();

    // Only send progress updates when page changes
    if (currentPage != lastReportedPage && totalUpdatePages > 0) {
      lastReportedPage = currentPage;
      int progress = (currentPage * 100) / totalUpdatePages;

      DBG_OUTPUT_PORT.printf("Firmware update progress: page %d/%d (%d%%)\n",
                            currentPage, totalUpdatePages, progress);

      JsonDocument doc;
      doc["event"] = "otaProgress";
      doc["data"]["progress"] = progress;
      String output;
      serializeJson(doc, output);
      ws.textAll(output);
    }
  } else if (updateWasInProgress) {
    // Update just finished
    updateWasInProgress = false;
    lastReportedPage = -1;

    // Check if we completed successfully or had an error
    // If state went back to IDLE or OBTAINSERIAL, it was successful
    if (OICan::IsIdle() || !OICan::IsIdle()) {
      DBG_OUTPUT_PORT.println("Firmware update completed successfully");

      JsonDocument doc;
      doc["event"] = "otaSuccess";
      String output;
      serializeJson(doc, output);
      ws.textAll(output);

      setStatusLED(StatusLED::SUCCESS);
      delay(1000);
      statusLEDOff();

      totalUpdatePages = 0;
    }
  }
}

// Process events from CAN task and broadcast to WebSocket
void processCANEvents() {
  CANEvent evt;

  // Process all pending events (non-blocking)
  while (xQueueReceive(canEventQueue, &evt, 0) == pdTRUE) {
    JsonDocument doc;
    doc["event"] = "";
    JsonObject data = doc["data"].to<JsonObject>();

    switch(evt.type) {
      case EVT_DEVICE_DISCOVERED:
        doc["event"] = "deviceDiscovered";
        data["nodeId"] = evt.data.deviceDiscovered.nodeId;
        data["serial"] = evt.data.deviceDiscovered.serial;
        data["lastSeen"] = evt.data.deviceDiscovered.lastSeen;
        if (evt.data.deviceDiscovered.name[0] != '\0') {
          data["name"] = evt.data.deviceDiscovered.name;
        }
        break;

      case EVT_SCAN_STATUS:
        doc["event"] = "scanStatus";
        data["active"] = evt.data.scanStatus.active;
        break;

      case EVT_SCAN_PROGRESS:
        doc["event"] = "scanProgress";
        data["currentNode"] = evt.data.scanProgress.currentNode;
        data["startNode"] = evt.data.scanProgress.startNode;
        data["endNode"] = evt.data.scanProgress.endNode;
        break;

      case EVT_CONNECTED:
        doc["event"] = "connected";
        data["nodeId"] = evt.data.connected.nodeId;
        data["serial"] = evt.data.connected.serial;
        break;

      case EVT_NODE_ID_INFO:
        doc["event"] = "nodeIdInfo";
        data["id"] = evt.data.nodeIdInfo.id;
        data["speed"] = evt.data.nodeIdInfo.speed;
        break;

      case EVT_NODE_ID_SET:
        doc["event"] = "nodeIdSet";
        data["id"] = evt.data.nodeIdSet.id;
        data["speed"] = evt.data.nodeIdSet.speed;
        break;

      case EVT_SPOT_VALUES_STATUS:
        doc["event"] = "spotValuesStatus";
        data["active"] = evt.data.spotValuesStatus.active;
        if (evt.data.spotValuesStatus.active) {
          data["interval"] = evt.data.spotValuesStatus.interval;
          data["paramCount"] = evt.data.spotValuesStatus.paramCount;
        }
        break;

      case EVT_SPOT_VALUES:
        doc["event"] = "spotValues";
        data["timestamp"] = evt.data.spotValues.timestamp;
        {
          JsonDocument valuesDoc;
          deserializeJson(valuesDoc, evt.data.spotValues.valuesJson);
          data["values"] = valuesDoc;
        }
        break;

      case EVT_DEVICE_NAME_SET:
        doc["event"] = "deviceNameSet";
        data["success"] = evt.data.deviceNameSet.success;
        data["serial"] = evt.data.deviceNameSet.serial;
        data["name"] = evt.data.deviceNameSet.name;
        break;

      case EVT_DEVICE_DELETED:
        doc["event"] = "deviceDeleted";
        data["success"] = evt.data.deviceDeleted.success;
        data["serial"] = evt.data.deviceDeleted.serial;
        break;

      case EVT_DEVICE_RENAMED:
        doc["event"] = "deviceRenamed";
        data["success"] = evt.data.deviceRenamed.success;
        data["serial"] = evt.data.deviceRenamed.serial;
        data["name"] = evt.data.deviceRenamed.name;
        break;

      case EVT_CAN_MESSAGE_SENT:
        doc["event"] = "canMessageSent";
        data["success"] = evt.data.canMessageSent.success;
        data["canId"] = evt.data.canMessageSent.canId;
        break;

      case EVT_CAN_INTERVAL_STATUS:
        doc["event"] = "canIntervalStatus";
        data["active"] = evt.data.canIntervalStatus.active;
        data["intervalId"] = evt.data.canIntervalStatus.intervalId;
        if (evt.data.canIntervalStatus.active) {
          data["intervalMs"] = evt.data.canIntervalStatus.intervalMs;
        }
        break;

      case EVT_CANIO_INTERVAL_STATUS:
        doc["event"] = "canIoIntervalStatus";
        data["active"] = evt.data.canIoIntervalStatus.active;
        if (evt.data.canIoIntervalStatus.active) {
          data["intervalMs"] = evt.data.canIoIntervalStatus.intervalMs;
        }
        break;

      case EVT_ERROR:
        doc["event"] = "error";
        data["message"] = evt.data.error.message;
        break;

      default:
        continue; // Unknown event, skip
    }

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
  }
}

void loop(void){
  // note: ArduinoOTA.handle() calls MDNS.update();
  ws.cleanupClients();
  ArduinoOTA.handle();

  // Process events from CAN task
  processCANEvents();

  // Process firmware update progress
  processFirmwareUpdateProgress();

  // NOTE: OICan::Loop() now runs in CAN task, don't call here
}
