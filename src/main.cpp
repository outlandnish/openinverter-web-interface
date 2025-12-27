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
#include "main.h"
#include "can_task.h"
#include "websocket_handlers.h"

#define INVERTER_PORT UART_NUM_1
#define INVERTER_RX 16
#define INVERTER_TX 17
#define UART_TIMEOUT (100 / portTICK_PERIOD_MS)
#define UART_MESSBUF_SIZE 100

// WS2812B_PIN and WS2812B_COUNT are defined in platformio.ini for each board
#ifndef WS2812B_PIN
#define WS2812B_PIN 8  // Fallback default
#endif
#ifndef WS2812B_COUNT
#define WS2812B_COUNT 1  // Fallback default
#endif
#define STATUS_LED_PIN WS2812B_PIN
#define STATUS_LED_COUNT WS2812B_COUNT
#define STATUS_LED_CHANNEL 0

// FreeRTOS Queue handles
QueueHandle_t canCommandQueue = nullptr;
QueueHandle_t canEventQueue = nullptr;

//HardwareSerial Inverter(INVERTER_PORT);

const char* host = "inverter";
bool fastUart = false;
bool fastUartAvailable = true;
char uartMessBuff[UART_MESSBUF_SIZE];
char jsonFileName[50];
//DynamicJsonDocument jsonDoc(30000);

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
static int totalUpdatePages = 0;
static bool updateWasInProgress = false;

// NeoPixel status LED (NEO_GRB + NEO_KHZ800 are typical for WS2812B)
Adafruit_NeoPixel statusLED(STATUS_LED_COUNT, STATUS_LED_PIN, NEO_GRB + NEO_KHZ800);

// Status LED color definitions (using Adafruit_NeoPixel::Color)
const uint32_t LED_OFF = Adafruit_NeoPixel::Color(0, 0, 0);
const uint32_t LED_COMMAND = Adafruit_NeoPixel::Color(0, 0, 255);        // Blue - command processing
const uint32_t LED_CAN_MAP = Adafruit_NeoPixel::Color(0, 255, 255);       // Cyan - CAN mapping
const uint32_t LED_UPDATE = Adafruit_NeoPixel::Color(128, 0, 255);        // Purple - firmware update
const uint32_t LED_WIFI_CONNECTING = Adafruit_NeoPixel::Color(255, 128, 0); // Orange - WiFi connecting
const uint32_t LED_WIFI_CONNECTED = Adafruit_NeoPixel::Color(0, 255, 0);    // Green - WiFi connected
const uint32_t LED_SUCCESS = Adafruit_NeoPixel::Color(0, 255, 0);         // Green - success
const uint32_t LED_ERROR = Adafruit_NeoPixel::Color(255, 0, 0);           // Red - error

// CRC-32 calculation for CAN IO messages (STM32 polynomial 0x04C11DB7)
// This matches the IEEE 802.3 / Ethernet CRC-32 polynomial
uint32_t crc32_word(uint32_t crc, uint32_t word) {
  const uint32_t polynomial = 0x04C11DB7;
  crc ^= word;
  for (int i = 0; i < 32; i++) {
    if (crc & 0x80000000) {
      crc = (crc << 1) ^ polynomial;
    } else {
      crc = crc << 1;
    }
  }
  return crc;
}

