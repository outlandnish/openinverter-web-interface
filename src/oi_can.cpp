/*
 * This file is part of the esp32 web interface
 *
 * Copyright (C) 2023 Johannes Huebner <dev@johanneshuebner.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */
#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_task_wdt.h"
#include <FS.h>
#include <LittleFS.h>
#include <StreamUtils.h>
#include <ArduinoJson.h>
#include <map>
#include <vector>
#include "oi_can.h"
#include "models/can_types.h"
#include "utils/can_utils.h"
#include "managers/device_storage.h"

#define DBG_OUTPUT_PORT Serial
#define SDO_REQUEST_DOWNLOAD  (1 << 5)
#define SDO_REQUEST_UPLOAD    (2 << 5)
#define SDO_REQUEST_SEGMENT   (3 << 5)
#define SDO_TOGGLE_BIT        (1 << 4)
#define SDO_RESPONSE_UPLOAD   (2 << 5)
#define SDO_RESPONSE_DOWNLOAD (3 << 5)
#define SDO_EXPEDITED         (1 << 1)
#define SDO_SIZE_SPECIFIED    (1)
#define SDO_WRITE             (SDO_REQUEST_DOWNLOAD | SDO_EXPEDITED | SDO_SIZE_SPECIFIED)
#define SDO_READ              SDO_REQUEST_UPLOAD
#define SDO_ABORT             0x80
#define SDO_WRITE_REPLY       SDO_RESPONSE_DOWNLOAD
#define SDO_READ_REPLY        (SDO_RESPONSE_UPLOAD | SDO_EXPEDITED | SDO_SIZE_SPECIFIED)
#define SDO_ERR_INVIDX        0x06020000
#define SDO_ERR_RANGE         0x06090030
#define SDO_ERR_GENERAL       0x08000000

#define SDO_INDEX_PARAMS      0x2000
#define SDO_INDEX_PARAM_UID   0x2100
#define SDO_INDEX_MAP_TX      0x3000
#define SDO_INDEX_MAP_RX      0x3001
#define SDO_INDEX_MAP_RD      0x3100
#define SDO_INDEX_SERIAL      0x5000
#define SDO_INDEX_STRINGS     0x5001
#define SDO_INDEX_COMMANDS    0x5002
#define SDO_INDEX_ERROR_NUM   0x5003
#define SDO_INDEX_ERROR_TIME  0x5004
#define SDO_CMD_SAVE          0
#define SDO_CMD_LOAD          1
#define SDO_CMD_RESET         2
#define SDO_CMD_DEFAULTS      3
#define SDO_CMD_START         4
#define SDO_CMD_STOP          5

namespace OICan {

enum State { IDLE, ERROR, OBTAINSERIAL, OBTAIN_JSON };
enum UpdState { UPD_IDLE, SEND_MAGIC, SEND_SIZE, SEND_PAGE, CHECK_CRC, REQUEST_JSON };

static uint8_t _nodeId;
static BaudRate baudRate;
static State state;
static UpdState updstate;
static int canTxPin = -1;
static int canRxPin = -1;
static uint32_t serial[4]; //contains id sum as well
static char jsonFileName[20];
static twai_message_t tx_frame;
static File updateFile;
static int currentFlashPage = 0;
static const size_t PAGE_SIZE_BYTES = 1024;
static int retries = 0;
static JsonDocument cachedParamJson; // In-memory parameter JSON storage
static String jsonReceiveBuffer;      // Buffer for receiving JSON segments
static int jsonTotalSize = 0;  // Total size of JSON being downloaded (from SDO response)

// Continuous scanning state
static bool continuousScanActive = false;
static uint8_t scanStartNode = 1;
static uint8_t scanEndNode = 32;
static uint8_t currentScanNode = 1;
static uint8_t currentSerialPartIndex = 0; // Which part of serial we're reading (0-3)
static uint32_t scanDeviceSerial[4];
static unsigned long lastScanTime = 0;

// State timeout tracking
static unsigned long stateStartTime = 0;
static const unsigned long OBTAINSERIAL_TIMEOUT_MS = 5000; // 5 second timeout
static const unsigned long SCAN_DELAY_MS = 50; // Delay between node probes
static const unsigned long SCAN_TIMEOUT_MS = 100; // Timeout for scan response
static DeviceDiscoveryCallback discoveryCallback = nullptr;
static ScanProgressCallback scanProgressCallback = nullptr;
static ConnectionReadyCallback connectionReadyCallback = nullptr;
static JsonDownloadProgressCallback jsonProgressCallback = nullptr;
static JsonStreamCallback jsonStreamCallback = nullptr;

// In-memory device list
struct Device {
  String serial;
  uint8_t nodeId;
  String name;
  uint32_t lastSeen;
};
static std::map<String, Device> deviceList; // Key: serial number

// Helper functions for CAN response validation
static bool isValidSerialResponse(const twai_message_t& frame, uint8_t nodeId, uint8_t partIndex) {
  uint16_t rxIndex = (frame.data[1] | (frame.data[2] << 8));
  return frame.identifier == (SDO_RESPONSE_BASE_ID | nodeId) &&
         frame.data[0] != SDO_ABORT &&
         rxIndex == SDO_INDEX_SERIAL &&
         frame.data[3] == partIndex;
}

static void advanceScanNode() {
  currentSerialPartIndex = 0;
  currentScanNode++;
  if (currentScanNode > scanEndNode) {
    currentScanNode = scanStartNode; // Wrap around to start
  }
}

static bool shouldProcessScan(unsigned long currentTime) {
  return continuousScanActive &&
         state == IDLE &&
         (currentTime - lastScanTime >= SCAN_DELAY_MS);
}

static bool handleScanResponse(const twai_message_t& frame, unsigned long currentTime) {
  if (!isValidSerialResponse(frame, currentScanNode, currentSerialPartIndex)) {
    return false;
  }

  scanDeviceSerial[currentSerialPartIndex] = *(uint32_t*)&frame.data[4];
  currentSerialPartIndex++;

  // If we've read all 4 parts, we found a device
  if (currentSerialPartIndex >= 4) {
    char serialStr[40];
    sprintf(serialStr, "%08" PRIX32 ":%08" PRIX32 ":%08" PRIX32 ":%08" PRIX32,
            scanDeviceSerial[0], scanDeviceSerial[1], scanDeviceSerial[2], scanDeviceSerial[3]);

    DBG_OUTPUT_PORT.printf("Continuous scan found device at node %d: %s\n", currentScanNode, serialStr);

    // Update in-memory device list (not saved to file until user names it)
    AddOrUpdateDevice(serialStr, currentScanNode, nullptr, currentTime);

    // Notify via callback (will broadcast to WebSocket clients)
    if (discoveryCallback) {
      discoveryCallback(currentScanNode, serialStr, currentTime);
    }

    // Move to next node
    advanceScanNode();
  }

  return true;
}

// CAN debug helpers
static void printCanTx(const twai_message_t* frame) {
  // Debug output disabled
}

static void printCanRx(const twai_message_t* frame) {
  // Debug output disabled
}

static void requestSdoElement(uint8_t nodeId, uint16_t index, uint8_t subIndex) {
  tx_frame.extd = false;
  tx_frame.identifier = SDO_REQUEST_BASE_ID | nodeId;
  tx_frame.data_length_code = 8;
  tx_frame.data[0] = SDO_READ;
  tx_frame.data[1] = index & 0xFF;
  tx_frame.data[2] = index >> 8;
  tx_frame.data[3] = subIndex;
  tx_frame.data[4] = 0;
  tx_frame.data[5] = 0;
  tx_frame.data[6] = 0;
  tx_frame.data[7] = 0;

  twai_transmit(&tx_frame, pdMS_TO_TICKS(10));
  printCanTx(&tx_frame);
}

// Non-blocking version for parameter requests (doesn't block if TX queue is full)
static bool requestSdoElementNonBlocking(uint8_t nodeId, uint16_t index, uint8_t subIndex) {
  tx_frame.extd = false;
  tx_frame.identifier = SDO_REQUEST_BASE_ID | nodeId;
  tx_frame.data_length_code = 8;
  tx_frame.data[0] = SDO_READ;
  tx_frame.data[1] = index & 0xFF;
  tx_frame.data[2] = index >> 8;
  tx_frame.data[3] = subIndex;
  tx_frame.data[4] = 0;
  tx_frame.data[5] = 0;
  tx_frame.data[6] = 0;
  tx_frame.data[7] = 0;

  // Non-blocking transmit (timeout = 0)
  esp_err_t result = twai_transmit(&tx_frame, 0);
  if (result == ESP_OK) {
    printCanTx(&tx_frame);
    return true;
  }
  return false; // TX queue full, message not sent
}

// Rate limiting for parameter requests
static unsigned long lastParamRequestTime = 0;
static unsigned long minParamRequestIntervalUs = 500; // Default: 500 microseconds between requests

// Send SDO request for a parameter value (truly non-blocking with rate limiting)
bool RequestValue(int paramId) {
  // Rate limiting: check if enough time has passed since last request
  unsigned long currentTime = micros();
  unsigned long timeSinceLastRequest = currentTime - lastParamRequestTime;

  if (timeSinceLastRequest < minParamRequestIntervalUs) {
    // Too soon - return false without blocking
    return false;
  }

  uint16_t index = SDO_INDEX_PARAM_UID | (paramId >> 8);
  uint8_t subIndex = paramId & 0xFF;

  bool success = requestSdoElementNonBlocking(_nodeId, index, subIndex);

  if (success) {
    lastParamRequestTime = micros();
  }

  return success;
}

// Configure rate limiting for parameter requests
void SetParameterRequestRateLimit(unsigned long intervalUs) {
  minParamRequestIntervalUs = intervalUs;
  DBG_OUTPUT_PORT.printf("Parameter request rate limit set to %lu microseconds\n", intervalUs);
}

static void setValueSdo(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t value) {
  tx_frame.extd = false;
  tx_frame.identifier = SDO_REQUEST_BASE_ID | nodeId;
  tx_frame.data_length_code = 8;
  tx_frame.data[0] = SDO_WRITE;
  tx_frame.data[1] = index & 0xFF;
  tx_frame.data[2] = index >> 8;
  tx_frame.data[3] = subIndex;
  *(uint32_t*)&tx_frame.data[4] = value;

  twai_transmit(&tx_frame, pdMS_TO_TICKS(10));
  printCanTx(&tx_frame);
}

// getId() removed - web client now sends parameter IDs directly instead of names

static void requestNextSegment(bool toggleBit) {
  tx_frame.extd = false;
  tx_frame.identifier = SDO_REQUEST_BASE_ID | _nodeId;
  tx_frame.data_length_code = 8;
  tx_frame.data[0] = SDO_REQUEST_SEGMENT | toggleBit << 4;
  tx_frame.data[1] = 0;
  tx_frame.data[2] = 0;
  tx_frame.data[3] = 0;
  tx_frame.data[4] = 0;
  tx_frame.data[5] = 0;
  tx_frame.data[6] = 0;
  tx_frame.data[7] = 0;

  twai_transmit(&tx_frame, pdMS_TO_TICKS(10));
  printCanTx(&tx_frame);
}

static void handleSdoResponse(twai_message_t *rxframe) {
  static bool toggleBit = false;
  static File file;

  if (rxframe->data[0] == SDO_ABORT) { //SDO abort
    state = ERROR;
    DBG_OUTPUT_PORT.println("Error obtaining serial number, try restarting");
    return;
  }

  switch (state) {
    case OBTAINSERIAL:
      if ((rxframe->data[1] | rxframe->data[2] << 8) == SDO_INDEX_SERIAL && rxframe->data[3] < 4) {
        serial[rxframe->data[3]] = *(uint32_t*)&rxframe->data[4];

        if (rxframe->data[3] < 3) {
          requestSdoElement(_nodeId, SDO_INDEX_SERIAL, rxframe->data[3] + 1);
        }
        else {
          sprintf(jsonFileName, "/%" PRIx32 ".json", serial[3]);
          DBG_OUTPUT_PORT.printf("Got Serial Number %" PRIX32 ":%" PRIX32 ":%" PRIX32 ":%" PRIX32 "\r\n", serial[0], serial[1], serial[2], serial[3]);

          // Go to IDLE - JSON will be downloaded on-demand when browser requests it
          state = IDLE;
          DBG_OUTPUT_PORT.println("Connection established. Parameter JSON available on request.");

          // Notify that connection is ready (device is in IDLE state)
          if (connectionReadyCallback) {
            char serialStr[64];
            sprintf(serialStr, "%" PRIX32 ":%" PRIX32 ":%" PRIX32 ":%" PRIX32, serial[0], serial[1], serial[2], serial[3]);
            connectionReadyCallback(_nodeId, serialStr);
          }
        }
      }
      break;
    case OBTAIN_JSON:
      //Receiving last segment
      if ((rxframe->data[0] & SDO_SIZE_SPECIFIED) && (rxframe->data[0] & SDO_READ) == 0) {
        DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Receiving last segment");
        int size = 7 - ((rxframe->data[0] >> 1) & 0x7);
        for (int i = 0; i < size; i++) {
          jsonReceiveBuffer += (char)rxframe->data[1 + i];
        }

        DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Download complete");
        DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] JSON size: %d bytes\r\n", jsonReceiveBuffer.length());

        // Parse JSON into cachedParamJson for future use
        DeserializationError error = deserializeJson(cachedParamJson, jsonReceiveBuffer);
        if (error) {
          DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] Parse error: %s\r\n", error.c_str());
        } else {
          DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Parsed successfully");
        }

        state = IDLE;
      }
      //Receiving a segment
      else if (rxframe->data[0] == (toggleBit << 4) && (rxframe->data[0] & SDO_READ) == 0) {
        DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] Segment received (buffer: %d bytes)\n", jsonReceiveBuffer.length());
        for (int i = 0; i < 7; i++) {
          jsonReceiveBuffer += (char)rxframe->data[1 + i];
        }
        toggleBit = !toggleBit;
        requestNextSegment(toggleBit);
      }
      //Request first segment (initiate upload response)
      else if ((rxframe->data[0] & SDO_READ) == SDO_READ) {
        DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Initiate upload response received");

        // Check if size is specified in the response (CANopen SDO protocol)
        if (rxframe->data[0] & SDO_SIZE_SPECIFIED) {
          // Extract total size from bytes 4-7 (little-endian)
          jsonTotalSize = *(uint32_t*)&rxframe->data[4];
          DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] Total size indicated: %d bytes\r\n", jsonTotalSize);

          // Send initial progress update (0 bytes received, but total is known)
          if (jsonProgressCallback) {
            jsonProgressCallback(0); // Will include totalBytes in the message via GetJsonTotalSize()
          }
        } else {
          jsonTotalSize = 0; // Unknown size
          DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Total size not specified by device");
        }

        DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Requesting first segment");
        requestNextSegment(toggleBit);
      }

      break;
    case ERROR:
      // Do not exit this state
      break;
    case IDLE:
      // Do not exit this state
      break;
  }
}

