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
static int currentPage = 0;
static const size_t PAGE_SIZE_BYTES = 1024;
static int retries = 0;
static JsonDocument paramJson; // In-memory parameter JSON storage
static String jsonBuffer;      // Buffer for receiving JSON segments
static int jsonTotalSize = 0;  // Total size of JSON being downloaded (from SDO response)

// Continuous scanning state
static bool continuousScanActive = false;
static uint8_t scanStartNode = 1;
static uint8_t scanEndNode = 32;
static uint8_t currentScanNode = 1;
static uint8_t scanSerialPart = 0; // Which part of serial we're reading (0-3)
static uint32_t scanDeviceSerial[4];
static unsigned long lastScanTime = 0;

// State timeout tracking
static unsigned long stateStartTime = 0;
static const unsigned long OBTAINSERIAL_TIMEOUT_MS = 5000; // 5 second timeout
static const unsigned long SCAN_DELAY_MS = 50; // Delay between node probes
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

// CAN debug helpers
static void printCanTx(const twai_message_t* frame) {
  // Debug output disabled
}

static void printCanRx(const twai_message_t* frame) {
  // Debug output disabled
}

static void requestSdoElement(uint8_t nodeId, uint16_t index, uint8_t subIndex) {
  tx_frame.extd = false;
  tx_frame.identifier = 0x600 | nodeId;
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

// Send SDO request for a parameter value (non-blocking)
void RequestValue(int paramId) {
  uint16_t index = SDO_INDEX_PARAM_UID | (paramId >> 8);
  uint8_t subIndex = paramId & 0xFF;
  requestSdoElement(_nodeId, index, subIndex);
}

static void setValueSdo(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t value) {
  tx_frame.extd = false;
  tx_frame.identifier = 0x600 | nodeId;
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
  tx_frame.identifier = 0x600 | _nodeId;
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
          jsonBuffer += (char)rxframe->data[1 + i];
        }

        DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Download complete");
        DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] JSON size: %d bytes\r\n", jsonBuffer.length());

        // Parse JSON into paramJson for future use
        DeserializationError error = deserializeJson(paramJson, jsonBuffer);
        if (error) {
          DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] Parse error: %s\r\n", error.c_str());
        } else {
          DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Parsed successfully");
        }

        state = IDLE;
      }
      //Receiving a segment
      else if (rxframe->data[0] == (toggleBit << 4) && (rxframe->data[0] & SDO_READ) == 0) {
        DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] Segment received (buffer: %d bytes)\n", jsonBuffer.length());
        for (int i = 0; i < 7; i++) {
          jsonBuffer += (char)rxframe->data[1 + i];
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

static uint32_t crc32_word(uint32_t Crc, uint32_t Data)
{
  int i;

  Crc = Crc ^ Data;

  for(i=0; i<32; i++)
    if (Crc & 0x80000000)
      Crc = (Crc << 1) ^ 0x04C11DB7; // Polynomial used in STM32
    else
      Crc = (Crc << 1);

  return(Crc);
}

static void handleUpdate(twai_message_t *rxframe) {
  static int currentByte = 0;
  static uint32_t crc;

  switch (updstate) {
    case SEND_MAGIC:
      if (rxframe->data[0] == 0x33) {
        tx_frame.identifier = 0x7dd;
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
        tx_frame.identifier = 0x7dd;
        tx_frame.data_length_code = 1;

        tx_frame.data[0] = (updateFile.size() + PAGE_SIZE_BYTES - 1) / PAGE_SIZE_BYTES;
        updstate = SEND_PAGE;
        crc = 0xFFFFFFFF;
        currentByte = 0;
        currentPage = 0;
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

        tx_frame.identifier = 0x7dd;
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
        tx_frame.identifier = 0x7dd;
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
      DBG_OUTPUT_PORT.printf("Sent bytes %u-%u... ", currentPage * PAGE_SIZE_BYTES, currentByte);
      if (rxframe->data[0] == 'P') {
        updstate = SEND_PAGE;
        currentPage++;
        DBG_OUTPUT_PORT.printf("CRC Good\r\n");
        handleUpdate(rxframe);
      }
      else if (rxframe->data[0] == 'E') {
        updstate = SEND_PAGE;
        currentByte = currentPage * PAGE_SIZE_BYTES;
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
  currentPage = 0;

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
  return currentPage;
}

bool IsUpdateInProgress() {
  return updstate != UPD_IDLE;
}

String GetRawJson() {
  // Return cached JSON if available (avoid blocking download)
  if (!jsonBuffer.isEmpty()) {
    DBG_OUTPUT_PORT.printf("[GetRawJson] Returning cached JSON (%d bytes)\n", jsonBuffer.length());
    return jsonBuffer;
  }

  if (state != IDLE) {
    DBG_OUTPUT_PORT.printf("[GetRawJson] Cannot get JSON - device busy (state=%d)\n", state);
    return "{}";
  }

  // Trigger JSON download from device
  DBG_OUTPUT_PORT.printf("[GetRawJson] Starting JSON download from node %d\n", _nodeId);
  state = OBTAIN_JSON;
  jsonBuffer = ""; // Clear buffer
  paramJson.clear(); // Clear parsed JSON
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
    if (jsonBuffer.length() > lastBufferSize) {
      int newDataSize = jsonBuffer.length() - lastBufferSize;
      lastBufferSize = jsonBuffer.length();
      lastSegmentTime = millis(); // Reset timeout on new data

      // Stream new data chunk to callback if registered
      if (jsonStreamCallback && jsonBuffer.length() > lastStreamedSize) {
        int chunkSize = jsonBuffer.length() - lastStreamedSize;
        const char* chunkStart = jsonBuffer.c_str() + lastStreamedSize;
        jsonStreamCallback(chunkStart, chunkSize, false); // isComplete = false
        lastStreamedSize = jsonBuffer.length();
      }

      // Send progress update via callback (throttled)
      if (jsonProgressCallback && (millis() - lastProgressUpdate > PROGRESS_UPDATE_INTERVAL_MS)) {
        jsonProgressCallback(jsonBuffer.length());
        lastProgressUpdate = millis();
      }

      if (loopCount % 100 == 0) {
        DBG_OUTPUT_PORT.printf("[GetRawJson] Progress: buffer size: %d bytes\n", jsonBuffer.length());
      }
    }

    // Check for segment timeout (no data received for too long)
    if ((millis() - lastSegmentTime) > SEGMENT_TIMEOUT_MS) {
      DBG_OUTPUT_PORT.printf("[GetRawJson] Segment timeout! No data for %lu ms, buffer size=%d\n",
        millis() - lastSegmentTime, jsonBuffer.length());
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
    DBG_OUTPUT_PORT.printf("[GetRawJson] Failed! State=%d, buffer size=%d\n", state, jsonBuffer.length());
    state = IDLE;
    return "{}";
  }

  DBG_OUTPUT_PORT.printf("[GetRawJson] Download complete! Buffer size: %d bytes\n", jsonBuffer.length());

  // Send final chunk with completion flag if streaming
  if (jsonStreamCallback && jsonBuffer.length() > lastStreamedSize) {
    int chunkSize = jsonBuffer.length() - lastStreamedSize;
    const char* chunkStart = jsonBuffer.c_str() + lastStreamedSize;
    jsonStreamCallback(chunkStart, chunkSize, true); // isComplete = true
  } else if (jsonStreamCallback) {
    // No remaining data, but signal completion
    jsonStreamCallback("", 0, true);
  }

  // Send completion notification (0 = done)
  if (jsonProgressCallback) {
    jsonProgressCallback(0);
  }

  return jsonBuffer;
}

// Overloaded version that fetches JSON from a specific device by nodeId
String GetRawJson(uint8_t nodeId) {
  if (state != IDLE) {
    DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Cannot get JSON - device busy (state=%d)\n", state);
    return "{}";
  }

  // If we're already connected to the requested node, just use the regular GetRawJson
  if (_nodeId == nodeId && !jsonBuffer.isEmpty()) {
    DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Already connected to node %d, using cached JSON\n", nodeId);
    return GetRawJson();
  }

  // If we're connected to the requested node but cache is empty, fetch without switching
  if (_nodeId == nodeId) {
    DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Already connected to node %d, fetching fresh JSON\n", nodeId);
    jsonBuffer = "";
    paramJson.clear();
    
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

      if (jsonBuffer.length() > lastBufferSize) {
        lastBufferSize = jsonBuffer.length();
        lastSegmentTime = millis();

        // Stream new data chunk to callback if registered
        if (jsonStreamCallback && jsonBuffer.length() > lastStreamedSize) {
          int chunkSize = jsonBuffer.length() - lastStreamedSize;
          const char* chunkStart = jsonBuffer.c_str() + lastStreamedSize;
          jsonStreamCallback(chunkStart, chunkSize, false);
          lastStreamedSize = jsonBuffer.length();
        }

        // Send progress update via callback (throttled)
        if (jsonProgressCallback && (millis() - lastProgressUpdate > PROGRESS_UPDATE_INTERVAL_MS)) {
          jsonProgressCallback(jsonBuffer.length());
          lastProgressUpdate = millis();
        }

        if (loopCount % 100 == 0) {
          DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Progress: buffer size: %d bytes\n", jsonBuffer.length());
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

    DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Download complete! Buffer size: %d bytes\n", jsonBuffer.length());

    // Send final chunk with completion flag if streaming
    if (jsonStreamCallback && jsonBuffer.length() > lastStreamedSize) {
      int chunkSize = jsonBuffer.length() - lastStreamedSize;
      const char* chunkStart = jsonBuffer.c_str() + lastStreamedSize;
      jsonStreamCallback(chunkStart, chunkSize, true);
    } else if (jsonStreamCallback) {
      jsonStreamCallback("", 0, true);
    }

    // Send completion notification (0 = done)
    if (jsonProgressCallback) {
      jsonProgressCallback(0);
    }

    return jsonBuffer;
  }

  // Save current state for switching to different node
  uint8_t savedNodeId = _nodeId;
  String savedJsonBuffer = jsonBuffer;

  // Switch to target device and clear cache
  DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Switching from node %d to node %d\n", _nodeId, nodeId);
  _nodeId = nodeId;
  jsonBuffer = "";
  paramJson.clear();

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

    if (jsonBuffer.length() > lastBufferSize) {
      lastBufferSize = jsonBuffer.length();
      lastSegmentTime = millis();

      // Stream new data chunk to callback if registered
      if (jsonStreamCallback && jsonBuffer.length() > lastStreamedSize) {
        int chunkSize = jsonBuffer.length() - lastStreamedSize;
        const char* chunkStart = jsonBuffer.c_str() + lastStreamedSize;
        jsonStreamCallback(chunkStart, chunkSize, false);
        lastStreamedSize = jsonBuffer.length();
      }

      // Send progress update via callback (throttled)
      if (jsonProgressCallback && (millis() - lastProgressUpdate > PROGRESS_UPDATE_INTERVAL_MS)) {
        jsonProgressCallback(jsonBuffer.length());
        lastProgressUpdate = millis();
      }

      if (loopCount % 100 == 0) {
        DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Progress: buffer size: %d bytes\n", jsonBuffer.length());
      }
    }

    if ((millis() - lastSegmentTime) > SEGMENT_TIMEOUT_MS) {
      DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Segment timeout! No data for %lu ms\n", millis() - lastSegmentTime);
      state = IDLE;
      // Restore previous state
      _nodeId = savedNodeId;
      jsonBuffer = savedJsonBuffer;
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
    jsonBuffer = savedJsonBuffer;
    return "{}";
  }

  DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Download complete! Buffer size: %d bytes\n", jsonBuffer.length());
  String result = jsonBuffer;

  // Send final chunk with completion flag if streaming
  if (jsonStreamCallback && jsonBuffer.length() > lastStreamedSize) {
    int chunkSize = jsonBuffer.length() - lastStreamedSize;
    const char* chunkStart = jsonBuffer.c_str() + lastStreamedSize;
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
  jsonBuffer = savedJsonBuffer;
  DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Restored to node %d\n", _nodeId);

  return result;
}

bool SendJson(WiFiClient client) {
  if (state != IDLE) return false;

  JsonDocument doc;
  twai_message_t rxframe;

  // Use in-memory JSON if available
  if (paramJson.isNull() || paramJson.size() == 0) {
    DBG_OUTPUT_PORT.println("No parameter JSON in memory");
    return false;
  }

  JsonObject root = paramJson.as<JsonObject>();
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

  setValueSdo(_nodeId, doc["index"].as<uint32_t>(), doc["subindex"].as<uint8_t>(), 0U); //Writing 0 to map index removes the mapping

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

  uint16_t id = 0x580 + nodeId;

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
  twai_filter_config_t f_config = {.acceptance_code = (uint32_t)(id << 5) | (uint32_t)(0x7de << 21),
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
    jsonBuffer = "";
    paramJson.clear();
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
    if (rxframe.identifier == 0x7de) {
      handleUpdate(&rxframe);
    }
    // Check if this is an SDO response (0x580 to 0x5FF range only)
    else if (rxframe.identifier >= 0x580 && rxframe.identifier <= 0x5FF) {
      uint8_t nodeId = rxframe.identifier & 0x7F;

      // Update lastSeen for any device we receive a message from
      // This acts as a passive heartbeat mechanism
      UpdateDeviceLastSeenByNodeId(nodeId, millis());

      if (rxframe.identifier == (0x580 | _nodeId)) {
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
    jsonBuffer = "";
    paramJson.clear();
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

String ScanDevices(uint8_t startNodeId, uint8_t endNodeId) {
  if (state != IDLE) return "[]";

  JsonDocument doc;
  JsonArray devices = doc.to<JsonArray>();

  // Load saved devices to update them
  JsonDocument savedDoc;
  if (LittleFS.exists("/devices.json")) {
    File file = LittleFS.open("/devices.json", "r");
    if (file) {
      deserializeJson(savedDoc, file);
      file.close();
    }
  }

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
    twai_message_t rxframe;
    uint32_t deviceSerial[4];
    bool deviceFound = true;

    DBG_OUTPUT_PORT.printf("Probing node %d...\n", nodeId);

    // Temporarily set the node ID for scanning
    _nodeId = nodeId;

    // Request serial number parts
    for (uint8_t part = 0; part < 4; part++) {
      requestSdoElement(_nodeId, SDO_INDEX_SERIAL, part);

      if (twai_receive(&rxframe, pdMS_TO_TICKS(100)) == ESP_OK) {
        printCanRx(&rxframe);
        if (rxframe.identifier == (0x580 | nodeId) &&
            rxframe.data[0] != SDO_ABORT &&
            (rxframe.data[1] | rxframe.data[2] << 8) == SDO_INDEX_SERIAL &&
            rxframe.data[3] == part) {
          deviceSerial[part] = *(uint32_t*)&rxframe.data[4];
        } else {
          deviceFound = false;
          break;
        }
      } else {
        deviceFound = false;
        break;
      }
    }

    if (deviceFound) {
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
      if (!savedDevices.containsKey(serialStr)) {
        savedDevices.createNestedObject(serialStr);
      }
      JsonObject savedDevice = savedDevices[serialStr].as<JsonObject>();
      savedDevice["nodeId"] = nodeId;
      savedDevice["lastSeen"] = millis();
      devicesUpdated = true;

      DBG_OUTPUT_PORT.printf("Updated stored nodeId for %s to %d\n", serialStr, nodeId);
    }
  }

  // Restore previous state
  _nodeId = prevNodeId;
  state = prevState;

  // Save updated devices back to LittleFS
  if (devicesUpdated) {
    File file = LittleFS.open("/devices.json", "w");
    if (file) {
      serializeJson(savedDoc, file);
      file.close();
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
  if (LittleFS.exists("/devices.json")) {
    File file = LittleFS.open("/devices.json", "r");
    if (file) {
      deserializeJson(doc, file);
      file.close();
    }
  }

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
  File file = LittleFS.open("/devices.json", "w");
  if (!file) {
    DBG_OUTPUT_PORT.println("Failed to open devices file for writing");
    return false;
  }

  serializeJson(doc, file);
  file.close();

  // Also update in-memory list
  AddOrUpdateDevice(serial.c_str(), nodeId >= 0 ? nodeId : 0, name.c_str(), 0);

  DBG_OUTPUT_PORT.println("Saved devices file and updated in-memory list");
  return true;
}

bool DeleteDevice(String serial) {
  JsonDocument doc;

  // Load existing devices
  if (LittleFS.exists("/devices.json")) {
    File file = LittleFS.open("/devices.json", "r");
    if (file) {
      deserializeJson(doc, file);
      file.close();
    }
  }

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
  File file = LittleFS.open("/devices.json", "w");
  if (!file) {
    DBG_OUTPUT_PORT.println("Failed to open devices file for writing");
    return false;
  }

  serializeJson(doc, file);
  file.close();

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
  scanSerialPart = 0;
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
  if (!continuousScanActive) {
    return;
  }

  if (state != IDLE) {
    static unsigned long lastDebug = 0;
    if (millis() - lastDebug > 5000) { // Only log every 5 seconds
      DBG_OUTPUT_PORT.printf("Scan paused: state=%d (not IDLE)\n", state);
      lastDebug = millis();
    }
    return;
  }

  unsigned long currentTime = millis();
  if (currentTime - lastScanTime < SCAN_DELAY_MS) {
    return; // Not time to probe yet
  }

  lastScanTime = currentTime;

  // Request serial number part from current scan node
  requestSdoElement(currentScanNode, SDO_INDEX_SERIAL, scanSerialPart);

  // Notify scan progress when starting a new node (first serial part)
  if (scanSerialPart == 0 && scanProgressCallback) {
    scanProgressCallback(currentScanNode, scanStartNode, scanEndNode);
  }

  twai_message_t rxframe;
  if (twai_receive(&rxframe, pdMS_TO_TICKS(100)) == ESP_OK) {
    printCanRx(&rxframe);

    if (rxframe.identifier == (0x580 | currentScanNode) &&
        rxframe.data[0] != SDO_ABORT &&
        (rxframe.data[1] | rxframe.data[2] << 8) == SDO_INDEX_SERIAL &&
        rxframe.data[3] == scanSerialPart) {

      scanDeviceSerial[scanSerialPart] = *(uint32_t*)&rxframe.data[4];
      scanSerialPart++;

      // If we've read all 4 parts, we found a device
      if (scanSerialPart >= 4) {
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
        scanSerialPart = 0;
        currentScanNode++;
        if (currentScanNode > scanEndNode) {
          currentScanNode = scanStartNode; // Wrap around to start
        }
      }
    } else {
      // No response or error - move to next node
      scanSerialPart = 0;
      currentScanNode++;
      if (currentScanNode > scanEndNode) {
        currentScanNode = scanStartNode; // Wrap around to start
      }
    }
  } else {
    // Timeout - move to next node
    scanSerialPart = 0;
    currentScanNode++;
    if (currentScanNode > scanEndNode) {
      currentScanNode = scanStartNode; // Wrap around to start
    }
  }
}

// Device list management functions
void LoadDevices() {
  deviceList.clear();

  if (!LittleFS.exists("/devices.json")) {
    DBG_OUTPUT_PORT.println("No devices.json file, starting with empty device list");
    return;
  }

  File file = LittleFS.open("/devices.json", "r");
  if (!file) {
    DBG_OUTPUT_PORT.println("Failed to open devices.json for reading");
    return;
  }

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();

  if (error) {
    DBG_OUTPUT_PORT.printf("Failed to parse devices.json: %s\n", error.c_str());
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
    if (rxframe.identifier == (0x580 | nodeId) &&
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
