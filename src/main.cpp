#include <WiFi.h>
#include <WiFiClient.h>
#include <SmartLeds.h>
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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/uart.h"
#include "oi_can.h"
#include "config.h"

#define DBG_OUTPUT_PORT Serial
#define INVERTER_PORT UART_NUM_1
#define INVERTER_RX 16
#define INVERTER_TX 17
#define UART_TIMEOUT (100 / portTICK_PERIOD_MS)
#define UART_MESSBUF_SIZE 100

#define STATUS_LED_PIN 8
#define STATUS_LED_COUNT 1
#define STATUS_LED_CHANNEL 0

// FreeRTOS Queue handles
QueueHandle_t canCommandQueue = nullptr;
QueueHandle_t canEventQueue = nullptr;

// Command types for CAN task
enum CANCommandType {
  CMD_START_SCAN,
  CMD_STOP_SCAN,
  CMD_CONNECT,
  CMD_SET_NODE_ID,
  CMD_SET_DEVICE_NAME,
  CMD_GET_NODE_ID,
  CMD_START_SPOT_VALUES,
  CMD_STOP_SPOT_VALUES
};

// Event types from CAN task
enum CANEventType {
  EVT_DEVICE_DISCOVERED,
  EVT_SCAN_STATUS,
  EVT_CONNECTED,
  EVT_NODE_ID_INFO,
  EVT_NODE_ID_SET,
  EVT_SPOT_VALUES_STATUS,
  EVT_SPOT_VALUES,
  EVT_DEVICE_NAME_SET
};

// Command message structure
struct CANCommand {
  CANCommandType type;
  union {
    struct {
      uint8_t start;
      uint8_t end;
    } scan;
    struct {
      uint8_t nodeId;
      char serial[50];
    } connect;
    struct {
      uint8_t nodeId;
    } setNodeId;
    struct {
      char serial[50];
      char name[50];
      int nodeId;
    } setDeviceName;
    struct {
      int paramIds[100];
      int paramCount;
      uint32_t interval;
    } spotValues;
  } data;
};

// Event message structure
struct CANEvent {
  CANEventType type;
  union {
    struct {
      uint8_t nodeId;
      char serial[50];
      uint32_t lastSeen;
      char name[50];
    } deviceDiscovered;
    struct {
      bool active;
    } scanStatus;
    struct {
      uint8_t nodeId;
      char serial[50];
    } connected;
    struct {
      uint8_t id;
      uint8_t speed;
    } nodeIdInfo;
    struct {
      uint8_t id;
      uint8_t speed;
    } nodeIdSet;
    struct {
      bool active;
      uint32_t interval;
      int paramCount;
    } spotValuesStatus;
    struct {
      uint32_t timestamp;
      char valuesJson[1024]; // JSON string of values
    } spotValues;
    struct {
      bool success;
      char serial[50];
      char name[50];
    } deviceNameSet;
  } data;
};


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

// Spot values streaming
bool spotValuesActive = false;
uint32_t spotValuesInterval = 1000; // Default 1000ms
Ticker spotValuesTicker;
std::vector<int> spotValuesParamIds; // Parameter IDs to monitor

// NeoPixel status LED
SmartLed statusLED(LED_WS2812B, STATUS_LED_COUNT, STATUS_LED_PIN, STATUS_LED_CHANNEL, DoubleBuffer);

// Status LED color definitions
const Rgb LED_OFF = Rgb(0, 0, 0);
const Rgb LED_COMMAND = Rgb(0, 0, 255);        // Blue - command processing
const Rgb LED_CAN_MAP = Rgb(0, 255, 255);       // Cyan - CAN mapping
const Rgb LED_UPDATE = Rgb(128, 0, 255);        // Purple - firmware update
const Rgb LED_WIFI_CONNECTING = Rgb(255, 128, 0); // Orange - WiFi connecting
const Rgb LED_WIFI_CONNECTED = Rgb(0, 255, 0);    // Green - WiFi connected
const Rgb LED_SUCCESS = Rgb(0, 255, 0);         // Green - success
const Rgb LED_ERROR = Rgb(255, 0, 0);           // Red - error