static void handleUpdate(twai_message_t *rxframe) {
  static int currentByte = 0;
  static uint32_t crc;

  switch (updstate) {
    case SEND_MAGIC:
      if (rxframe->data[0] == 0x33) {
        tx_frame.identifier = BOOTLOADER_COMMAND_ID;
        tx_frame.data_length_code = 4;

        //For now just reflect ID
        tx_frame.data[0] = rxframe->data[4];
        tx_frame.data[1] = rxframe->data[5];
        tx_frame.data[2] = rxframe->data[6];
        tx_frame.data[3] = rxframe->data[7];
        updstate = SEND_SIZE;
        DBG_OUTPUT_PORT.printf("Sending ID %" PRIu32 "\r\n", *(uint32_t*)tx_frame.data);
        twai_transmit(&tx_frame, pdMS_TO_TICKS(10));
        printCanTx(&tx_frame);

        if (rxframe->data[1] < 1) //boot loader with timing quirk, wait 100 ms
          delay(100);
      }
      break;
    case SEND_SIZE:
      if (rxframe->data[0] == 'S') {
        tx_frame.identifier = BOOTLOADER_COMMAND_ID;
        tx_frame.data_length_code = 1;

        tx_frame.data[0] = (updateFile.size() + PAGE_SIZE_BYTES - 1) / PAGE_SIZE_BYTES;
        updstate = SEND_PAGE;
        crc = 0xFFFFFFFF;
        currentByte = 0;
        currentFlashPage = 0;
        DBG_OUTPUT_PORT.printf("Sending size %u\r\n", tx_frame.data[0]);
        twai_transmit(&tx_frame, pdMS_TO_TICKS(10));
        printCanTx(&tx_frame);
      }
      break;
    case SEND_PAGE:
      if (rxframe->data[0] == 'P') {
        char buffer[8];
        size_t bytesRead = 0;

        if (currentByte < updateFile.size()) {
          updateFile.seek(currentByte);
          bytesRead = updateFile.readBytes(buffer, sizeof(buffer));
        }

        while (bytesRead < 8)
          buffer[bytesRead++] = 0xff;

        currentByte += bytesRead;
        crc = crc32_word(crc, *(uint32_t*)&buffer[0]);
        crc = crc32_word(crc, *(uint32_t*)&buffer[4]);

        tx_frame.identifier = BOOTLOADER_COMMAND_ID;
        tx_frame.data_length_code = 8;
        tx_frame.data[0] = buffer[0];
        tx_frame.data[1] = buffer[1];
        tx_frame.data[2] = buffer[2];
        tx_frame.data[3] = buffer[3];
        tx_frame.data[4] = buffer[4];
        tx_frame.data[5] = buffer[5];
        tx_frame.data[6] = buffer[6];
        tx_frame.data[7] = buffer[7];

        updstate = SEND_PAGE;
        twai_transmit(&tx_frame, pdMS_TO_TICKS(10));
        printCanTx(&tx_frame);
      }
      else if (rxframe->data[0] == 'C') {
        tx_frame.identifier = BOOTLOADER_COMMAND_ID;
        tx_frame.data_length_code = 4;
        tx_frame.data[0] = crc & 0xFF;
        tx_frame.data[1] = (crc >> 8) & 0xFF;
        tx_frame.data[2] = (crc >> 16) & 0xFF;
        tx_frame.data[3] = (crc >> 24) & 0xFF;

        updstate = CHECK_CRC;
        twai_transmit(&tx_frame, pdMS_TO_TICKS(10));
        printCanTx(&tx_frame);
      }
      break;
    case CHECK_CRC:
      crc = 0xFFFFFFFF;
      DBG_OUTPUT_PORT.printf("Sent bytes %u-%u... ", currentFlashPage * PAGE_SIZE_BYTES, currentByte);
      if (rxframe->data[0] == 'P') {
        updstate = SEND_PAGE;
        currentFlashPage++;
        DBG_OUTPUT_PORT.printf("CRC Good\r\n");
        handleUpdate(rxframe);
      }
      else if (rxframe->data[0] == 'E') {
        updstate = SEND_PAGE;
        currentByte = currentFlashPage * PAGE_SIZE_BYTES;
        DBG_OUTPUT_PORT.printf("CRC Error\r\n");
        handleUpdate(rxframe);
      }
      else if (rxframe->data[0] == 'D') {
        updstate = REQUEST_JSON;
        state = OBTAINSERIAL;
        retries = 50;
        updateFile.close();
        DBG_OUTPUT_PORT.printf("Done!\r\n");
      }
      break;
    case REQUEST_JSON:
      // Do not exit this state
      break;
    case UPD_IDLE:
      // Do not exit this state
      break;
  }
}

int StartUpdate(String fileName) {
  updateFile = LittleFS.open(fileName, "r");
  currentFlashPage = 0;

  // Set state BEFORE reset so we catch the bootloader's magic response
  updstate = SEND_MAGIC;

  //Reset host processor
  setValueSdo(_nodeId, SDO_INDEX_COMMANDS, SDO_CMD_RESET, 1U);

  // Give device time to reset and enter bootloader mode
  // The bootloader needs time to boot and start sending magic
  DBG_OUTPUT_PORT.println("Waiting for device to enter bootloader mode...");
  DBG_OUTPUT_PORT.println("Starting Update");
  delay(500);

  return (updateFile.size() + PAGE_SIZE_BYTES - 1) / PAGE_SIZE_BYTES;
}

