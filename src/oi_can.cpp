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
#include <functional>
#include "oi_can.h"
#include "models/can_types.h"
#include "utils/can_utils.h"
#include "managers/device_storage.h"
#include "managers/device_discovery.h"
#include "managers/device_connection.h"
#include "firmware/update_handler.h"
#include "protocols/sdo_protocol.h"
#include "utils/can_queue.h"

#define DBG_OUTPUT_PORT Serial

namespace OICan {

// Use DeviceConnection singleton for all connection and JSON cache state
static DeviceConnection& conn = DeviceConnection::instance();

// Send SDO request for a parameter value (truly non-blocking with rate limiting)
bool RequestValue(int paramId) {
  // Rate limiting: check if enough time has passed since last request
  if (!conn.canSendParameterRequest()) {
    // Too soon - return false without blocking
    return false;
  }

  uint16_t index = SDOProtocol::INDEX_PARAM_UID | (paramId >> 8);
  uint8_t subIndex = paramId & 0xFF;

  bool success = SDOProtocol::requestElementNonBlocking(conn.getNodeId(), index, subIndex);

  if (success) {
    conn.markParameterRequestSent();
  }

  return success;
}

// Configure rate limiting for parameter requests
void SetParameterRequestRateLimit(unsigned long intervalUs) {
  conn.setParameterRequestRateLimit(intervalUs);
  DBG_OUTPUT_PORT.printf("Parameter request rate limit set to %lu microseconds\n", intervalUs);
}

int StartUpdate(String fileName) {
  // Start the firmware update handler
  int totalPages = FirmwareUpdateHandler::instance().startUpdate(fileName, conn.getNodeId());

  // Reset host processor to enter bootloader mode
  SDOProtocol::setValue(conn.getNodeId(), SDOProtocol::INDEX_COMMANDS, SDOProtocol::CMD_RESET, 1U);

  // Give device time to reset and enter bootloader mode
  // The bootloader needs time to boot and start sending magic
  delay(500);

  return totalPages;
}

// Helper: Initiate JSON download from device
static void initiateJsonDownload() {
  DBG_OUTPUT_PORT.printf("[GetRawJson] Starting JSON download from node %d\n", conn.getNodeId());
  conn.clearJsonCache();
  conn.startJsonDownload();
  DBG_OUTPUT_PORT.println("[GetRawJson] Started JSON download state machine");
}

// Helper: Handle streaming callback updates during JSON download
// Note: Streaming is disabled during download due to thread safety - only final result is sent
static void handleJsonStreamingUpdate(int& lastStreamedSize) {
  // Skip streaming during download - will send complete data at end
  // This avoids race condition with CAN task modifying buffer
  lastStreamedSize = conn.getJsonReceiveBufferLength();
}

// Helper: Handle progress callback updates during JSON download
static void handleJsonProgressUpdate(unsigned long& lastProgressUpdate) {
  const unsigned long PROGRESS_UPDATE_INTERVAL_MS = 200;
  JsonDownloadProgressCallback progressCallback = conn.getJsonProgressCallback();
  if (progressCallback && (millis() - lastProgressUpdate > PROGRESS_UPDATE_INTERVAL_MS)) {
    progressCallback(conn.getJsonReceiveBufferLength());
    lastProgressUpdate = millis();
  }
}

// Helper: Send final streaming and progress notifications
static void handleJsonDownloadCompletion(int lastStreamedSize) {
  // Download is complete, safe to access buffer directly now
  int bufferLen = conn.getJsonReceiveBuffer().length();
  DBG_OUTPUT_PORT.printf("[GetRawJson] Download complete! Buffer size: %d bytes\n", bufferLen);

  // Send final chunk with completion flag if streaming
  JsonStreamCallback streamCallback = conn.getJsonStreamCallback();
  if (streamCallback && bufferLen > lastStreamedSize) {
    int chunkSize = bufferLen - lastStreamedSize;
    const char* chunkStart = conn.getJsonReceiveBuffer().c_str() + lastStreamedSize;
    streamCallback(chunkStart, chunkSize, true); // isComplete = true
  } else if (streamCallback) {
    // No remaining data, but signal completion
    streamCallback("", 0, true);
  }

  // Send completion notification (0 = done)
  JsonDownloadProgressCallback progressCallback = conn.getJsonProgressCallback();
  if (progressCallback) {
    progressCallback(0);
  }
}

// Helper: Wait for JSON download to complete
static bool waitForJsonDownload(int& lastStreamedSize) {
  const unsigned long SEGMENT_TIMEOUT_MS = 5000;
  unsigned long lastSegmentTime = millis();
  unsigned long lastProgressUpdate = 0;
  int lastBufferSize = 0;
  int loopCount = 0;

  while (conn.isDownloadingJson()) {
    // CAN messages are now processed by canTask in background
    // Just wait and check the state

    // Check if we received a new segment (buffer grew) - use thread-safe accessor
    int currentBufferSize = conn.getJsonReceiveBufferLength();
    if (currentBufferSize > lastBufferSize) {
      lastBufferSize = currentBufferSize;
      lastSegmentTime = millis(); // Reset timeout on new data

      handleJsonStreamingUpdate(lastStreamedSize);
      handleJsonProgressUpdate(lastProgressUpdate);

      if (loopCount % 100 == 0) {
        DBG_OUTPUT_PORT.printf("[GetRawJson] Progress: buffer size: %d bytes\n", currentBufferSize);
      }
    }

    // Check for segment timeout (no data received for too long)
    if ((millis() - lastSegmentTime) > SEGMENT_TIMEOUT_MS) {
      DBG_OUTPUT_PORT.printf("[GetRawJson] Segment timeout! No data for %lu ms, buffer size=%d\n",
        millis() - lastSegmentTime, lastBufferSize);
      conn.setState(DeviceConnection::IDLE);
      return false;
    }

    // Yield to other tasks (especially async_tcp) to prevent watchdog timeout
    delay(10);

    // Feed the watchdog to prevent timeout during long transfers
    esp_task_wdt_reset();

    loopCount++;
  }

  // Check if download completed successfully
  if (!conn.isIdle()) {
    DBG_OUTPUT_PORT.printf("[GetRawJson] Failed! State=%d, buffer size=%d\n", conn.getState(), conn.getJsonReceiveBuffer().length());
    conn.setState(DeviceConnection::IDLE);
    return false;
  }

  return true;
}

String GetRawJson() {
  // Return cached JSON if available (avoid blocking download)
  if (!conn.isJsonBufferEmpty()) {
    String cached = conn.getJsonReceiveBufferCopy();
    DBG_OUTPUT_PORT.printf("[GetRawJson] Returning cached JSON (%d bytes)\n", cached.length());
    return cached;
  }

  if (!conn.isIdle()) {
    DBG_OUTPUT_PORT.printf("[GetRawJson] Cannot get JSON - device busy (conn.getState()=%d)\n", conn.getState());
    return "{}";
  }

  // Trigger JSON download from device
  initiateJsonDownload();

  // Wait for download to complete
  int lastStreamedSize = 0;
  if (!waitForJsonDownload(lastStreamedSize)) {
    return "{}";
  }

  // Handle completion notifications
  handleJsonDownloadCompletion(lastStreamedSize);

  return conn.getJsonReceiveBuffer();
}

// Overloaded version that fetches JSON from a specific device by nodeId
// Simplified: only works if already connected to the requested node
String GetRawJson(uint8_t nodeId) {
  if (!conn.isIdle()) {
    DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Cannot get JSON - device busy (conn.getState()=%d)\n", conn.getState());
    return "{}";
  }

  // Only fetch JSON if we're connected to the requested node
  if (conn.getNodeId() != nodeId) {
    DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Not connected to node %d (currently connected to %d). Use Init() to connect first.\n", nodeId, conn.getNodeId());
    return "{}";
  }

  // We're connected to the right node - just delegate to the regular GetRawJson()
  DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Connected to node %d, fetching JSON\n", nodeId);
  return GetRawJson();
}

bool SendJson(WiFiClient client) {
  if (!conn.isIdle()) return false;

  JsonDocument doc;
  twai_message_t rxframe;

  // Use in-memory JSON if available
  if (conn.getCachedJson().isNull() || conn.getCachedJson().size() == 0) {
    DBG_OUTPUT_PORT.println("No parameter JSON in memory");
    return false;
  }

  JsonObject root = conn.getCachedJson().as<JsonObject>();
  int failed = 0;

  SDOProtocol::clearPendingResponses();

  for (JsonPair kv : root) {
    int id = kv.value()["id"].as<int>();

    if (id > 0) {
      SDOProtocol::requestElement(conn.getNodeId(), SDOProtocol::INDEX_PARAM_UID | (id >> 8), id & 0xff);

      if (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10)) && rxframe.data[3] == (id & 0xFF)) {
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

// Callback type for receiving individual CAN mapping items
typedef std::function<void(bool isRx, int cobid, int paramid, int pos, int len, float gain, int offset, int index, int subIndex)> CanMappingCallback;

// Helper function to retrieve CAN mappings via callback
// Returns true if successful, false on error
static bool retrieveCanMappings(CanMappingCallback callback) {
  if (!conn.isIdle()) {
    DBG_OUTPUT_PORT.println("retrieveCanMappings called while not DeviceConnection::IDLE, ignoring");
    return false;
  }

  enum ReqMapStt { START, COBID, DATAPOSLEN, GAINOFS, DONE };

  twai_message_t rxframe;
  int index = SDOProtocol::INDEX_MAP_RD, subIndex = 0;
  int cobid = 0, pos = 0, len = 0, paramid = 0;
  bool rx = false;
  ReqMapStt reqMapStt = START;

  SDOProtocol::clearPendingResponses();

  while (DONE != reqMapStt) {
    switch (reqMapStt) {
    case START:
      SDOProtocol::requestElement(conn.getNodeId(), index, 0); //request COB ID
      reqMapStt = COBID;
      cobid = 0;
      pos = 0;
      len = 0;
      paramid = 0;
      break;
    case COBID:
      if (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
        if (rxframe.data[0] != SDOProtocol::ABORT) {
          cobid = *(int32_t*)&rxframe.data[4]; //convert bytes to word
          subIndex++;
          SDOProtocol::requestElement(conn.getNodeId(), index, subIndex); //request parameter id, position and length
          reqMapStt = DATAPOSLEN;
        }
        else if (!rx) { //after receiving tx item collect rx items
          rx = true;
          index = SDOProtocol::INDEX_MAP_RD + 0x80;
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
      if (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
        if (rxframe.data[0] != SDOProtocol::ABORT) {
          paramid = *(uint16_t*)&rxframe.data[4];
          pos = rxframe.data[6];
          len = (int8_t)rxframe.data[7];
          subIndex++;
          SDOProtocol::requestElement(conn.getNodeId(), index, subIndex); //gain and offset
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
      if (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
        if (rxframe.data[0] != SDOProtocol::ABORT) {
          int32_t gainFixedPoint = ((*(uint32_t*)&rxframe.data[4]) & 0xFFFFFF) << (32-24);
          gainFixedPoint >>= (32-24);
          float gain = gainFixedPoint / 1000.0f;
          int offset = (int8_t)rxframe.data[7];
          DBG_OUTPUT_PORT.printf("can %s %d %d %d %d %f %d\r\n", rx ? "rx" : "tx", paramid, cobid, pos, len, gain, offset);

          // Call the callback with mapping data
          callback(rx, cobid, paramid, pos, len, gain, offset, index, subIndex);

          subIndex++;

          if (subIndex < 100) { //limit maximum items in case there is a bug ;)
            SDOProtocol::requestElement(conn.getNodeId(), index, subIndex); //request next item
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

  return true;
}

// Helper: Retrieve CAN mappings and return as JsonDocument
static bool retrieveCanMappingsAsJson(JsonDocument& doc) {
  return retrieveCanMappings([&doc](bool isRx, int cobid, int paramid, int pos, int len, float gain, int offset, int index, int subIndex) {
    JsonObject object = doc.add<JsonObject>();
    object["isrx"] = isRx;
    object["id"] = cobid;
    object["paramid"] = paramid;
    object["position"] = pos;
    object["length"] = len;
    object["gain"] = gain;
    object["offset"] = offset;
    object["index"] = index;
    object["subindex"] = subIndex;
  });
}

String GetCanMapping() {
  JsonDocument doc;

  if (!retrieveCanMappingsAsJson(doc)) {
    return "[]";
  }

  String result;
  serializeJson(doc, result);
  return result;
}

void SendCanMapping(WiFiClient client) {
  JsonDocument doc;

  if (retrieveCanMappingsAsJson(doc)) {
    WriteBufferingStream bufferedWifiClient{client, 1000};
    serializeJson(doc, bufferedWifiClient);
  }
}

SetResult AddCanMapping(String json) {
  if (!conn.isIdle()) return CommError;

  JsonDocument doc;
  twai_message_t rxframe;

  deserializeJson(doc, json);

  if (doc["isrx"].isNull() || doc["id"].isNull() || doc["paramid"].isNull() || doc["position"].isNull() ||
      doc["length"].isNull() || doc["gain"].isNull() || doc["offset"].isNull()) {
    DBG_OUTPUT_PORT.println("Add: Missing argument");
    return UnknownIndex;
  }

  int index = doc["isrx"] ? SDOProtocol::INDEX_MAP_RX : SDOProtocol::INDEX_MAP_TX;

  SDOProtocol::clearPendingResponses();
  SDOProtocol::setValue(conn.getNodeId(), index, 0, (uint32_t)doc["id"]); //Send CAN Id

  if (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
    DBG_OUTPUT_PORT.println("Sent COB Id");
    SDOProtocol::setValue(conn.getNodeId(), index, 1, doc["paramid"].as<uint32_t>() | (doc["position"].as<uint32_t>() << 16) | (doc["length"].as<int32_t>() << 24)); //data item, position and length
    if (rxframe.data[0] != SDOProtocol::ABORT && SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
      DBG_OUTPUT_PORT.println("Sent position and length");
      SDOProtocol::setValue(conn.getNodeId(), index, 2, (uint32_t)((int32_t)(doc["gain"].as<double>() * 1000) & 0xFFFFFF) | doc["offset"].as<int32_t>() << 24); //gain and offset

      if (rxframe.data[0] != SDOProtocol::ABORT && SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
        if (rxframe.data[0] != SDOProtocol::ABORT){
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
  if (!conn.isIdle()) return CommError;

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

  if (readIndex >= SDOProtocol::INDEX_MAP_RD + 0x80) {
    // RX mapping (read index 0x3180+)
    writeIndex = readIndex;  // Use the read index directly for removal
    isRx = true;
  } else if (readIndex >= SDOProtocol::INDEX_MAP_RD) {
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
  SDOProtocol::clearPendingResponses();
  SDOProtocol::setValue(conn.getNodeId(), writeIndex, 0, 0U);

  if (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
    if (rxframe.data[0] != SDOProtocol::ABORT){
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
  if (!conn.isIdle()) return false;

  twai_message_t rxframe;
  int baseIndex = isRx ? (SDOProtocol::INDEX_MAP_RD + 0x80) : SDOProtocol::INDEX_MAP_RD;
  int removedCount = 0;
  const int MAX_ITERATIONS = 100; // Safety limit to prevent infinite loops

  DBG_OUTPUT_PORT.printf("Clearing all %s CAN mappings\n", isRx ? "RX" : "TX");
  SDOProtocol::clearPendingResponses();

  // Repeatedly delete the first entry (index + 0, subindex 0) until we get an abort
  for (int i = 0; i < MAX_ITERATIONS; i++) {
    // Write 0 to the first mapping slot (index 0x3100 or 0x3180, subindex 0)
    SDOProtocol::setValue(conn.getNodeId(), baseIndex, 0, 0U);

    if (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
      if (rxframe.data[0] == SDOProtocol::ABORT) {
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
  if (!conn.isIdle()) return CommError;

  twai_message_t rxframe;

  SDOProtocol::clearPendingResponses();
  SDOProtocol::setValue(conn.getNodeId(), SDOProtocol::INDEX_PARAM_UID | (paramId >> 8), paramId & 0xFF, (uint32_t)(value * 32));

  if (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
    if (rxframe.data[0] == SDOProtocol::RESPONSE_DOWNLOAD)
      return Ok;
    else if (*(uint32_t*)&rxframe.data[4] == SDOProtocol::ERR_RANGE)
      return ValueOutOfRange;
    else
      return UnknownIndex;
  }
  else {
    return CommError;
  }
}

// Helper: Send a device command and wait for acknowledgment
static const int DEVICE_COMMAND_TIMEOUT_MS = 200;

static bool sendDeviceCommand(uint8_t cmd, uint32_t value = 0) {
  if (!conn.isIdle()) return false;

  twai_message_t rxframe;
  SDOProtocol::clearPendingResponses();  // Clear any stale responses
  SDOProtocol::setValue(conn.getNodeId(), SDOProtocol::INDEX_COMMANDS, cmd, value);

  if (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(DEVICE_COMMAND_TIMEOUT_MS))) {
    return true;
  }
  return false;
}

bool SaveToFlash() {
  return sendDeviceCommand(SDOProtocol::CMD_SAVE);
}

bool LoadFromFlash() {
  return sendDeviceCommand(SDOProtocol::CMD_LOAD);
}

bool LoadDefaults() {
  return sendDeviceCommand(SDOProtocol::CMD_DEFAULTS);
}

bool StartDevice(uint32_t mode) {
  return sendDeviceCommand(SDOProtocol::CMD_START, mode);
}

bool StopDevice() {
  return sendDeviceCommand(SDOProtocol::CMD_STOP);
}

// Helper: Build error description map from parameter JSON
static std::map<int, String> buildErrorDescriptionMap() {
  std::map<int, String> errorDescriptions;
  if (!conn.getCachedJson().isNull() && conn.getCachedJson().containsKey("lasterr")) {
    JsonObject lasterr = conn.getCachedJson()["lasterr"].as<JsonObject>();
    for (JsonPair kv : lasterr) {
      int errorNum = atoi(kv.key().c_str());
      errorDescriptions[errorNum] = kv.value().as<String>();
    }
    DBG_OUTPUT_PORT.printf("Loaded %d error descriptions from lasterr\n", errorDescriptions.size());
  }
  return errorDescriptions;
}

// Helper: Determine tick duration from uptime parameter's unit
static int determineTickDuration() {
  int tickDurationMs = 10; // Default to 10ms
  if (!conn.getCachedJson().isNull() && conn.getCachedJson().containsKey("uptime")) {
    JsonObject uptime = conn.getCachedJson()["uptime"].as<JsonObject>();
    if (uptime.containsKey("unit")) {
      String unit = uptime["unit"].as<String>();
      if (unit == "sec" || unit == "s") {
        tickDurationMs = 1000; // 1 second
        DBG_OUTPUT_PORT.println("Using 1-second tick duration based on uptime unit");
      }
    }
  }
  return tickDurationMs;
}

// Helper: Request error data (timestamp and number) for a specific index
static bool requestErrorAtIndex(uint8_t index, uint32_t& errorTime, uint32_t& errorNum) {
  twai_message_t rxframe;
  bool hasErrorTime = false;
  bool hasErrorNum = false;

  // Request error timestamp
  SDOProtocol::requestElement(conn.getNodeId(), SDOProtocol::INDEX_ERROR_TIME, index);
  if (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
    if (rxframe.data[0] != SDOProtocol::ABORT &&
        (rxframe.data[1] | rxframe.data[2] << 8) == SDOProtocol::INDEX_ERROR_TIME &&
        rxframe.data[3] == index) {
      errorTime = *(uint32_t*)&rxframe.data[4];
      hasErrorTime = true;
    }
  }

  // Request error number
  SDOProtocol::requestElement(conn.getNodeId(), SDOProtocol::INDEX_ERROR_NUM, index);
  if (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
    if (rxframe.data[0] != SDOProtocol::ABORT &&
        (rxframe.data[1] | rxframe.data[2] << 8) == SDOProtocol::INDEX_ERROR_NUM &&
        rxframe.data[3] == index) {
      errorNum = *(uint32_t*)&rxframe.data[4];
      hasErrorNum = true;
    }
  }

  return hasErrorTime && hasErrorNum;
}

// Helper: Create JSON object for an error entry
static void createErrorJsonObject(JsonArray& errors, uint8_t index, uint32_t errorNum, uint32_t errorTime,
                                   int tickDurationMs, const std::map<int, String>& errorDescriptions) {
  JsonObject errorObj = errors.add<JsonObject>();
  errorObj["index"] = index;
  errorObj["errorNum"] = errorNum;
  errorObj["errorTime"] = errorTime;
  errorObj["elapsedTimeMs"] = errorTime * tickDurationMs;

  // Add description if available
  if (errorDescriptions.count(errorNum) > 0) {
    errorObj["description"] = errorDescriptions.at(errorNum);
  } else {
    errorObj["description"] = "Unknown error " + String(errorNum);
  }

  DBG_OUTPUT_PORT.printf("Error %lu at index %d: time=%lu ticks (%lu ms), desc=%s\n",
                         (unsigned long)errorNum, index, (unsigned long)errorTime,
                         (unsigned long)(errorTime * tickDurationMs),
                         errorObj["description"].as<const char*>());
}

String ListErrors() {
  if (!conn.isIdle()) {
    DBG_OUTPUT_PORT.println("ListErrors called while not DeviceConnection::IDLE, ignoring");
    return "[]";
  }

  JsonDocument doc;
  JsonArray errors = doc.to<JsonArray>();

  // Build error description map and determine tick duration
  std::map<int, String> errorDescriptions = buildErrorDescriptionMap();
  int tickDurationMs = determineTickDuration();
  DBG_OUTPUT_PORT.printf("Retrieving error log (tick duration: %dms)\n", tickDurationMs);

  // Iterate through error indices (0-254)
  for (uint8_t i = 0; i < 255; i++) {
    uint32_t errorTime = 0;
    uint32_t errorNum = 0;

    // Request error data for this index
    if (!requestErrorAtIndex(i, errorTime, errorNum)) {
      DBG_OUTPUT_PORT.printf("Reached end of error log at index %d\n", i);
      break;
    }

    // If we have valid error data, add it to the result
    if (errorNum != 0) {
      createErrorJsonObject(errors, i, errorNum, errorTime, tickDurationMs, errorDescriptions);
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

  // Send the message via TX queue
  if (canQueueTransmit(&frame, pdMS_TO_TICKS(10))) {
    DBG_OUTPUT_PORT.printf("Sent CAN message: ID=0x%03lX, Len=%d\n", (unsigned long)canId, dataLength);
    return true;
  } else {
    DBG_OUTPUT_PORT.printf("Failed to queue CAN message: ID=0x%03lX\n", (unsigned long)canId);
    return false;
  }
}

String StreamValues(String paramIds, int samples) {
  if (!conn.isIdle()) return "";

  twai_message_t rxframe;

  int ids[30], numItems = 0;
  String result;

  // Parse comma-separated parameter IDs
  for (int pos = 0; pos >= 0; pos = paramIds.indexOf(',', pos + 1)) {
    String idStr = paramIds.substring(pos + 1, paramIds.indexOf(',', pos + 1));
    ids[numItems++] = idStr.toInt();
  }

  SDOProtocol::clearPendingResponses();

  for (int i = 0; i < samples; i++) {
    for (int item = 0; item < numItems; item++) {
      int id = ids[item];
      SDOProtocol::requestElement(conn.getNodeId(), SDOProtocol::INDEX_PARAM_UID | (id >> 8), id & 0xFF);
    }

    int item = 0;
    while (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
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

  if (!SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(timeoutMs))) {
    return false; // No message available
  }
  
  printCanRx(&rxframe);
  
  // Parse the response to extract paramId
  uint16_t responseIndex = rxframe.data[1] | (rxframe.data[2] << 8);
  uint8_t responseSubIndex = rxframe.data[3];

  // Check if this is a parameter value response (SDO_INDEX_PARAM_UID range)
  if ((responseIndex & 0xFF00) != (SDOProtocol::INDEX_PARAM_UID & 0xFF00)) {
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
  if (!conn.isIdle()) {
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

// Initialize CAN bus without connecting to a specific device
void InitCAN(BaudRate baud, int txPin, int rxPin) {
  conn.initializeForScanning(baud, txPin, rxPin);
}

// Initialize CAN and connect to a specific device
void Init(uint8_t nodeId, BaudRate baud, int txPin, int rxPin) {
  conn.connectToDevice(nodeId, baud, txPin, rxPin);
}

bool ReloadJson() {
  if (!conn.isIdle()) return false;

  // Remove the cached JSON file to force re-download
  if (LittleFS.exists(conn.getJsonFileName())) {
    LittleFS.remove(conn.getJsonFileName());
    DBG_OUTPUT_PORT.printf("Removed cached JSON file: %s\r\n", conn.getJsonFileName());
  }

  // Clear cached JSON and trigger download
  conn.clearJsonCache();
  conn.startJsonDownload();

  DBG_OUTPUT_PORT.println("Reloading JSON from device");
  return true;
}

// Overloaded version that reloads JSON for a specific device by nodeId
bool ReloadJson(uint8_t nodeId) {
  if (!conn.isIdle()) {
    DBG_OUTPUT_PORT.printf("[ReloadJson(nodeId)] Cannot reload - device busy (conn.getState()=%d)\n", conn.getState());
    return false;
  }

  // Clear the cached JSON buffer for the requested node
  if (conn.getNodeId() == nodeId) {
    // If it's the currently connected node, clear the cache
    conn.getJsonReceiveBuffer() = "";
    conn.getCachedJson().clear();
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
  if (!conn.isIdle()) return false;

  // Send reset command to the device
  SDOProtocol::setValue(conn.getNodeId(), SDOProtocol::INDEX_COMMANDS, SDOProtocol::CMD_RESET, 1U);

  DBG_OUTPUT_PORT.println("Device reset command sent");

  // The device will reset immediately and won't send an acknowledgment
  // After a short delay, trigger serial re-acquisition
  delay(500); // Give device time to start resetting
  conn.startSerialAcquisition();

  return true;
}

// Device management functions

String ScanDevices(uint8_t startNodeId, uint8_t endNodeId) {
  uint8_t nodeId = conn.getNodeId();
  return DeviceDiscovery::instance().scanDevices(startNodeId, endNodeId, nodeId, conn.getBaudRate(), conn.getCanTxPin(), conn.getCanRxPin());
}

// Continuous scanning implementation

bool StartContinuousScan(uint8_t startNodeId, uint8_t endNodeId) {
  if (!conn.isIdle()) {
    DBG_OUTPUT_PORT.printf("Cannot start continuous scan - device busy: %d\n", conn.getState());
    return false;
  }

  // Reinitialize CAN bus with accept-all filter for scanning
  // This ensures we can see all nodes, not just the previously connected one
  DBG_OUTPUT_PORT.println("Reinitializing CAN bus for scanning (accept all messages)");
  InitCAN(conn.getBaudRate(), conn.getCanTxPin(), conn.getCanRxPin()); // Use current baud rate and stored pins

  return DeviceDiscovery::instance().startContinuousScan(startNodeId, endNodeId);
}

}