// Build CAN IO message with bit packing
// Set useCRC=true for controlcheck=1 (StmCrc8), false for controlcheck=0 (CounterOnly)
void buildCanIoMessage(uint8_t* msg, uint16_t pot, uint16_t pot2, uint8_t canio,
                       uint8_t ctr, uint16_t cruisespeed, uint8_t regenpreset, bool useCRC = false) {
  // Mask inputs to their bit limits
  pot &= CAN_IO_POT_MASK;          // 12 bits
  pot2 &= CAN_IO_POT_MASK;         // 12 bits
  canio &= CAN_IO_CANIO_MASK;      // 6 bits
  ctr &= CAN_IO_COUNTER_MASK;      // 2 bits
  cruisespeed &= CAN_IO_CRUISE_MASK;  // 14 bits
  regenpreset &= CAN_IO_REGEN_MASK;   // 8 bits

  // data[0] (32 bits): pot (0-11), pot2 (12-23), canio (24-29), ctr1 (30-31)
  uint32_t data0 = pot | (pot2 << 12) | (canio << 24) | (ctr << 30);
  msg[0] = data0 & 0xFF;
  msg[1] = (data0 >> 8) & 0xFF;
  msg[2] = (data0 >> 16) & 0xFF;
  msg[3] = (data0 >> 24) & 0xFF;

  // data[1] (32 bits): cruisespeed (0-13), ctr2 (14-15), regenpreset (16-23), crc (24-31)
  uint32_t data1 = cruisespeed | (ctr << 14) | (regenpreset << 16);
  msg[4] = data1 & 0xFF;
  msg[5] = (data1 >> 8) & 0xFF;
  msg[6] = (data1 >> 16) & 0xFF;

  // Calculate CRC-32 if requested, otherwise set to 0
  uint8_t crcByte = 0;
  if (useCRC) {
    uint32_t crc = 0xFFFFFFFF;
    crc = crc32_word(crc, data0);
    crc = crc32_word(crc, data1);
    crcByte = crc & 0xFF;  // Lower 8 bits
  }
  msg[7] = crcByte;
}

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
  if (LittleFS.exists("/devices.json")) {
    File file = LittleFS.open("/devices.json", "r");
    if (file) {
      JsonDocument savedDoc;
      deserializeJson(savedDoc, file);
      file.close();

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

//format bytes
String formatBytes(uint64_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename, AsyncWebServerRequest *request = nullptr){
  if(request && request->hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".bin")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void handleFileRequest(AsyncWebServerRequest *request) {
  String path = request->url();
  if(path.endsWith("/")) path += "index.html";

  String contentType = getContentType(path);
  bool isGzipped = false;

  // Serve web app files from /dist/ folder
  String distPath = "/dist" + path;
  String pathWithGz = distPath + ".gz";

  if(LittleFS.exists(pathWithGz) || LittleFS.exists(distPath)){
    if(LittleFS.exists(pathWithGz)) {
      distPath += ".gz";
      isGzipped = true;
    }
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, distPath, contentType);
    response->addHeader("Cache-Control", "max-age=86400");
    if(isGzipped) {
      response->addHeader("Content-Encoding", "gzip");
    }
    request->send(response);
    return;
  }

  // Fallback to root for other files (like wifi.txt, devices.json, etc.)
  isGzipped = false;
  pathWithGz = path + ".gz";
  if(LittleFS.exists(pathWithGz) || LittleFS.exists(path)){
    if(LittleFS.exists(pathWithGz)) {
      path += ".gz";
      isGzipped = true;
    }
    AsyncWebServerResponse *response = request->beginResponse(LittleFS, path, contentType);
    response->addHeader("Cache-Control", "max-age=86400");
    if(isGzipped) {
      response->addHeader("Content-Encoding", "gzip");
    }
    request->send(response);
    return;
  }

  request->send(404, "text/plain", "FileNotFound");
}


void uart_readUntill(char val)
{
  int retVal;
  do
  {
    retVal = uart_read_bytes(INVERTER_PORT, uartMessBuff, 1, UART_TIMEOUT);
  }
  while((retVal>0) && (uartMessBuff[0] != val));
}

bool uart_readStartsWith(const char *val)
{
  bool retVal = false;
  int rxBytes = uart_read_bytes(INVERTER_PORT, uartMessBuff, strnlen(val,UART_MESSBUF_SIZE), UART_TIMEOUT);
  if(rxBytes >= strnlen(val,UART_MESSBUF_SIZE))
  {
    if(strncmp(val, uartMessBuff, strnlen(val,UART_MESSBUF_SIZE))==0)
      retVal = true;
    uartMessBuff[rxBytes] = 0;
    DBG_OUTPUT_PORT.println(uartMessBuff);
  }
  return retVal;
}




bool loadWiFiCredentials(String &ssid, String &password) {
  if (!LittleFS.exists("/wifi.txt")) {
    DBG_OUTPUT_PORT.println("wifi.txt not found in LittleFS");
    return false;
  }

  File file = LittleFS.open("/wifi.txt", "r");
  if (!file) {
    DBG_OUTPUT_PORT.println("Failed to open wifi.txt");
    return false;
  }

  // Read SSID (first line)
  ssid = file.readStringUntil('\n');
  ssid.trim();

  // Read Password (second line)
  password = file.readStringUntil('\n');
  password.trim();

  file.close();

  if (ssid.length() == 0) {
    DBG_OUTPUT_PORT.println("SSID is empty in wifi.txt");
    return false;
  }

  DBG_OUTPUT_PORT.println("WiFi credentials loaded from wifi.txt");
  DBG_OUTPUT_PORT.print("SSID: ");
  DBG_OUTPUT_PORT.println(ssid);

  return true;
}

void setup(void){
  DBG_OUTPUT_PORT.begin(115200);

  // Initialize status LED (NeoPixel)
  statusLED.begin();
  statusLEDOff();

  //Start SPI Flash file system
  LittleFS.begin(false, "/littlefs", 10, "littlefs");

  //WIFI INIT
  String wifiSSID, wifiPassword;
  bool staConnected = false;

  if (loadWiFiCredentials(wifiSSID, wifiPassword)) {
    WiFi.mode(WIFI_STA);
    //WiFi.setPhyMode(WIFI_PHY_MODE_11B);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_19_5dBm);//25); //dbm
    WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());

    DBG_OUTPUT_PORT.print("Connecting to WiFi");
    setStatusLED(LED_WIFI_CONNECTING);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
      delay(500);
      DBG_OUTPUT_PORT.print(".");
      attempts++;
    }
    DBG_OUTPUT_PORT.println();

    if (WiFi.status() == WL_CONNECTED) {
      DBG_OUTPUT_PORT.println("WiFi connected!");
      DBG_OUTPUT_PORT.print("IP address: ");
      DBG_OUTPUT_PORT.println(WiFi.localIP());
      setStatusLED(LED_WIFI_CONNECTED);
      delay(1000); // Show connected status for 1 second
      statusLEDOff();
      staConnected = true;
    } else {
      DBG_OUTPUT_PORT.println("WiFi connection failed!");
      setStatusLED(LED_ERROR);
      delay(1000); // Show error for 1 second
      statusLEDOff();
    }
  }

  // If STA mode failed or no credentials, start AP mode
  if (!staConnected) {
    DBG_OUTPUT_PORT.println("Starting in AP mode");

    // Generate AP name using MAC address
    uint8_t mac[6];
    WiFi.macAddress(mac);
    String apSSID = "ESP-" + String(mac[4], HEX) + String(mac[5], HEX);
    apSSID.toUpperCase();

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSSID.c_str());

    // Set AP IP to 192.168.4.1
    IPAddress apIP(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, gateway, subnet);

    DBG_OUTPUT_PORT.print("AP Name: ");
    DBG_OUTPUT_PORT.println(apSSID);
    DBG_OUTPUT_PORT.print("AP IP address: ");
    DBG_OUTPUT_PORT.println(WiFi.softAPIP());

    setStatusLED(LED_WIFI_CONNECTED);
    delay(1000);
    statusLEDOff();
  }

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
    if (LittleFS.exists("/devices.json")) {
      File file = LittleFS.open("/devices.json", "r");
      if (file) {
        JsonDocument doc;
        deserializeJson(doc, file);
        file.close();
        if (doc.containsKey("devices") && doc["devices"].containsKey(serial)) {
          const char* name = doc["devices"][serial]["name"];
          if (name) {
            safeCopyString(evt.data.deviceDiscovered.name, name);
          }
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

  // Simple async endpoints for essential functionality
  server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", "1.1.R-WS");
  });

  server.on("/devices", HTTP_GET, [](AsyncWebServerRequest *request){
    String result = OICan::GetSavedDevices();
    request->send(200, "application/json", result);
  });

  server.on("/params/json", HTTP_GET, [](AsyncWebServerRequest *request){
    DBG_OUTPUT_PORT.println("[HTTP] /params/json request received");

    // Require nodeId parameter for all requests (multi-client support)
    if (!request->hasParam("nodeId")) {
      DBG_OUTPUT_PORT.println("[HTTP] Sending 400 - nodeId parameter required");
      request->send(400, "application/json", "{\"error\":\"nodeId parameter is required\"}");
      return;
    }

    int nodeId = request->getParam("nodeId")->value().toInt();
    DBG_OUTPUT_PORT.printf("[HTTP] Fetching params for nodeId: %d\n", nodeId);

    // Download JSON - progress will be sent via WebSocket jsonProgress events
    // No need for HTTP streaming since progress is tracked via WebSocket
    String json = OICan::GetRawJson(nodeId);

    DBG_OUTPUT_PORT.printf("[HTTP] GetRawJson returned %d bytes\n", json.length());

    if (json.isEmpty() || json == "{}") {
      DBG_OUTPUT_PORT.println("[HTTP] Sending 503 - device busy");
      request->send(503, "application/json", "{\"error\":\"Device busy or not connected\"}");
    } else {
      DBG_OUTPUT_PORT.printf("[HTTP] Sending response with %d bytes\n", json.length());
      request->send(200, "application/json", json);
    }
  });

  server.on("/reloadjson", HTTP_GET, [](AsyncWebServerRequest *request){
    DBG_OUTPUT_PORT.println("[HTTP] /reloadjson request received");
    
    // Require nodeId parameter for multi-client support
    if (!request->hasParam("nodeId")) {
      DBG_OUTPUT_PORT.println("[HTTP] Sending 400 - nodeId parameter required");
      request->send(400, "application/json", "{\"error\":\"nodeId parameter is required\"}");
      return;
    }
    
    int nodeId = request->getParam("nodeId")->value().toInt();
    DBG_OUTPUT_PORT.printf("[HTTP] Reloading JSON for nodeId: %d\n", nodeId);
    
    bool success = OICan::ReloadJson(nodeId);
    if (success) {
      request->send(200, "text/plain", "Cached JSON cleared, will reload from device");
    } else {
      request->send(503, "text/plain", "Device busy, cannot reload");
    }
  });

  // Firmware update upload endpoint for remote devices via CAN
  server.on("/ota/upload", HTTP_POST,
    // Request complete handler
    [](AsyncWebServerRequest *request) {
      // Firmware update completion is handled via WebSocket events
      // The actual update runs asynchronously in the background
      request->send(200, "text/plain", "Firmware upload started");
    },
    // Upload handler
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      static File firmwareFile;
      static String firmwareFilePath = "/firmware_update.bin";

      if (!index) {
        DBG_OUTPUT_PORT.printf("OTA Upload Start: %s (%zu bytes)\n", filename.c_str(), request->contentLength());

        // Check if device is connected and idle
        if (!OICan::IsIdle()) {
          DBG_OUTPUT_PORT.println("OTA Upload failed - device not idle");
          JsonDocument doc;
          doc["event"] = "otaError";
          doc["data"]["error"] = "Device is busy or not connected";
          String output;
          serializeJson(doc, output);
          ws.textAll(output);
          return;
        }

        // Delete old firmware file if it exists
        if (LittleFS.exists(firmwareFilePath)) {
          LittleFS.remove(firmwareFilePath);
        }

        // Create new firmware file
        firmwareFile = LittleFS.open(firmwareFilePath, "w");
        if (!firmwareFile) {
          DBG_OUTPUT_PORT.println("Failed to create firmware file");
          JsonDocument doc;
          doc["event"] = "otaError";
          doc["data"]["error"] = "Failed to create firmware file";
          String output;
          serializeJson(doc, output);
          ws.textAll(output);
          return;
        }

        setStatusLED(LED_UPDATE);
      }

      // Write chunk to file
      if (firmwareFile && len > 0) {
        size_t written = firmwareFile.write(data, len);
        if (written != len) {
          DBG_OUTPUT_PORT.printf("Failed to write firmware chunk (wrote %zu of %zu bytes)\n", written, len);
          firmwareFile.close();

          JsonDocument doc;
          doc["event"] = "otaError";
          doc["data"]["error"] = "Failed to write firmware data";
          String output;
          serializeJson(doc, output);
          ws.textAll(output);

          setStatusLED(LED_ERROR);
          return;
        }
      }

      if (final) {
        firmwareFile.close();
        DBG_OUTPUT_PORT.printf("Firmware file saved: %zu bytes\n", index + len);

        // Start firmware update process
        totalUpdatePages = OICan::StartUpdate(firmwareFilePath);
        DBG_OUTPUT_PORT.printf("Starting firmware update - %d pages to send\n", totalUpdatePages);

        // Send initial progress
        JsonDocument doc;
        doc["event"] = "otaProgress";
        doc["data"]["progress"] = 0;
        String output;
        serializeJson(doc, output);
        ws.textAll(output);
      }
    }
  );

  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
    // If query parameters are provided, update settings
    if (request->hasArg("canRXPin") || request->hasArg("canTXPin") ||
        request->hasArg("canSpeed") || request->hasArg("scanStartNode") ||
        request->hasArg("scanEndNode")) {

      if (request->hasArg("canRXPin")) {
        config.setCanRXPin(request->arg("canRXPin").toInt());
      }
      if (request->hasArg("canTXPin")) {
        config.setCanTXPin(request->arg("canTXPin").toInt());
      }
      if (request->hasArg("canEnablePin")) {
        config.setCanEnablePin(request->arg("canEnablePin").toInt());
      }
      if (request->hasArg("canSpeed")) {
        config.setCanSpeed(request->arg("canSpeed").toInt());
      }
      if (request->hasArg("scanStartNode")) {
        config.setScanStartNode(request->arg("scanStartNode").toInt());
      }
      if (request->hasArg("scanEndNode")) {
        config.setScanEndNode(request->arg("scanEndNode").toInt());
      }

      config.saveSettings();
      request->send(200, "text/plain", "Settings saved successfully");
    } else {
      // Return current settings as JSON
      JsonDocument doc;
      doc["canRXPin"] = config.getCanRXPin();
      doc["canTXPin"] = config.getCanTXPin();
      doc["canEnablePin"] = config.getCanEnablePin();
      doc["canSpeed"] = config.getCanSpeed();
      doc["scanStartNode"] = config.getScanStartNode();
      doc["scanEndNode"] = config.getScanEndNode();

      String output;
      serializeJson(doc, output);
      request->send(200, "application/json", output);
    }
  });

  // Serve files - catch all handler
  server.onNotFound(handleFileRequest);

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

      setStatusLED(LED_SUCCESS);
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