int GetCurrentUpdatePage() {
  return currentFlashPage;
}

bool IsUpdateInProgress() {
  return updstate != UPD_IDLE;
}

String GetRawJson() {
  // Return cached JSON if available (avoid blocking download)
  if (!jsonReceiveBuffer.isEmpty()) {
    DBG_OUTPUT_PORT.printf("[GetRawJson] Returning cached JSON (%d bytes)\n", jsonReceiveBuffer.length());
    return jsonReceiveBuffer;
  }

  if (state != IDLE) {
    DBG_OUTPUT_PORT.printf("[GetRawJson] Cannot get JSON - device busy (state=%d)\n", state);
    return "{}";
  }

  // Trigger JSON download from device
  DBG_OUTPUT_PORT.printf("[GetRawJson] Starting JSON download from node %d\n", _nodeId);
  state = OBTAIN_JSON;
  jsonReceiveBuffer = ""; // Clear buffer
  cachedParamJson.clear(); // Clear parsed JSON
  jsonTotalSize = 0; // Reset total size (will be updated from SDO response if available)
  requestSdoElement(_nodeId, SDO_INDEX_STRINGS, 0);
  DBG_OUTPUT_PORT.println("[GetRawJson] Sent SDO request, waiting for response...");

  // Wait for download to complete (with per-segment timeout)
  // Timeout only if no segment received within timeout period (not total time)
  unsigned long lastSegmentTime = millis();
  unsigned long lastProgressUpdate = 0;
  int lastBufferSize = 0;
  int lastStreamedSize = 0; // Track how much we've sent via streaming callback
  int loopCount = 0;
  const unsigned long SEGMENT_TIMEOUT_MS = 5000; // 5 second timeout per segment
  const unsigned long PROGRESS_UPDATE_INTERVAL_MS = 200; // Throttle progress updates to 200ms

  while (state == OBTAIN_JSON) {
    Loop(); // Process CAN messages

    // Check if we received a new segment (buffer grew)
    if (jsonReceiveBuffer.length() > lastBufferSize) {
      int newDataSize = jsonReceiveBuffer.length() - lastBufferSize;
      lastBufferSize = jsonReceiveBuffer.length();
      lastSegmentTime = millis(); // Reset timeout on new data

      // Stream new data chunk to callback if registered
      if (jsonStreamCallback && jsonReceiveBuffer.length() > lastStreamedSize) {
        int chunkSize = jsonReceiveBuffer.length() - lastStreamedSize;
        const char* chunkStart = jsonReceiveBuffer.c_str() + lastStreamedSize;
        jsonStreamCallback(chunkStart, chunkSize, false); // isComplete = false
        lastStreamedSize = jsonReceiveBuffer.length();
      }

      // Send progress update via callback (throttled)
      if (jsonProgressCallback && (millis() - lastProgressUpdate > PROGRESS_UPDATE_INTERVAL_MS)) {
        jsonProgressCallback(jsonReceiveBuffer.length());
        lastProgressUpdate = millis();
      }

      if (loopCount % 100 == 0) {
        DBG_OUTPUT_PORT.printf("[GetRawJson] Progress: buffer size: %d bytes\n", jsonReceiveBuffer.length());
      }
    }

    // Check for segment timeout (no data received for too long)
    if ((millis() - lastSegmentTime) > SEGMENT_TIMEOUT_MS) {
      DBG_OUTPUT_PORT.printf("[GetRawJson] Segment timeout! No data for %lu ms, buffer size=%d\n",
        millis() - lastSegmentTime, jsonReceiveBuffer.length());
      state = IDLE;
      return "{}";
    }

    // Yield to other tasks (especially async_tcp) to prevent watchdog timeout
    delay(10);

    // Feed the watchdog to prevent timeout during long transfers
    esp_task_wdt_reset();

    loopCount++;
  }

  if (state != IDLE) {
    DBG_OUTPUT_PORT.printf("[GetRawJson] Failed! State=%d, buffer size=%d\n", state, jsonReceiveBuffer.length());
    state = IDLE;
    return "{}";
  }

  DBG_OUTPUT_PORT.printf("[GetRawJson] Download complete! Buffer size: %d bytes\n", jsonReceiveBuffer.length());

  // Send final chunk with completion flag if streaming
  if (jsonStreamCallback && jsonReceiveBuffer.length() > lastStreamedSize) {
    int chunkSize = jsonReceiveBuffer.length() - lastStreamedSize;
    const char* chunkStart = jsonReceiveBuffer.c_str() + lastStreamedSize;
    jsonStreamCallback(chunkStart, chunkSize, true); // isComplete = true
  } else if (jsonStreamCallback) {
    // No remaining data, but signal completion
    jsonStreamCallback("", 0, true);
  }

  // Send completion notification (0 = done)
  if (jsonProgressCallback) {
    jsonProgressCallback(0);
  }

  return jsonReceiveBuffer;
}

// Overloaded version that fetches JSON from a specific device by nodeId
String GetRawJson(uint8_t nodeId) {
  if (state != IDLE) {
    DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Cannot get JSON - device busy (state=%d)\n", state);
    return "{}";
  }

  // If we're already connected to the requested node, just use the regular GetRawJson
  if (_nodeId == nodeId && !jsonReceiveBuffer.isEmpty()) {
    DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Already connected to node %d, using cached JSON\n", nodeId);
    return GetRawJson();
  }

  // If we're connected to the requested node but cache is empty, fetch without switching
  if (_nodeId == nodeId) {
    DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Already connected to node %d, fetching fresh JSON\n", nodeId);
    jsonReceiveBuffer = "";
    cachedParamJson.clear();
    
    // Trigger JSON download
    state = OBTAIN_JSON;
    requestSdoElement(_nodeId, SDO_INDEX_STRINGS, 0);
    DBG_OUTPUT_PORT.println("[GetRawJson(nodeId)] Sent SDO request, waiting for response...");

    unsigned long lastSegmentTime = millis();
    unsigned long lastProgressUpdate = 0;
    int lastBufferSize = 0;
    int lastStreamedSize = 0;
    int loopCount = 0;
    const unsigned long SEGMENT_TIMEOUT_MS = 5000;
    const unsigned long PROGRESS_UPDATE_INTERVAL_MS = 200;

    while (state == OBTAIN_JSON) {
      Loop();

      if (jsonReceiveBuffer.length() > lastBufferSize) {
        lastBufferSize = jsonReceiveBuffer.length();
        lastSegmentTime = millis();

        // Stream new data chunk to callback if registered
        if (jsonStreamCallback && jsonReceiveBuffer.length() > lastStreamedSize) {
          int chunkSize = jsonReceiveBuffer.length() - lastStreamedSize;
          const char* chunkStart = jsonReceiveBuffer.c_str() + lastStreamedSize;
          jsonStreamCallback(chunkStart, chunkSize, false);
          lastStreamedSize = jsonReceiveBuffer.length();
        }

        // Send progress update via callback (throttled)
        if (jsonProgressCallback && (millis() - lastProgressUpdate > PROGRESS_UPDATE_INTERVAL_MS)) {
          jsonProgressCallback(jsonReceiveBuffer.length());
          lastProgressUpdate = millis();
        }

        if (loopCount % 100 == 0) {
          DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Progress: buffer size: %d bytes\n", jsonReceiveBuffer.length());
        }
      }

      if ((millis() - lastSegmentTime) > SEGMENT_TIMEOUT_MS) {
        DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Segment timeout! No data for %lu ms\n", millis() - lastSegmentTime);
        state = IDLE;
        return "{}";
      }

      delay(10);
      esp_task_wdt_reset();
      loopCount++;
    }

    if (state != IDLE) {
      DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Failed! State=%d\n", state);
      state = IDLE;
      return "{}";
    }

    DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Download complete! Buffer size: %d bytes\n", jsonReceiveBuffer.length());

    // Send final chunk with completion flag if streaming
    if (jsonStreamCallback && jsonReceiveBuffer.length() > lastStreamedSize) {
      int chunkSize = jsonReceiveBuffer.length() - lastStreamedSize;
      const char* chunkStart = jsonReceiveBuffer.c_str() + lastStreamedSize;
      jsonStreamCallback(chunkStart, chunkSize, true);
    } else if (jsonStreamCallback) {
      jsonStreamCallback("", 0, true);
    }

    // Send completion notification (0 = done)
    if (jsonProgressCallback) {
      jsonProgressCallback(0);
    }

    return jsonReceiveBuffer;
  }

  // Save current state for switching to different node
  uint8_t savedNodeId = _nodeId;
  String savedJsonBuffer = jsonReceiveBuffer;

  // Switch to target device and clear cache
  DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Switching from node %d to node %d\n", _nodeId, nodeId);
  _nodeId = nodeId;
  jsonReceiveBuffer = "";
  cachedParamJson.clear();

  // Trigger JSON download from target device
  DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Starting JSON download from node %d\n", _nodeId);
  state = OBTAIN_JSON;
  requestSdoElement(_nodeId, SDO_INDEX_STRINGS, 0);
  DBG_OUTPUT_PORT.println("[GetRawJson(nodeId)] Sent SDO request, waiting for response...");

  // Wait for download to complete (with per-segment timeout)
  unsigned long lastSegmentTime = millis();
  unsigned long lastProgressUpdate = 0;
  int lastBufferSize = 0;
  int lastStreamedSize = 0;
  int loopCount = 0;
  const unsigned long SEGMENT_TIMEOUT_MS = 5000;
  const unsigned long PROGRESS_UPDATE_INTERVAL_MS = 200;

  while (state == OBTAIN_JSON) {
    Loop();

    if (jsonReceiveBuffer.length() > lastBufferSize) {
      lastBufferSize = jsonReceiveBuffer.length();
      lastSegmentTime = millis();

      // Stream new data chunk to callback if registered
      if (jsonStreamCallback && jsonReceiveBuffer.length() > lastStreamedSize) {
        int chunkSize = jsonReceiveBuffer.length() - lastStreamedSize;
        const char* chunkStart = jsonReceiveBuffer.c_str() + lastStreamedSize;
        jsonStreamCallback(chunkStart, chunkSize, false);
        lastStreamedSize = jsonReceiveBuffer.length();
      }

      // Send progress update via callback (throttled)
      if (jsonProgressCallback && (millis() - lastProgressUpdate > PROGRESS_UPDATE_INTERVAL_MS)) {
        jsonProgressCallback(jsonReceiveBuffer.length());
        lastProgressUpdate = millis();
      }

      if (loopCount % 100 == 0) {
        DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Progress: buffer size: %d bytes\n", jsonReceiveBuffer.length());
      }
    }

    if ((millis() - lastSegmentTime) > SEGMENT_TIMEOUT_MS) {
      DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Segment timeout! No data for %lu ms\n", millis() - lastSegmentTime);
      state = IDLE;
      // Restore previous state
      _nodeId = savedNodeId;
      jsonReceiveBuffer = savedJsonBuffer;
      return "{}";
    }

    delay(10);
    esp_task_wdt_reset();
    loopCount++;
  }

  if (state != IDLE) {
    DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Failed! State=%d\n", state);
    state = IDLE;
    // Restore previous state
    _nodeId = savedNodeId;
    jsonReceiveBuffer = savedJsonBuffer;
    return "{}";
  }

  DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Download complete! Buffer size: %d bytes\n", jsonReceiveBuffer.length());
  String result = jsonReceiveBuffer;

  // Send final chunk with completion flag if streaming
  if (jsonStreamCallback && jsonReceiveBuffer.length() > lastStreamedSize) {
    int chunkSize = jsonReceiveBuffer.length() - lastStreamedSize;
    const char* chunkStart = jsonReceiveBuffer.c_str() + lastStreamedSize;
    jsonStreamCallback(chunkStart, chunkSize, true);
  } else if (jsonStreamCallback) {
    jsonStreamCallback("", 0, true);
  }

  // Send completion notification (0 = done)
  if (jsonProgressCallback) {
    jsonProgressCallback(0);
  }

  // Restore previous state
  _nodeId = savedNodeId;
  jsonReceiveBuffer = savedJsonBuffer;
  DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Restored to node %d\n", _nodeId);

  return result;
}