// Helper functions for status LED
void setStatusLED(Rgb color) {
  statusLED[0] = color;
  statusLED.show();
}

void statusLEDOff() {
  setStatusLED(LED_OFF);
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

// Broadcast spot values to all websocket clients
void broadcastSpotValues() {
  if (!spotValuesActive || spotValuesParamIds.empty()) return;

  JsonDocument doc;
  doc["event"] = "spotValues";
  JsonObject data = doc["data"].to<JsonObject>();

  // Add timestamp (milliseconds since boot)
  data["timestamp"] = millis();

  // Request each parameter value from device
  JsonObject values = data["values"].to<JsonObject>();
  for (int paramId : spotValuesParamIds) {
    double value = OICan::GetValue(paramId);
    values[String(paramId)] = value;
  }

  String output;
  serializeJson(doc, output);
  ws.textAll(output);
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

      String action = doc["action"].as<String>();

      if (action == "startScan") {
        uint8_t start = doc["start"] | 1;
        uint8_t end = doc["end"] | 32;

        CANCommand cmd;
        cmd.type = CMD_START_SCAN;
        cmd.data.scan.start = start;
        cmd.data.scan.end = end;

        if (xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
          DBG_OUTPUT_PORT.println("[WebSocket] Scan start command queued");
        } else {
          DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Failed to queue scan start command");
        }

      } else if (action == "stopScan") {
        CANCommand cmd;
        cmd.type = CMD_STOP_SCAN;

        if (xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
          DBG_OUTPUT_PORT.println("[WebSocket] Scan stop command queued");
        } else {
          DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Failed to queue scan stop command");
        }

      } else if (action == "connect") {
        uint8_t nodeId = doc["nodeId"];
        String serial = doc["serial"] | "";

        CANCommand cmd;
        cmd.type = CMD_CONNECT;
        cmd.data.connect.nodeId = nodeId;
        strncpy(cmd.data.connect.serial, serial.c_str(), sizeof(cmd.data.connect.serial) - 1);

        if (xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
          DBG_OUTPUT_PORT.printf("[WebSocket] Connect command queued (node %d)\n", nodeId);
        } else {
          DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Failed to queue connect command");
        }

      } else if (action == "setDeviceName") {
        String serial = doc["serial"];
        String name = doc["name"];
        int nodeId = doc["nodeId"] | -1;

        CANCommand cmd;
        cmd.type = CMD_SET_DEVICE_NAME;
        strncpy(cmd.data.setDeviceName.serial, serial.c_str(), sizeof(cmd.data.setDeviceName.serial) - 1);
        strncpy(cmd.data.setDeviceName.name, name.c_str(), sizeof(cmd.data.setDeviceName.name) - 1);
        cmd.data.setDeviceName.nodeId = nodeId;

        if (xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
          DBG_OUTPUT_PORT.printf("[WebSocket] Set device name command queued (%s)\n", serial.c_str());
        } else {
          DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Failed to queue set device name command");
        }

      } else if (action == "getNodeId") {
        CANCommand cmd;
        cmd.type = CMD_GET_NODE_ID;

        if (xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
          DBG_OUTPUT_PORT.println("[WebSocket] Get node ID command queued");
        } else {
          DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Failed to queue get node ID command");
        }

      } else if (action == "setNodeId") {
        int id = doc["id"];

        CANCommand cmd;
        cmd.type = CMD_SET_NODE_ID;
        cmd.data.setNodeId.nodeId = id;

        if (xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
          DBG_OUTPUT_PORT.printf("[WebSocket] Set node ID command queued (node %d)\n", id);
        } else {
          DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Failed to queue set node ID command");
        }

      } else if (action == "startSpotValues") {
        CANCommand cmd;
        cmd.type = CMD_START_SPOT_VALUES;

        if (doc.containsKey("paramIds")) {
          JsonArray paramIds = doc["paramIds"].as<JsonArray>();
          cmd.data.spotValues.paramCount = 0;
          for (JsonVariant id : paramIds) {
            if (cmd.data.spotValues.paramCount < 100) {
              cmd.data.spotValues.paramIds[cmd.data.spotValues.paramCount++] = id.as<int>();
            }
          }
        }

        if (doc.containsKey("interval")) {
          cmd.data.spotValues.interval = doc["interval"].as<uint32_t>();
          if (cmd.data.spotValues.interval < 100) cmd.data.spotValues.interval = 100;
          if (cmd.data.spotValues.interval > 10000) cmd.data.spotValues.interval = 10000;
        } else {
          cmd.data.spotValues.interval = 1000;
        }

        if (xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
          DBG_OUTPUT_PORT.printf("[WebSocket] Start spot values command queued (%d params)\n", cmd.data.spotValues.paramCount);
        } else {
          DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Failed to queue start spot values command");
        }

      } else if (action == "stopSpotValues") {
        CANCommand cmd;
        cmd.type = CMD_STOP_SPOT_VALUES;

        if (xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
          DBG_OUTPUT_PORT.println("[WebSocket] Stop spot values command queued");
        } else {
          DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Failed to queue stop spot values command");
        }
      }
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

// CAN processing task - runs independently on separate core
void canTask(void* parameter) {
  DBG_OUTPUT_PORT.println("[CAN Task] Started");

  CANCommand cmd;

  while(true) {
    // Process commands from WebSocket
    if (xQueueReceive(canCommandQueue, &cmd, 0) == pdTRUE) {
      switch(cmd.type) {
        case CMD_START_SCAN:
          DBG_OUTPUT_PORT.printf("[CAN Task] Starting scan %d-%d\n", cmd.data.scan.start, cmd.data.scan.end);
          OICan::StartContinuousScan(cmd.data.scan.start, cmd.data.scan.end);

          // Send scan status event
          {
            CANEvent evt;
            evt.type = EVT_SCAN_STATUS;
            evt.data.scanStatus.active = true;
            xQueueSend(canEventQueue, &evt, 0);
          }
          break;

        case CMD_STOP_SCAN:
          DBG_OUTPUT_PORT.println("[CAN Task] Stopping scan");
          OICan::StopContinuousScan();

          // Send scan status event
          {
            CANEvent evt;
            evt.type = EVT_SCAN_STATUS;
            evt.data.scanStatus.active = false;
            xQueueSend(canEventQueue, &evt, 0);
          }
          break;

        case CMD_CONNECT:
          DBG_OUTPUT_PORT.printf("[CAN Task] Connecting to node %d\n", cmd.data.connect.nodeId);
          {
            OICan::BaudRate baud = config.getCanSpeed() == 0 ? OICan::Baud125k : (config.getCanSpeed() == 1 ? OICan::Baud250k : OICan::Baud500k);
            OICan::Init(cmd.data.connect.nodeId, baud, config.getCanTXPin(), config.getCanRXPin());

            // Send connected event
            CANEvent evt;
            evt.type = EVT_CONNECTED;
            evt.data.connected.nodeId = cmd.data.connect.nodeId;
            strcpy(evt.data.connected.serial, cmd.data.connect.serial);
            xQueueSend(canEventQueue, &evt, 0);
          }
          break;

        case CMD_SET_NODE_ID:
          DBG_OUTPUT_PORT.printf("[CAN Task] Setting node ID to %d\n", cmd.data.setNodeId.nodeId);
          {
            OICan::BaudRate baud = config.getCanSpeed() == 0 ? OICan::Baud125k : (config.getCanSpeed() == 1 ? OICan::Baud250k : OICan::Baud500k);
            OICan::Init(cmd.data.setNodeId.nodeId, baud, config.getCanTXPin(), config.getCanRXPin());

            // Send node ID set event
            CANEvent evt;
            evt.type = EVT_NODE_ID_SET;
            evt.data.nodeIdSet.id = OICan::GetNodeId();
            evt.data.nodeIdSet.speed = OICan::GetBaudRate();
            xQueueSend(canEventQueue, &evt, 0);
          }
          break;

        case CMD_GET_NODE_ID:
          {
            CANEvent evt;
            evt.type = EVT_NODE_ID_INFO;
            evt.data.nodeIdInfo.id = OICan::GetNodeId();
            evt.data.nodeIdInfo.speed = OICan::GetBaudRate();
            xQueueSend(canEventQueue, &evt, 0);
          }
          break;

        case CMD_SET_DEVICE_NAME:
          {
            bool success = OICan::SaveDeviceName(cmd.data.setDeviceName.serial, cmd.data.setDeviceName.name, cmd.data.setDeviceName.nodeId);
            CANEvent evt;
            evt.type = EVT_DEVICE_NAME_SET;
            evt.data.deviceNameSet.success = success;
            strcpy(evt.data.deviceNameSet.serial, cmd.data.setDeviceName.serial);
            strcpy(evt.data.deviceNameSet.name, cmd.data.setDeviceName.name);
            xQueueSend(canEventQueue, &evt, 0);
          }
          break;

        case CMD_START_SPOT_VALUES:
          // Handle start spot values
          spotValuesActive = true;
          spotValuesInterval = cmd.data.spotValues.interval;
          spotValuesParamIds.clear();
          for(int i = 0; i < cmd.data.spotValues.paramCount; i++) {
            spotValuesParamIds.push_back(cmd.data.spotValues.paramIds[i]);
          }

          spotValuesTicker.attach_ms(spotValuesInterval, broadcastSpotValues);

          {
            CANEvent evt;
            evt.type = EVT_SPOT_VALUES_STATUS;
            evt.data.spotValuesStatus.active = true;
            evt.data.spotValuesStatus.interval = spotValuesInterval;
            evt.data.spotValuesStatus.paramCount = spotValuesParamIds.size();
            xQueueSend(canEventQueue, &evt, 0);
          }
          break;

        case CMD_STOP_SPOT_VALUES:
          spotValuesActive = false;
          spotValuesTicker.detach();
          spotValuesParamIds.clear();

          {
            CANEvent evt;
            evt.type = EVT_SPOT_VALUES_STATUS;
            evt.data.spotValuesStatus.active = false;
            xQueueSend(canEventQueue, &evt, 0);
          }
          break;
      }
    }

    // Run CAN processing loop
    OICan::Loop();

    // Small delay to prevent task starvation
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}

void setup(void){
  DBG_OUTPUT_PORT.begin(115200);
  //Inverter.setRxBufferSize(50000);
  //Inverter.begin(115200, SERIAL_8N1, INVERTER_RX, INVERTER_TX);
  //Need to use low level Espressif IDF API instead of Serial to get high enough data rates
  // uart_config_t uart_config = {
  //       .baud_rate = 115200,
  //       .data_bits = UART_DATA_8_BITS,
  //       .parity    = UART_PARITY_DISABLE,
  //       .stop_bits = UART_STOP_BITS_1,
  //       .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
  //       .rx_flow_ctrl_thresh = UART_HW_FLOWCTRL_DISABLE,
  //       .source_clk = UART_SCLK_APB};

  // uart_param_config(INVERTER_PORT, &uart_config);
  // uart_set_pin(INVERTER_PORT, INVERTER_TX, INVERTER_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
  // uart_driver_install(INVERTER_PORT, 8192, 0, 0, NULL, 0);
  // delay(100);

  // Initialize status LED (NeoPixel)
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
    strncpy(evt.data.deviceDiscovered.serial, serial, sizeof(evt.data.deviceDiscovered.serial) - 1);
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
            strncpy(evt.data.deviceDiscovered.name, name, sizeof(evt.data.deviceDiscovered.name) - 1);
          }
        }
      }
    }

    xQueueSend(canEventQueue, &evt, 0);
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
    String json = OICan::GetRawJson();
    request->send(200, "application/json", json);
  });

  // Serve files - catch all handler
  server.onNotFound(handleFileRequest);

  server.begin();

  MDNS.addService("http", "tcp", 80);
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

      case EVT_DEVICE_NAME_SET:
        doc["event"] = "deviceNameSet";
        data["success"] = evt.data.deviceNameSet.success;
        data["serial"] = evt.data.deviceNameSet.serial;
        data["name"] = evt.data.deviceNameSet.name;
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

  // NOTE: OICan::Loop() now runs in CAN task, don't call here
}