bool SendJson(WiFiClient client) {
  if (state != IDLE) return false;

  JsonDocument doc;
  twai_message_t rxframe;

  // Use in-memory JSON if available
  if (cachedParamJson.isNull() || cachedParamJson.size() == 0) {
    DBG_OUTPUT_PORT.println("No parameter JSON in memory");
    return false;
  }

  JsonObject root = cachedParamJson.as<JsonObject>();
  int failed = 0;

  for (JsonPair kv : root) {
    int id = kv.value()["id"].as<int>();

    if (id > 0) {
      requestSdoElement(_nodeId, SDO_INDEX_PARAM_UID | (id >> 8), id & 0xff);

      if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK && rxframe.data[3] == (id & 0xFF)) {
        printCanRx(&rxframe);
        kv.value()["value"] = ((double)*(int32_t*)&rxframe.data[4]) / 32;
      } else {
        failed++;
      }
    }
  }
  if (failed < 5) {
    WriteBufferingStream bufferedWifiClient{client, 1000};
    serializeJson(doc, bufferedWifiClient);
  }
  return failed < 5;
}

String GetCanMapping() {
  if (state != IDLE) {
    DBG_OUTPUT_PORT.println("GetCanMapping called while not IDLE, ignoring");
    return "[]";
  }

  enum ReqMapStt { START, COBID, DATAPOSLEN, GAINOFS, DONE };

  twai_message_t rxframe;
  int index = SDO_INDEX_MAP_RD, subIndex = 0;
  int cobid = 0, pos = 0, len = 0, paramid = 0;
  bool rx = false;
  ReqMapStt reqMapStt = START;

  JsonDocument doc;

  while (DONE != reqMapStt) {
    switch (reqMapStt) {
    case START:
      requestSdoElement(_nodeId, index, 0); //request COB ID
      reqMapStt = COBID;
      cobid = 0;
      pos = 0;
      len = 0;
      paramid = 0;
      break;
    case COBID:
      if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
        if (rxframe.data[0] != SDO_ABORT) {
          cobid = *(int32_t*)&rxframe.data[4]; //convert bytes to word
          subIndex++;
          requestSdoElement(_nodeId, index, subIndex); //request parameter id, position and length
          reqMapStt = DATAPOSLEN;
        }
        else if (!rx) { //after receiving tx item collect rx items
          rx = true;
          index = SDO_INDEX_MAP_RD + 0x80;
          reqMapStt = START;
          DBG_OUTPUT_PORT.println("Getting RX items");
        }
        else //no more items, we are done
          reqMapStt = DONE;
      }
      else
        reqMapStt = DONE; //don't lock up when not receiving
      break;
    case DATAPOSLEN:
      if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
        if (rxframe.data[0] != SDO_ABORT) {
          paramid = *(uint16_t*)&rxframe.data[4];
          pos = rxframe.data[6];
          len = (int8_t)rxframe.data[7];
          subIndex++;
          requestSdoElement(_nodeId, index, subIndex); //gain and offset
          reqMapStt = GAINOFS;
        }
        else { //all items of this message collected, move to next message
          index++;
          subIndex = 0;
          reqMapStt = START;
          DBG_OUTPUT_PORT.println("Mapping received, moving to next");
        }
      }
      else
        reqMapStt = DONE; //don't lock up when not receiving
      break;
    case GAINOFS:
      if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
        if (rxframe.data[0] != SDO_ABORT) {
          int32_t gainFixedPoint = ((*(uint32_t*)&rxframe.data[4]) & 0xFFFFFF) << (32-24);
          gainFixedPoint >>= (32-24);
          float gain = gainFixedPoint / 1000.0f;
          int offset = (int8_t)rxframe.data[7];
          DBG_OUTPUT_PORT.printf("can %s %d %d %d %d %f %d\r\n", rx ? "rx" : "tx", paramid, cobid, pos, len, gain, offset);
          JsonDocument subdoc;
          JsonObject object = subdoc.to<JsonObject>();
          object["isrx"] = rx;
          object["id"] = cobid;
          object["paramid"] = paramid;
          object["position"] = pos;
          object["length"] = len;
          object["gain"] = gain;
          object["offset"] = offset;
          object["index"] = index;
          object["subindex"] = subIndex;
          doc.add(object);
          subIndex++;

          if (subIndex < 100) { //limit maximum items in case there is a bug ;)
            requestSdoElement(_nodeId, index, subIndex); //request next item
            reqMapStt = DATAPOSLEN;
          }
          else {
            reqMapStt = DONE;
          }
        }
        else //should never get here
          reqMapStt = DONE;
      }
      else
        reqMapStt = DONE; //don't lock up when not receiving
      break;
    case DONE:
      break;
    }
  }

  String result;
  serializeJson(doc, result);
  return result;
}

void SendCanMapping(WiFiClient client) {
  if (state != IDLE) {
    DBG_OUTPUT_PORT.println("SendCanMapping called while not IDLE, ignoring");
    return;
  }

  enum ReqMapStt { START, COBID, DATAPOSLEN, GAINOFS, DONE };

  twai_message_t rxframe;
  int index = SDO_INDEX_MAP_RD, subIndex = 0;
  int cobid = 0, pos = 0, len = 0, paramid = 0;
  bool rx = false;
  String result;
  ReqMapStt reqMapStt = START;

  JsonDocument doc;

  while (DONE != reqMapStt) {
    switch (reqMapStt) {
    case START:
      requestSdoElement(_nodeId, index, 0); //request COB ID
      reqMapStt = COBID;
      cobid = 0;
      pos = 0;
      len = 0;
      paramid = 0;
      break;
    case COBID:
      if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
        if (rxframe.data[0] != SDO_ABORT) {
          cobid = *(int32_t*)&rxframe.data[4]; //convert bytes to word
          subIndex++;
          requestSdoElement(_nodeId, index, subIndex); //request parameter id, position and length
          reqMapStt = DATAPOSLEN;
        }
        else if (!rx) { //after receiving tx item collect rx items
          rx = true;
          index = SDO_INDEX_MAP_RD + 0x80;
          reqMapStt = START;
          DBG_OUTPUT_PORT.println("Getting RX items");
        }
        else //no more items, we are done
          reqMapStt = DONE;
      }
      else
        reqMapStt = DONE; //don't lock up when not receiving
      break;
    case DATAPOSLEN:
      if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
        if (rxframe.data[0] != SDO_ABORT) {
          paramid = *(uint16_t*)&rxframe.data[4];
          pos = rxframe.data[6];
          len = (int8_t)rxframe.data[7];
          subIndex++;
          requestSdoElement(_nodeId, index, subIndex); //gain and offset
          reqMapStt = GAINOFS;
        }
        else { //all items of this message collected, move to next message
          index++;
          subIndex = 0;
          reqMapStt = START;
          DBG_OUTPUT_PORT.println("Mapping received, moving to next");
        }
      }
      else
        reqMapStt = DONE; //don't lock up when not receiving
      break;
    case GAINOFS:
      if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
        if (rxframe.data[0] != SDO_ABORT) {
          int32_t gainFixedPoint = ((*(uint32_t*)&rxframe.data[4]) & 0xFFFFFF) << (32-24);
          gainFixedPoint >>= (32-24);
          float gain = gainFixedPoint / 1000.0f;
          int offset = (int8_t)rxframe.data[7];
          DBG_OUTPUT_PORT.printf("can %s %d %d %d %d %f %d\r\n", rx ? "rx" : "tx", paramid, cobid, pos, len, gain, offset);
          JsonDocument subdoc;
          JsonObject object = subdoc.to<JsonObject>();
          object["isrx"] = rx;
          object["id"] = cobid;
          object["paramid"] = paramid;
          object["position"] = pos;
          object["length"] = len;
          object["gain"] = gain;
          object["offset"] = offset;
          object["index"] = index;
          object["subindex"] = subIndex;
          doc.add(object);
          subIndex++;

          if (subIndex < 100) { //limit maximum items in case there is a bug ;)
            requestSdoElement(_nodeId, index, subIndex); //request next item
            reqMapStt = DATAPOSLEN;
          }
          else {
            reqMapStt = DONE;
          }
        }
        else //should never get here
          reqMapStt = DONE;
      }
      else
        reqMapStt = DONE; //don't lock up when not receiving
      break;
    case DONE:
      break;
    }
  }

  WriteBufferingStream bufferedWifiClient{client, 1000};
  serializeJson(doc, bufferedWifiClient);
}

SetResult AddCanMapping(String json) {
  if (state != IDLE) return CommError;

  JsonDocument doc;
  twai_message_t rxframe;

  deserializeJson(doc, json);

  if (doc["isrx"].isNull() || doc["id"].isNull() || doc["paramid"].isNull() || doc["position"].isNull() ||
      doc["length"].isNull() || doc["gain"].isNull() || doc["offset"].isNull()) {
    DBG_OUTPUT_PORT.println("Add: Missing argument");
    return UnknownIndex;
  }

  int index = doc["isrx"] ? SDO_INDEX_MAP_RX : SDO_INDEX_MAP_TX;

  setValueSdo(_nodeId, index, 0, (uint32_t)doc["id"]); //Send CAN Id

  if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
    printCanRx(&rxframe);
    DBG_OUTPUT_PORT.println("Sent COB Id");
    setValueSdo(_nodeId, index, 1, doc["paramid"].as<uint32_t>() | (doc["position"].as<uint32_t>() << 16) | (doc["length"].as<int32_t>() << 24)); //data item, position and length
    if (rxframe.data[0] != SDO_ABORT && twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
      printCanRx(&rxframe);
      DBG_OUTPUT_PORT.println("Sent position and length");
      setValueSdo(_nodeId, index, 2, (uint32_t)((int32_t)(doc["gain"].as<double>() * 1000) & 0xFFFFFF) | doc["offset"].as<int32_t>() << 24); //gain and offset

      if (rxframe.data[0] != SDO_ABORT && twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
        if (rxframe.data[0] != SDO_ABORT){
          DBG_OUTPUT_PORT.println("Sent gain and offset -> map successful");
          return Ok;
        }
      }
    }
  }

  DBG_OUTPUT_PORT.println("Mapping failed");

  return CommError;
}

SetResult RemoveCanMapping(String json){
  if (state != IDLE) return CommError;

  JsonDocument doc;
  twai_message_t rxframe;

  deserializeJson(doc, json);

  if (doc["index"].isNull() || doc["subindex"].isNull()) {
    DBG_OUTPUT_PORT.println("Remove: Missing argument");

    return UnknownIndex;
  }

  // The index from GetCanMapping is a read index (0x3100+ for TX, 0x3180+ for RX)
  // To remove a mapping, we need to determine if it's TX or RX and write to the correct base index
  uint32_t readIndex = doc["index"].as<uint32_t>();
  uint32_t writeIndex;
  bool isRx;

  if (readIndex >= SDO_INDEX_MAP_RD + 0x80) {
    // RX mapping (read index 0x3180+)
    writeIndex = readIndex;  // Use the read index directly for removal
    isRx = true;
  } else if (readIndex >= SDO_INDEX_MAP_RD) {
    // TX mapping (read index 0x3100+)
    writeIndex = readIndex;  // Use the read index directly for removal
    isRx = false;
  } else {
    DBG_OUTPUT_PORT.printf("Remove: Invalid index 0x%lX\n", (unsigned long)readIndex);
    return UnknownIndex;
  }

  DBG_OUTPUT_PORT.printf("Removing %s mapping at index 0x%lX, subindex 0\n",
                         isRx ? "RX" : "TX", (unsigned long)writeIndex);

  // Write 0 to subindex 0 (COB ID) to remove the entire mapping
  setValueSdo(_nodeId, writeIndex, 0, 0U);

  if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
    printCanRx(&rxframe);
    if (rxframe.data[0] != SDO_ABORT){
      DBG_OUTPUT_PORT.println("Item removed");
      return Ok;
    }
    else {
      DBG_OUTPUT_PORT.println("Invalid item index/subindex");
      return UnknownIndex;
    }
  }
  DBG_OUTPUT_PORT.println("Comm Error");
  return CommError;
}

bool ClearCanMap(bool isRx) {
  if (state != IDLE) return false;

  twai_message_t rxframe;
  int baseIndex = isRx ? (SDO_INDEX_MAP_RD + 0x80) : SDO_INDEX_MAP_RD;
  int removedCount = 0;
  const int MAX_ITERATIONS = 100; // Safety limit to prevent infinite loops

  DBG_OUTPUT_PORT.printf("Clearing all %s CAN mappings\n", isRx ? "RX" : "TX");

  // Repeatedly delete the first entry (index + 0, subindex 0) until we get an abort
  for (int i = 0; i < MAX_ITERATIONS; i++) {
    // Write 0 to the first mapping slot (index 0x3100 or 0x3180, subindex 0)
    setValueSdo(_nodeId, baseIndex, 0, 0U);

    if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
      printCanRx(&rxframe);

      if (rxframe.data[0] == SDO_ABORT) {
        // Abort means no more entries to delete
        DBG_OUTPUT_PORT.printf("All %s mappings cleared (%d removed)\n", isRx ? "RX" : "TX", removedCount);
        return true;
      } else {
        // Successfully removed one entry
        removedCount++;
        DBG_OUTPUT_PORT.printf("Removed %s mapping #%d\n", isRx ? "RX" : "TX", removedCount);
      }
    } else {
      // Communication timeout
      DBG_OUTPUT_PORT.printf("Communication timeout while clearing %s mappings\n", isRx ? "RX" : "TX");
      return false;
    }
  }

  // Hit maximum iterations - probably a bug
  DBG_OUTPUT_PORT.printf("Warning: Hit maximum iterations (%d) while clearing %s mappings\n",
                         MAX_ITERATIONS, isRx ? "RX" : "TX");
  return false;
}

SetResult SetValue(int paramId, double value) {
  if (state != IDLE) return CommError;

  twai_message_t rxframe;

  setValueSdo(_nodeId, SDO_INDEX_PARAM_UID | (paramId >> 8), paramId & 0xFF, (uint32_t)(value * 32));

  if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
    printCanRx(&rxframe);
    if (rxframe.data[0] == SDO_RESPONSE_DOWNLOAD)
      return Ok;
    else if (*(uint32_t*)&rxframe.data[4] == SDO_ERR_RANGE)
      return ValueOutOfRange;
    else
      return UnknownIndex;
  }
  else {
    return CommError;
  }
}

bool SaveToFlash() {
  if (state != IDLE) return false;

  twai_message_t rxframe;

  setValueSdo(_nodeId, SDO_INDEX_COMMANDS, SDO_CMD_SAVE, 0U);

  if (twai_receive(&rxframe, pdMS_TO_TICKS(200)) == ESP_OK) {
    printCanRx(&rxframe);
    return true;
  }
  else {
    return false;
  }
}

bool LoadFromFlash() {
  if (state != IDLE) return false;

  twai_message_t rxframe;

  setValueSdo(_nodeId, SDO_INDEX_COMMANDS, SDO_CMD_LOAD, 0U);

  if (twai_receive(&rxframe, pdMS_TO_TICKS(200)) == ESP_OK) {
    printCanRx(&rxframe);
    return true;
  }
  else {
    return false;
  }
}

bool LoadDefaults() {
  if (state != IDLE) return false;

  twai_message_t rxframe;

  setValueSdo(_nodeId, SDO_INDEX_COMMANDS, SDO_CMD_DEFAULTS, 0U);

  if (twai_receive(&rxframe, pdMS_TO_TICKS(200)) == ESP_OK) {
    printCanRx(&rxframe);
    return true;
  }
  else {
    return false;
  }
}

bool StartDevice(uint32_t mode) {
  if (state != IDLE) return false;

  twai_message_t rxframe;

  setValueSdo(_nodeId, SDO_INDEX_COMMANDS, SDO_CMD_START, mode);

  if (twai_receive(&rxframe, pdMS_TO_TICKS(200)) == ESP_OK) {
    printCanRx(&rxframe);
    return true;
  }
  else {
    return false;
  }
}

bool StopDevice() {
  if (state != IDLE) return false;

  twai_message_t rxframe;

  setValueSdo(_nodeId, SDO_INDEX_COMMANDS, SDO_CMD_STOP, 0U);

  if (twai_receive(&rxframe, pdMS_TO_TICKS(200)) == ESP_OK) {
    printCanRx(&rxframe);
    return true;
  }
  else {
    return false;
  }
}

String ListErrors() {
  if (state != IDLE) {
    DBG_OUTPUT_PORT.println("ListErrors called while not IDLE, ignoring");
    return "[]";
  }

  twai_message_t rxframe;
  JsonDocument doc;
  JsonArray errors = doc.to<JsonArray>();

  // Build error description map from parameter JSON (lasterr field)
  std::map<int, String> errorDescriptions;
  if (!cachedParamJson.isNull() && cachedParamJson.containsKey("lasterr")) {
    JsonObject lasterr = cachedParamJson["lasterr"].as<JsonObject>();
    for (JsonPair kv : lasterr) {
      int errorNum = atoi(kv.key().c_str());
      errorDescriptions[errorNum] = kv.value().as<String>();
    }
    DBG_OUTPUT_PORT.printf("Loaded %d error descriptions from lasterr\n", errorDescriptions.size());
  }

  // Determine tick duration from uptime parameter's unit (default: 10ms)
  int tickDurationMs = 10; // Default to 10ms
  if (!cachedParamJson.isNull() && cachedParamJson.containsKey("uptime")) {
    JsonObject uptime = cachedParamJson["uptime"].as<JsonObject>();
    if (uptime.containsKey("unit")) {
      String unit = uptime["unit"].as<String>();
      if (unit == "sec" || unit == "s") {
        tickDurationMs = 1000; // 1 second
        DBG_OUTPUT_PORT.println("Using 1-second tick duration based on uptime unit");
      }
    }
  }

  DBG_OUTPUT_PORT.printf("Retrieving error log (tick duration: %dms)\n", tickDurationMs);

  // Iterate through error indices (0-254)
  for (uint8_t i = 0; i < 255; i++) {
    uint32_t errorTime = 0;
    uint32_t errorNum = 0;
    bool hasErrorTime = false;
    bool hasErrorNum = false;

    // Request error timestamp
    requestSdoElement(_nodeId, SDO_INDEX_ERROR_TIME, i);
    if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
      printCanRx(&rxframe);
      if (rxframe.data[0] != SDO_ABORT &&
          (rxframe.data[1] | rxframe.data[2] << 8) == SDO_INDEX_ERROR_TIME &&
          rxframe.data[3] == i) {
        errorTime = *(uint32_t*)&rxframe.data[4];
        hasErrorTime = true;
      }
    }

    // Request error number
    requestSdoElement(_nodeId, SDO_INDEX_ERROR_NUM, i);
    if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
      printCanRx(&rxframe);
      if (rxframe.data[0] != SDO_ABORT &&
          (rxframe.data[1] | rxframe.data[2] << 8) == SDO_INDEX_ERROR_NUM &&
          rxframe.data[3] == i) {
        errorNum = *(uint32_t*)&rxframe.data[4];
        hasErrorNum = true;
      }
    }

    // If we got an abort for both or no data, we've reached the end
    if (!hasErrorTime && !hasErrorNum) {
      DBG_OUTPUT_PORT.printf("Reached end of error log at index %d\n", i);
      break;
    }

    // If we have error data, add it to the result
    if (hasErrorTime && hasErrorNum && errorNum != 0) {
      JsonObject errorObj = errors.add<JsonObject>();
      errorObj["index"] = i;
      errorObj["errorNum"] = errorNum;
      errorObj["errorTime"] = errorTime;
      errorObj["elapsedTimeMs"] = errorTime * tickDurationMs;

      // Add description if available
      if (errorDescriptions.count(errorNum) > 0) {
        errorObj["description"] = errorDescriptions[errorNum];
      } else {
        errorObj["description"] = "Unknown error " + String(errorNum);
      }

      DBG_OUTPUT_PORT.printf("Error %lu at index %d: time=%lu ticks (%lu ms), desc=%s\n",
                             (unsigned long)errorNum, i, (unsigned long)errorTime,
                             (unsigned long)(errorTime * tickDurationMs),
                             errorObj["description"].as<const char*>());
    }
  }

  String result;
  serializeJson(doc, result);
  DBG_OUTPUT_PORT.printf("Retrieved %d errors\n", errors.size());
  return result;
}

bool SendCanMessage(uint32_t canId, const uint8_t* data, uint8_t dataLength) {
  if (dataLength > 8) return false; // CAN data cannot be longer than 8 bytes

  twai_message_t frame;
  frame.identifier = canId;
  frame.data_length_code = dataLength;
  frame.flags = 0; // Standard frame, no RTR

  // Copy data to frame
  for (uint8_t i = 0; i < dataLength; i++) {
    frame.data[i] = data[i];
  }

  // Clear remaining bytes
  for (uint8_t i = dataLength; i < 8; i++) {
    frame.data[i] = 0;
  }

  // Send the message
  esp_err_t result = twai_transmit(&frame, pdMS_TO_TICKS(10));

  if (result == ESP_OK) {
    DBG_OUTPUT_PORT.printf("Sent CAN message: ID=0x%03lX, Len=%d\n", (unsigned long)canId, dataLength);
    return true;
  } else {
    DBG_OUTPUT_PORT.printf("Failed to send CAN message: ID=0x%03lX, Error=%d\n", (unsigned long)canId, result);
    return false;
  }
}

String StreamValues(String paramIds, int samples) {
  if (state != IDLE) return "";

  twai_message_t rxframe;

  int ids[30], numItems = 0;
  String result;

  // Parse comma-separated parameter IDs
  for (int pos = 0; pos >= 0; pos = paramIds.indexOf(',', pos + 1)) {
    String idStr = paramIds.substring(pos + 1, paramIds.indexOf(',', pos + 1));
    ids[numItems++] = idStr.toInt();
  }

  for (int i = 0; i < samples; i++) {
    for (int item = 0; item < numItems; item++) {
      int id = ids[item];
      requestSdoElement(_nodeId, SDO_INDEX_PARAM_UID | (id >> 8), id & 0xFF);
    }

    int item = 0;
    while (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
      printCanRx(&rxframe);
      if (item > 0) result += ",";
      if (rxframe.data[0] == 0x80)
        result += "0";
      else {
        int receivedItem = (rxframe.data[1] << 8) + rxframe.data[3];

        if (receivedItem == ids[item])
          result += String(((double)*(int32_t*)&rxframe.data[4]) / 32, 2);
        else
          result += "0";
      }
      item++;
    }
    result += "\r\n";
  }
  return result;
}

// Try to receive and parse a parameter value response (non-blocking)
// Returns true if a valid response was received, false if no response available
bool TryGetValueResponse(int& outParamId, double& outValue, int timeoutMs) {
  twai_message_t rxframe;
  
  esp_err_t result = twai_receive(&rxframe, pdMS_TO_TICKS(timeoutMs));
  
  if (result != ESP_OK) {
    return false; // No message available
  }
  
  printCanRx(&rxframe);
  
  // Parse the response to extract paramId
  uint16_t responseIndex = rxframe.data[1] | (rxframe.data[2] << 8);
  uint8_t responseSubIndex = rxframe.data[3];
  
  // Check if this is a parameter value response (SDO_INDEX_PARAM_UID range)
  if ((responseIndex & 0xFF00) != (SDO_INDEX_PARAM_UID & 0xFF00)) {
    // Not a parameter response, skip
    return false;
  }
  
  // Reconstruct paramId from index and subindex
  int paramId = ((responseIndex & 0xFF) << 8) | responseSubIndex;
  
  // Check for SDO error response
  if (rxframe.data[0] == 0x80) {
    return false;
  }
  
  // Extract value (converted by dividing by 32)
  uint32_t rawValue = *(uint32_t*)&rxframe.data[4];
  double convertedValue = ((double)rawValue) / 32.0;
  
  outParamId = paramId;
  outValue = convertedValue;
  return true;
}

// Legacy blocking version (kept for compatibility with other code)
double GetValue(int paramId) {
  if (state != IDLE) {
    return 0;
  }

  RequestValue(paramId);
  
  // Wait for response with timeout
  unsigned long startTime = millis();
  const unsigned long TIMEOUT_MS = 100;
  
  while ((millis() - startTime) < TIMEOUT_MS) {
    int receivedParamId;
    double value;
    
    if (TryGetValueResponse(receivedParamId, value, 10)) {
      if (receivedParamId == paramId) {
        return value;
      }
      // Got a response for a different parameter, keep waiting
    }
  }
  
  return 0; // Timeout
}

bool IsIdle() {
  return state == IDLE;
}

int GetNodeId() {
  return _nodeId;
}

BaudRate GetBaudRate() {
  return baudRate;
}

// Initialize CAN bus without connecting to a specific device
void InitCAN(BaudRate baud, int txPin, int rxPin) {
  // Store pin configuration for later use
  canTxPin = txPin;
  canRxPin = rxPin;

  twai_general_config_t g_config = {
        .mode = TWAI_MODE_NORMAL,
        .tx_io = static_cast<gpio_num_t>(txPin),
        .rx_io = static_cast<gpio_num_t>(rxPin),
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 30,
        .rx_queue_len = 30,
        .alerts_enabled = TWAI_ALERT_NONE,
        .clkout_divider = 0,
        .intr_flags = 0
  };

  twai_stop();
  twai_driver_uninstall();

  twai_timing_config_t t_config;
  baudRate = baud;

  switch (baud)
  {
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

  // Accept all messages for scanning (no filtering)
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
     DBG_OUTPUT_PORT.println("CAN driver installed");
  } else {
     DBG_OUTPUT_PORT.println("Failed to install CAN driver");
     return;
  }

  // Start TWAI driver
  if (twai_start() == ESP_OK) {
    DBG_OUTPUT_PORT.println("CAN driver started");
  } else {
    DBG_OUTPUT_PORT.println("Failed to start CAN driver");
    return;
  }

  _nodeId = 0; // No specific device connected yet
  state = IDLE;
  DBG_OUTPUT_PORT.println("CAN bus initialized (no device connected)");

  // Load saved devices into memory
  LoadDevices();
}

// Initialize CAN and connect to a specific device
void Init(uint8_t nodeId, BaudRate baud, int txPin, int rxPin) {
  // Store pin configuration for later use
  canTxPin = txPin;
  canRxPin = rxPin;

  twai_general_config_t g_config = {
        .mode = TWAI_MODE_NORMAL,
        .tx_io = static_cast<gpio_num_t>(txPin),
        .rx_io = static_cast<gpio_num_t>(rxPin),
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 30,
        .rx_queue_len = 30,
        .alerts_enabled = TWAI_ALERT_NONE,
        .clkout_divider = 0,
        .intr_flags = 0
  };

  uint16_t id = SDO_RESPONSE_BASE_ID + nodeId;

  twai_stop();
  twai_driver_uninstall();

  twai_timing_config_t t_config;
  baudRate = baud;

  switch (baud)
  {
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

  // Filter for SDO responses and bootloader messages only
  twai_filter_config_t f_config = {.acceptance_code = (uint32_t)(id << 5) | (uint32_t)(BOOTLOADER_RESPONSE_ID << 21),
                                   .acceptance_mask = 0x001F001F,
                                   .single_filter = false};

  if (twai_driver_install(&g_config, &t_config, &f_config) == ESP_OK) {
     DBG_OUTPUT_PORT.println("CAN driver installed");
  } else {
     DBG_OUTPUT_PORT.println("Failed to install CAN driver");
     return;
  }

  // Start TWAI driver
  if (twai_start() == ESP_OK) {
    DBG_OUTPUT_PORT.println("CAN driver started");
  } else {
    DBG_OUTPUT_PORT.println("Failed to start CAN driver");
    return;
  }

  // Clear cached JSON when switching to a different device
  if (_nodeId != nodeId) {
    jsonReceiveBuffer = "";
    cachedParamJson.clear();
    DBG_OUTPUT_PORT.println("Cleared cached JSON (switching devices)");
  }

  _nodeId = nodeId;
  state = OBTAINSERIAL;
  stateStartTime = millis(); // Track when we entered OBTAINSERIAL state
  DBG_OUTPUT_PORT.printf("Requesting serial number from node %d (SDO 0x5000:0)\n", nodeId);
  requestSdoElement(_nodeId, SDO_INDEX_SERIAL, 0);
  DBG_OUTPUT_PORT.println("Connected to device");
}

void Loop() {
  bool recvdResponse = false;
  twai_message_t rxframe;

  // Check for OBTAINSERIAL timeout
  if (state == OBTAINSERIAL && (millis() - stateStartTime > OBTAINSERIAL_TIMEOUT_MS)) {
    DBG_OUTPUT_PORT.println("OBTAINSERIAL timeout - resetting to IDLE");
    state = IDLE;
  }

  if (twai_receive(&rxframe, 0) == ESP_OK) {
    printCanRx(&rxframe);

    // Check bootloader messages first (before SDO responses)
    if (rxframe.identifier == BOOTLOADER_RESPONSE_ID) {
      handleUpdate(&rxframe);
    }
    // Check if this is an SDO response (SDO_RESPONSE_BASE_ID to SDO_RESPONSE_MAX_ID range only)
    else if (rxframe.identifier >= SDO_RESPONSE_BASE_ID && rxframe.identifier <= SDO_RESPONSE_MAX_ID) {
      uint8_t nodeId = rxframe.identifier & 0x7F;

      // Update lastSeen for any device we receive a message from
      // This acts as a passive heartbeat mechanism
      UpdateDeviceLastSeenByNodeId(nodeId, millis());

      if (rxframe.identifier == (SDO_RESPONSE_BASE_ID | _nodeId)) {
        handleSdoResponse(&rxframe);
        recvdResponse = true;
      }
    }
    else {
      DBG_OUTPUT_PORT.printf("Received unwanted frame %" PRIu32 "\r\n", rxframe.identifier);
    }
  }

  if (updstate == REQUEST_JSON) {
    //Re-download JSON if necessary

    retries--;

    if (recvdResponse || retries < 0)
      updstate = UPD_IDLE; //if request was successful
    else
      requestSdoElement(_nodeId, SDO_INDEX_SERIAL, 0);

     delay(100);
  }

  // Process continuous scanning
  ProcessContinuousScan();

  // Note: Removed ProcessHeartbeat() - we now passively update lastSeen
  // whenever we receive messages from devices

}

bool ReloadJson() {
  if (state != IDLE) return false;

  // Remove the cached JSON file to force re-download
  if (LittleFS.exists(jsonFileName)) {
    LittleFS.remove(jsonFileName);
    DBG_OUTPUT_PORT.printf("Removed cached JSON file: %s\r\n", jsonFileName);
  }

  // Trigger JSON download
  state = OBTAINSERIAL;
  requestSdoElement(_nodeId, SDO_INDEX_SERIAL, 0);

  DBG_OUTPUT_PORT.println("Reloading JSON from device");
  return true;
}

// Overloaded version that reloads JSON for a specific device by nodeId
bool ReloadJson(uint8_t nodeId) {
  if (state != IDLE) {
    DBG_OUTPUT_PORT.printf("[ReloadJson(nodeId)] Cannot reload - device busy (state=%d)\n", state);
    return false;
  }

  // Clear the cached JSON buffer for the requested node
  if (_nodeId == nodeId) {
    // If it's the currently connected node, clear the cache
    jsonReceiveBuffer = "";
    cachedParamJson.clear();
    DBG_OUTPUT_PORT.printf("[ReloadJson(nodeId)] Cleared cache for node %d\n", nodeId);
    return true;
  } else {
    // For a different node, we can't force reload without switching
    // Just report success - the next GetRawJson(nodeId) will fetch fresh
    DBG_OUTPUT_PORT.printf("[ReloadJson(nodeId)] Marked node %d for reload on next fetch\n", nodeId);
    return true;
  }
}

bool ResetDevice() {
  if (state != IDLE) return false;

  // Send reset command to the device
  setValueSdo(_nodeId, SDO_INDEX_COMMANDS, SDO_CMD_RESET, 1U);

  DBG_OUTPUT_PORT.println("Device reset command sent");

  // The device will reset immediately and won't send an acknowledgment
  // After a short delay, trigger JSON reload
  delay(500); // Give device time to start resetting
  state = OBTAINSERIAL;
  retries = 50;

  return true;
}

// Device management functions

// Helper function: Request device serial number from a node
// Returns true if all 4 serial parts were successfully received
static bool requestDeviceSerial(uint8_t nodeId, uint32_t serialParts[4]) {
  for (uint8_t part = 0; part < 4; part++) {
    requestSdoElement(nodeId, SDO_INDEX_SERIAL, part);

    twai_message_t rxframe;
    if (twai_receive(&rxframe, pdMS_TO_TICKS(100)) != ESP_OK) {
      return false;
    }

    printCanRx(&rxframe);

    if (!isValidSerialResponse(rxframe, nodeId, part)) {
      return false;
    }

    serialParts[part] = *(uint32_t*)&rxframe.data[4];
  }
  return true;
}

// Helper function: Load devices.json from LittleFS
// Returns true if file was successfully loaded and parsed
String ScanDevices(uint8_t startNodeId, uint8_t endNodeId) {
  if (state != IDLE) return "[]";

  JsonDocument doc;
  JsonArray devices = doc.to<JsonArray>();

  // Load saved devices to update them
  JsonDocument savedDoc;
  DeviceStorage::loadDevices(savedDoc);

  // Ensure devices object exists in saved doc
  if (!savedDoc.containsKey("devices")) {
    savedDoc.createNestedObject("devices");
  }

  JsonObject savedDevices = savedDoc["devices"].as<JsonObject>();
  bool devicesUpdated = false;

  DBG_OUTPUT_PORT.printf("Scanning CAN bus for devices (nodes %d-%d)...\n", startNodeId, endNodeId);

  // Save current state
  State prevState = state;
  uint8_t prevNodeId = _nodeId;

  for (uint8_t nodeId = startNodeId; nodeId <= endNodeId; nodeId++) {
    uint32_t deviceSerial[4];

    DBG_OUTPUT_PORT.printf("Probing node %d...\n", nodeId);

    // Temporarily set the node ID for scanning
    _nodeId = nodeId;

    // Request serial number from device
    if (requestDeviceSerial(nodeId, deviceSerial)) {
      char serialStr[40];
      sprintf(serialStr, "%08" PRIX32 ":%08" PRIX32 ":%08" PRIX32 ":%08" PRIX32,
              deviceSerial[0], deviceSerial[1], deviceSerial[2], deviceSerial[3]);

      DBG_OUTPUT_PORT.printf("Found device at node %d: %s\n", nodeId, serialStr);

      // Add to scan results
      JsonObject device = devices.add<JsonObject>();
      device["nodeId"] = nodeId;
      device["serial"] = serialStr;
      device["lastSeen"] = millis();

      // Update saved devices with new nodeId and lastSeen
      DeviceStorage::updateDeviceInJson(savedDevices, serialStr, nodeId);
      devicesUpdated = true;

      DBG_OUTPUT_PORT.printf("Updated stored nodeId for %s to %d\n", serialStr, nodeId);
    }
  }

  // Restore previous state
  _nodeId = prevNodeId;
  state = prevState;

  // Save updated devices back to LittleFS
  if (devicesUpdated) {
    if (DeviceStorage::saveDevices(savedDoc)) {
      DBG_OUTPUT_PORT.println("Updated devices.json with new nodeIds");
    }
  }

  String result;
  serializeJson(doc, result);
  DBG_OUTPUT_PORT.printf("Scan complete. Found %d devices\n", devices.size());

  return result;
}

String GetSavedDevices() {
  JsonDocument doc;
  JsonObject devices = doc["devices"].to<JsonObject>();

  // Build JSON from in-memory device list
  for (auto& kv : deviceList) {
    const Device& dev = kv.second;
    JsonObject deviceObj = devices[dev.serial].to<JsonObject>();
    deviceObj["nodeId"] = dev.nodeId;
    deviceObj["name"] = dev.name;
    deviceObj["lastSeen"] = dev.lastSeen;
  }

  String result;
  serializeJson(doc, result);
  return result;
}

bool SaveDeviceName(String serial, String name, int nodeId) {
  JsonDocument doc;

  // Load existing devices
  DeviceStorage::loadDevices(doc);

  // Ensure devices object exists
  if (!doc.containsKey("devices")) {
    doc.createNestedObject("devices");
  }

  JsonObject devices = doc["devices"].as<JsonObject>();

  // Get or create device object (serial is the key)
  if (!devices.containsKey(serial)) {
    devices.createNestedObject(serial);
  }

  JsonObject device = devices[serial].as<JsonObject>();
  device["name"] = name;

  if (nodeId >= 0) {
    device["nodeId"] = nodeId;
  }

  DBG_OUTPUT_PORT.printf("Saved device: %s -> %s (nodeId: %d)\n", serial.c_str(), name.c_str(), nodeId);

  // Save back to file
  if (!DeviceStorage::saveDevices(doc)) {
    DBG_OUTPUT_PORT.println("Failed to save devices file");
    return false;
  }

  // Also update in-memory list
  AddOrUpdateDevice(serial.c_str(), nodeId >= 0 ? nodeId : 0, name.c_str(), 0);

  DBG_OUTPUT_PORT.println("Saved devices file and updated in-memory list");
  return true;
}

bool DeleteDevice(String serial) {
  JsonDocument doc;

  // Load existing devices
  DeviceStorage::loadDevices(doc);

  // Check if devices object exists
  if (!doc.containsKey("devices")) {
    DBG_OUTPUT_PORT.println("No devices to delete");
    return false;
  }

  JsonObject devices = doc["devices"].as<JsonObject>();

  // Check if device exists
  if (!devices.containsKey(serial)) {
    DBG_OUTPUT_PORT.printf("Device %s not found\n", serial.c_str());
    return false;
  }

  // Remove device
  devices.remove(serial);

  DBG_OUTPUT_PORT.printf("Deleted device: %s\n", serial.c_str());

  // Save back to file
  if (!DeviceStorage::saveDevices(doc)) {
    DBG_OUTPUT_PORT.println("Failed to save devices file");
    return false;
  }

  // Also remove from in-memory list
  deviceList.erase(serial.c_str());

  DBG_OUTPUT_PORT.println("Deleted device from file and in-memory list");
  return true;
}


// Continuous scanning implementation

bool StartContinuousScan(uint8_t startNodeId, uint8_t endNodeId) {
  if (state != IDLE) {
    DBG_OUTPUT_PORT.printf("Cannot start continuous scan - device busy: %d\n", state);
    return false;
  }

  // Reinitialize CAN bus with accept-all filter for scanning
  // This ensures we can see all nodes, not just the previously connected one
  DBG_OUTPUT_PORT.println("Reinitializing CAN bus for scanning (accept all messages)");
  InitCAN(baudRate, canTxPin, canRxPin); // Use current baud rate and stored pins

  continuousScanActive = true;
  scanStartNode = startNodeId;
  scanEndNode = endNodeId;
  currentScanNode = startNodeId;
  currentSerialPartIndex = 0;
  lastScanTime = 0;

  DBG_OUTPUT_PORT.printf("Started continuous CAN scan (nodes %d-%d)\n", startNodeId, endNodeId);
  return true;
}

void StopContinuousScan() {
  continuousScanActive = false;
  DBG_OUTPUT_PORT.println("Stopped continuous CAN scan");
}

bool IsContinuousScanActive() {
  return continuousScanActive;
}

void SetDeviceDiscoveryCallback(DeviceDiscoveryCallback callback) {
  discoveryCallback = callback;
}

void SetScanProgressCallback(ScanProgressCallback callback) {
  scanProgressCallback = callback;
}

void SetConnectionReadyCallback(ConnectionReadyCallback callback) {
  connectionReadyCallback = callback;
}

void SetJsonDownloadProgressCallback(JsonDownloadProgressCallback callback) {
  jsonProgressCallback = callback;
}

void SetJsonStreamCallback(JsonStreamCallback callback) {
  jsonStreamCallback = callback;
}

void ProcessContinuousScan() {
  unsigned long currentTime = millis();

  if (!shouldProcessScan(currentTime)) {
    return;
  }

  lastScanTime = currentTime;

  // Request serial number part from current scan node
  requestSdoElement(currentScanNode, SDO_INDEX_SERIAL, currentSerialPartIndex);

  // Notify scan progress when starting a new node (first serial part)
  if (currentSerialPartIndex == 0 && scanProgressCallback) {
    scanProgressCallback(currentScanNode, scanStartNode, scanEndNode);
  }

  twai_message_t rxframe;
  if (twai_receive(&rxframe, pdMS_TO_TICKS(SCAN_TIMEOUT_MS)) == ESP_OK) {
    printCanRx(&rxframe);

    if (!handleScanResponse(rxframe, currentTime)) {
      // No response or error - move to next node
      advanceScanNode();
    }
  } else {
    // Timeout - move to next node
    advanceScanNode();
  }
}

// Device list management functions
void LoadDevices() {
  deviceList.clear();

  JsonDocument doc;
  if (!DeviceStorage::loadDevices(doc)) {
    DBG_OUTPUT_PORT.println("No devices.json file, starting with empty device list");
    return;
  }

  if (!doc.containsKey("devices")) {
    DBG_OUTPUT_PORT.println("No 'devices' key in devices.json");
    return;
  }

  JsonObject devices = doc["devices"].as<JsonObject>();
  int count = 0;

  for (JsonPair kv : devices) {
    Device dev;
    dev.serial = kv.key().c_str();

    JsonObject deviceObj = kv.value().as<JsonObject>();
    dev.nodeId = deviceObj["nodeId"] | 0;
    dev.name = deviceObj["name"] | "";
    dev.lastSeen = deviceObj["lastSeen"] | 0;

    deviceList[dev.serial] = dev;
    count++;
  }

  DBG_OUTPUT_PORT.printf("Loaded %d devices from file\n", count);
}

void AddOrUpdateDevice(const char* serial, uint8_t nodeId, const char* name, uint32_t lastSeen) {
  Device dev;

  // Check if device already exists
  if (deviceList.count(serial) > 0) {
    dev = deviceList[serial];
  } else {
    dev.serial = serial;
  }

  // Update fields
  if (nodeId > 0) {
    dev.nodeId = nodeId;
  }
  if (name != nullptr && strlen(name) > 0) {
    dev.name = name;
  }
  if (lastSeen > 0) {
    dev.lastSeen = lastSeen;
  }

  deviceList[serial] = dev;
}

// Heartbeat implementation
static unsigned long lastHeartbeatTime = 0;
static const unsigned long HEARTBEAT_INTERVAL_MS = 5000; // Base interval: 5 seconds
static const unsigned long MAX_HEARTBEAT_INTERVAL_MS = 60000; // Max backoff: 60 seconds
static int heartbeatDeviceIndex = 0;

// Per-device heartbeat backoff state (serial -> next heartbeat time)
static std::map<String, unsigned long> deviceNextHeartbeat;
static std::map<String, int> deviceFailureCount;

// Throttle passive heartbeat updates to prevent flooding WebSocket
static const unsigned long PASSIVE_HEARTBEAT_THROTTLE_MS = 1000; // Update at most once per second
static std::map<uint8_t, unsigned long> lastPassiveHeartbeatByNode; // nodeId -> last update time

void ProcessHeartbeat() {
  // Don't send heartbeats if we're scanning or not in IDLE state
  if (continuousScanActive || state != IDLE) {
    return;
  }

  unsigned long currentTime = millis();
  if (currentTime - lastHeartbeatTime < HEARTBEAT_INTERVAL_MS) {
    return; // Not time for heartbeat yet
  }

  lastHeartbeatTime = currentTime;

  // Use in-memory device list
  if (deviceList.empty()) {
    return; // No devices to heartbeat
  }

  // Build vector of device serials from in-memory list
  std::vector<String> deviceSerials;
  for (auto& kv : deviceList) {
    deviceSerials.push_back(kv.first);
  }

  // Cycle through devices, one per heartbeat interval
  if (heartbeatDeviceIndex >= deviceSerials.size()) {
    heartbeatDeviceIndex = 0;
  }

  String serial = deviceSerials[heartbeatDeviceIndex];
  Device& device = deviceList[serial];

  if (device.nodeId == 0) {
    heartbeatDeviceIndex++;
    return;
  }

  uint8_t nodeId = device.nodeId;

  // Don't send heartbeat to the currently active device
  if (nodeId == _nodeId) {
    heartbeatDeviceIndex++;
    return;
  }

  // Check if it's time to heartbeat this device (exponential backoff)
  if (deviceNextHeartbeat.count(serial) > 0 && currentTime < deviceNextHeartbeat[serial]) {
    heartbeatDeviceIndex++;
    return; // Not time yet for this device
  }

  // Send a simple request to check if device is alive
  // Request first part of serial number as a lightweight ping
  requestSdoElement(nodeId, SDO_INDEX_SERIAL, 0);

  twai_message_t rxframe;
  bool deviceResponded = false;

  if (twai_receive(&rxframe, pdMS_TO_TICKS(100)) == ESP_OK) {
    if (rxframe.identifier == (SDO_RESPONSE_BASE_ID | nodeId) &&
        rxframe.data[0] != SDO_ABORT) {
      deviceResponded = true;

      // Device responded! Update lastSeen
      UpdateDeviceLastSeen(serial.c_str(), currentTime);

      DBG_OUTPUT_PORT.printf("Heartbeat: Device %s (node %d) is alive\n", serial.c_str(), nodeId);

      // Reset failure count and backoff
      deviceFailureCount[serial] = 0;
      deviceNextHeartbeat[serial] = currentTime + HEARTBEAT_INTERVAL_MS;
    }
  }

  if (!deviceResponded) {
    // Device didn't respond - apply exponential backoff
    int failures = deviceFailureCount[serial];
    deviceFailureCount[serial] = failures + 1;

    // Calculate backoff: base_interval * 2^failures, capped at max
    unsigned long backoffInterval = HEARTBEAT_INTERVAL_MS * (1 << failures);
    if (backoffInterval > MAX_HEARTBEAT_INTERVAL_MS) {
      backoffInterval = MAX_HEARTBEAT_INTERVAL_MS;
    }

    deviceNextHeartbeat[serial] = currentTime + backoffInterval;

    DBG_OUTPUT_PORT.printf("Heartbeat: Device %s (node %d) not responding (failures: %d, next try in %lums)\n",
                           serial.c_str(), nodeId, failures + 1, (unsigned long)backoffInterval);
  }

  // Move to next device
  heartbeatDeviceIndex++;
}

void UpdateDeviceLastSeen(const char* serial, uint32_t lastSeen) {
  // Update in-memory device list only (not saved to file)
  if (deviceList.count(serial) > 0) {
    deviceList[serial].lastSeen = lastSeen;

    // Notify via callback (will broadcast to WebSocket clients)
    if (discoveryCallback) {
      uint8_t nodeId = deviceList[serial].nodeId;
      discoveryCallback(nodeId, serial, lastSeen);
    }
  }
}

void UpdateDeviceLastSeenByNodeId(uint8_t nodeId, uint32_t lastSeen) {
  // Throttle updates to prevent flooding WebSocket with too many messages
  // Only update if enough time has passed since last update for this node
  if (lastPassiveHeartbeatByNode.count(nodeId) > 0) {
    unsigned long timeSinceLastUpdate = lastSeen - lastPassiveHeartbeatByNode[nodeId];
    if (timeSinceLastUpdate < PASSIVE_HEARTBEAT_THROTTLE_MS) {
      return; // Too soon, skip this update
    }
  }

  // Record this update time
  lastPassiveHeartbeatByNode[nodeId] = lastSeen;

  // Find device by nodeId and update its lastSeen
  for (auto& kv : deviceList) {
    if (kv.second.nodeId == nodeId) {
      UpdateDeviceLastSeen(kv.first.c_str(), lastSeen);
      return;
    }
  }
}

int GetJsonTotalSize() {
  return jsonTotalSize;
}

}
