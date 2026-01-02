/*
 * This file is part of the openinverter web interface
 *
 * Copyright (C) 2023 Johannes Huebner <dev@johanneshuebner.com>
 * Copyright (C) 2025 Nishanth Samala <contact@outlandnish.com>
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
#include "oi_can.h"

#include <ArduinoJson.h>

#include <functional>
#include <map>
#include <vector>

#include <FS.h>
#include <StreamUtils.h>

#include "driver/gpio.h"
#include "driver/twai.h"
#include "esp_task_wdt.h"
#include "firmware/update_handler.h"

#include "managers/device_connection.h"
#include "managers/device_discovery.h"
#include "managers/device_storage.h"
#include "models/can_types.h"
#include "protocols/sdo_protocol.h"
#include "utils/can_queue.h"
#include "utils/can_utils.h"

#define DBG_OUTPUT_PORT Serial

namespace OICan {

// Use DeviceConnection singleton for all connection and JSON cache state
static DeviceConnection& conn = DeviceConnection::instance();

// Helper: Extract parameter value from SDO response frame
// Parameters are stored as signed fixed-point with scale of 32
static double extractParameterValue(const twai_message_t& frame) {
  return ((double)*(int32_t*)&frame.data[4]) / 32.0;
}

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

// GetRawJson - Returns cached JSON data (non-blocking)
// JSON download is now handled asynchronously via DeviceConnection::startJsonDownloadAsync()
// and the result is delivered via EVT_JSON_READY event
String GetRawJson() {
  // Return cached JSON if available
  if (!conn.isJsonBufferEmpty()) {
    String cached = conn.getJsonReceiveBufferCopy();
    DBG_OUTPUT_PORT.printf("[GetRawJson] Returning cached JSON (%d bytes)\n", cached.length());
    return cached;
  }

  // No cached data - return empty
  // Callers should use startJsonDownloadAsync() for async download
  DBG_OUTPUT_PORT.println("[GetRawJson] No cached JSON available");
  return "{}";
}

// Overloaded version that checks node ID first
String GetRawJson(uint8_t nodeId) {
  // Only return JSON if we're connected to the requested node
  if (conn.getNodeId() != nodeId) {
    DBG_OUTPUT_PORT.printf("[GetRawJson(nodeId)] Not connected to node %d (currently connected to %d)\n", nodeId,
                           conn.getNodeId());
    return "{}";
  }

  return GetRawJson();
}

// Callback type for receiving parameter values during iteration
typedef std::function<void(const char* key, int id, double value)> ParameterValueCallback;

// Helper: Iterate through cached parameters and fetch current values
// Returns number of failed requests (0 = all succeeded)
static int iterateParameterValues(ParameterValueCallback callback) {
  if (conn.getCachedJson().isNull() || conn.getCachedJson().size() == 0) {
    DBG_OUTPUT_PORT.println("No parameter JSON in memory");
    return -1;  // Error: no cached JSON
  }

  JsonObject root = conn.getCachedJson().as<JsonObject>();
  twai_message_t rxframe;
  int failed = 0;

  SDOProtocol::clearPendingResponses();

  for (JsonPair kv : root) {
    int id = kv.value()["id"].as<int>();

    if (id > 0) {
      SDOProtocol::requestElement(conn.getNodeId(), SDOProtocol::INDEX_PARAM_UID | (id >> 8), id & 0xff);

      if (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10)) && rxframe.data[3] == (id & 0xFF)) {
        callback(kv.key().c_str(), id, extractParameterValue(rxframe));
      } else {
        failed++;
      }
    }
  }

  return failed;
}

bool SendJson(WiFiClient client) {
  if (!conn.isIdle())
    return false;

  JsonDocument doc;

  int failed = iterateParameterValues([&doc](const char* key, int id, double value) { doc[key]["value"] = value; });

  if (failed < 0)
    return false;  // No cached JSON

  if (failed < 5) {
    WriteBufferingStream bufferedWifiClient{client, 1000};
    serializeJson(doc, bufferedWifiClient);
  }
  return failed < 5;
}

// Callback type for receiving individual CAN mapping items
typedef std::function<void(const CanMappingData& mapping)> CanMappingCallback;

// Helper: Parse 24-bit signed fixed-point gain from SDO response
static float parseGainFromResponse(const twai_message_t& frame) {
  // Extract 24-bit signed value and convert to float
  int32_t gainFixedPoint = ((*(uint32_t*)&frame.data[4]) & 0xFFFFFF) << 8;
  gainFixedPoint >>= 8;  // Sign-extend
  return gainFixedPoint / 1000.0f;
}

// Helper: Request SDO element and wait for non-abort response
// Returns true if got valid response, false on timeout or abort
static bool requestMappingElement(uint16_t index, uint8_t subIndex, twai_message_t& response) {
  SDOProtocol::requestElement(conn.getNodeId(), index, subIndex);
  if (!SDOProtocol::waitForResponse(&response, pdMS_TO_TICKS(10))) {
    return false;  // Timeout
  }
  return response.data[0] != SDOProtocol::ABORT;
}

// Helper: Retrieve all mappings for one direction (TX or RX)
// Returns true to continue to next direction, false to stop
static bool retrieveMappingsForDirection(bool isRx, uint16_t baseIndex, CanMappingCallback callback) {
  twai_message_t rxframe;
  uint16_t index = baseIndex;
  const int MAX_ITEMS_PER_MESSAGE = 100;

  while (true) {
    // Request COB ID (subindex 0)
    if (!requestMappingElement(index, 0, rxframe)) {
      return true;  // No more messages in this direction, continue to next
    }

    int cobId = *(int32_t*)&rxframe.data[4];
    uint8_t subIndex = 1;

    // Collect all items in this message
    while (subIndex < MAX_ITEMS_PER_MESSAGE) {
      // Request param ID, position, length
      if (!requestMappingElement(index, subIndex, rxframe)) {
        DBG_OUTPUT_PORT.println("Mapping received, moving to next");
        break;  // Move to next message
      }

      int paramId = *(uint16_t*)&rxframe.data[4];
      int position = rxframe.data[6];
      int length = (int8_t)rxframe.data[7];
      subIndex++;

      // Request gain and offset
      if (!requestMappingElement(index, subIndex, rxframe)) {
        return false;  // Unexpected abort
      }

      float gain = parseGainFromResponse(rxframe);
      int offset = (int8_t)rxframe.data[7];

      DBG_OUTPUT_PORT.printf("can %s %d %d %d %d %f %d\r\n", isRx ? "rx" : "tx", paramId, cobId, position, length, gain,
                             offset);

      // Call callback with mapping data
      callback({isRx, cobId, paramId, position, length, gain, offset, index, subIndex});

      subIndex++;
    }

    index++;  // Move to next message
  }
}

// Main function: Retrieve all CAN mappings (TX then RX) via callback
static bool retrieveCanMappings(CanMappingCallback callback) {
  if (!conn.isIdle()) {
    DBG_OUTPUT_PORT.println("retrieveCanMappings called while not DeviceConnection::IDLE, ignoring");
    return false;
  }

  SDOProtocol::clearPendingResponses();

  // Retrieve TX mappings (0x3100+)
  retrieveMappingsForDirection(false, SDOProtocol::INDEX_MAP_RD, callback);

  // Retrieve RX mappings (0x3180+)
  DBG_OUTPUT_PORT.println("Getting RX items");
  retrieveMappingsForDirection(true, SDOProtocol::INDEX_MAP_RD + 0x80, callback);

  return true;
}

// Helper: Retrieve CAN mappings and return as JsonDocument
static bool retrieveCanMappingsAsJson(JsonDocument& doc) {
  return retrieveCanMappings([&doc](const CanMappingData& m) {
    JsonObject obj = doc.add<JsonObject>();
    obj["isrx"] = m.isRx;
    obj["id"] = m.cobId;
    obj["paramid"] = m.paramId;
    obj["position"] = m.position;
    obj["length"] = m.length;
    obj["gain"] = m.gain;
    obj["offset"] = m.offset;
    obj["index"] = m.sdoIndex;
    obj["subindex"] = m.sdoSubIndex;
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
  if (!conn.isIdle())
    return CommError;

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
  SDOProtocol::setValue(conn.getNodeId(), index, 0, (uint32_t)doc["id"]);  // Send CAN Id

  if (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
    DBG_OUTPUT_PORT.println("Sent COB Id");

    uint32_t paramId = doc["paramid"].as<uint32_t>();
    uint32_t position = doc["position"].as<uint32_t>();
    int32_t length = doc["length"].as<int32_t>();
    uint32_t paramPositionLength = paramId | (position << 16) | (length << 24);

    SDOProtocol::setValue(conn.getNodeId(), index, 1, paramPositionLength);
    if (rxframe.data[0] != SDOProtocol::ABORT && SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
      DBG_OUTPUT_PORT.println("Sent position and length");

      int32_t gainScaled = (int32_t)(doc["gain"].as<double>() * 1000) & 0xFFFFFF;
      int32_t offset = doc["offset"].as<int32_t>();
      uint32_t gainOffset = (uint32_t)gainScaled | (offset << 24);

      SDOProtocol::setValue(conn.getNodeId(), index, 2, gainOffset);

      if (rxframe.data[0] != SDOProtocol::ABORT && SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
        if (rxframe.data[0] != SDOProtocol::ABORT) {
          DBG_OUTPUT_PORT.println("Sent gain and offset -> map successful");
          return Ok;
        }
      }
    }
  }

  DBG_OUTPUT_PORT.println("Mapping failed");

  return CommError;
}

SetResult RemoveCanMapping(String json) {
  if (!conn.isIdle())
    return CommError;

  JsonDocument doc;
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

  DBG_OUTPUT_PORT.printf("Removing %s mapping at index 0x%lX, subindex 0\n", isRx ? "RX" : "TX",
                         (unsigned long)writeIndex);

  // Write 0 to subindex 0 (COB ID) to remove the entire mapping
  twai_message_t rxframe;
  if (SDOProtocol::writeAndWait(conn.getNodeId(), writeIndex, 0, 0U, &rxframe)) {
    DBG_OUTPUT_PORT.println("Item removed");
    return Ok;
  }

  // writeAndWait returns false on timeout or abort
  if (rxframe.data[0] == SDOProtocol::ABORT) {
    DBG_OUTPUT_PORT.println("Invalid item index/subindex");
    return UnknownIndex;
  }

  DBG_OUTPUT_PORT.println("Comm Error");
  return CommError;
}

bool ClearCanMap(bool isRx, ClearMapProgressCallback onProgress) {
  if (!conn.isIdle())
    return false;

  twai_message_t rxframe;
  int baseIndex = isRx ? (SDOProtocol::INDEX_MAP_RD + 0x80) : SDOProtocol::INDEX_MAP_RD;
  int removedCount = 0;
  const int MAX_ITERATIONS = 100;  // Safety limit to prevent infinite loops

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

        // Notify progress callback if provided
        if (onProgress) {
          onProgress(removedCount);
        }
      }
    } else {
      // Communication timeout
      DBG_OUTPUT_PORT.printf("Communication timeout while clearing %s mappings\n", isRx ? "RX" : "TX");
      return false;
    }
  }

  // Hit maximum iterations - probably a bug
  DBG_OUTPUT_PORT.printf("Warning: Hit maximum iterations (%d) while clearing %s mappings\n", MAX_ITERATIONS,
                         isRx ? "RX" : "TX");
  return false;
}

SetResult SetValue(int paramId, double value) {
  if (!conn.isIdle())
    return CommError;

  twai_message_t rxframe;
  uint16_t index = SDOProtocol::INDEX_PARAM_UID | (paramId >> 8);
  uint8_t subIndex = paramId & 0xFF;

  if (!SDOProtocol::writeAndWait(conn.getNodeId(), index, subIndex, (uint32_t)(value * 32), &rxframe)) {
    // Check if we got a response but it was an abort
    if (rxframe.data[0] == SDOProtocol::ABORT) {
      if (*(uint32_t*)&rxframe.data[4] == SDOProtocol::ERR_RANGE)
        return ValueOutOfRange;
      else
        return UnknownIndex;
    }
    return CommError;
  }

  return Ok;
}

// Helper: Send a device command and wait for acknowledgment
static const int DEVICE_COMMAND_TIMEOUT_MS = 200;

static bool sendDeviceCommand(uint8_t cmd, uint32_t value = 0) {
  if (!conn.isIdle())
    return false;

  return SDOProtocol::writeAndWait(conn.getNodeId(), SDOProtocol::INDEX_COMMANDS, cmd, value,
                                   pdMS_TO_TICKS(DEVICE_COMMAND_TIMEOUT_MS));
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
  int tickDurationMs = 10;  // Default to 10ms
  if (!conn.getCachedJson().isNull() && conn.getCachedJson().containsKey("uptime")) {
    JsonObject uptime = conn.getCachedJson()["uptime"].as<JsonObject>();
    if (uptime.containsKey("unit")) {
      String unit = uptime["unit"].as<String>();
      if (unit == "sec" || unit == "s") {
        tickDurationMs = 1000;  // 1 second
        DBG_OUTPUT_PORT.println("Using 1-second tick duration based on uptime unit");
      }
    }
  }
  return tickDurationMs;
}

// Helper: Request error data (timestamp and number) for a specific index
static bool requestErrorAtIndex(uint8_t index, uint32_t& errorTime, uint32_t& errorNum) {
  // Request error timestamp
  if (!SDOProtocol::requestValue(conn.getNodeId(), SDOProtocol::INDEX_ERROR_TIME, index, &errorTime)) {
    return false;
  }

  // Request error number
  if (!SDOProtocol::requestValue(conn.getNodeId(), SDOProtocol::INDEX_ERROR_NUM, index, &errorNum)) {
    return false;
  }

  return true;
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

  DBG_OUTPUT_PORT.printf("Error %lu at index %d: time=%lu ticks (%lu ms), desc=%s\n", (unsigned long)errorNum, index,
                         (unsigned long)errorTime, (unsigned long)(errorTime * tickDurationMs),
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
  if (dataLength > 8)
    return false;  // CAN data cannot be longer than 8 bytes

  twai_message_t frame;
  frame.identifier = canId;
  frame.data_length_code = dataLength;
  frame.flags = 0;  // Standard frame, no RTR

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

// Helper: Parse comma-separated parameter IDs from string
// Returns vector of parameter IDs
static std::vector<int> parseParameterIds(const String& paramIds) {
  std::vector<int> ids;
  ids.reserve(30);  // Reasonable initial capacity

  for (int pos = 0; pos >= 0; pos = paramIds.indexOf(',', pos + 1)) {
    String idStr = paramIds.substring(pos + 1, paramIds.indexOf(',', pos + 1));
    ids.push_back(idStr.toInt());
  }

  return ids;
}

// Callback for streaming parameter values
// sampleIndex: which sample (0 to samples-1)
// itemIndex: which parameter in this sample (0 to numItems-1)
// numItems: total number of parameters per sample
// value: the parameter value (or 0 on error)
typedef std::function<void(int sampleIndex, int itemIndex, int numItems, double value)> StreamValueCallback;

// Helper: Stream parameter values with callback
// Returns true if completed successfully
static bool streamParameterValues(const std::vector<int>& ids, int samples, StreamValueCallback callback) {
  twai_message_t rxframe;
  int numItems = ids.size();

  SDOProtocol::clearPendingResponses();

  for (int sampleIdx = 0; sampleIdx < samples; sampleIdx++) {
    // Send all requests for this sample
    for (int itemIdx = 0; itemIdx < numItems; itemIdx++) {
      int id = ids[itemIdx];
      SDOProtocol::requestElement(conn.getNodeId(), SDOProtocol::INDEX_PARAM_UID | (id >> 8), id & 0xFF);
    }

    // Collect all responses for this sample
    int itemIdx = 0;
    while (SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(10))) {
      double value = 0;
      if (rxframe.data[0] != 0x80) {
        int receivedItem = (rxframe.data[1] << 8) + rxframe.data[3];
        if (receivedItem == ids[itemIdx]) {
          value = extractParameterValue(rxframe);
        }
      }
      callback(sampleIdx, itemIdx, numItems, value);
      itemIdx++;
    }
  }

  return true;
}

String StreamValues(String paramIds, int samples) {
  if (!conn.isIdle())
    return "";

  auto ids = parseParameterIds(paramIds);
  String result;

  streamParameterValues(ids, samples, [&result](int sampleIdx, int itemIdx, int numItems, double value) {
    if (itemIdx > 0)
      result += ",";
    result += String(value, 2);
    if (itemIdx == numItems - 1)
      result += "\r\n";
  });

  return result;
}

// Try to receive and parse a parameter value response (non-blocking)
// Returns true if a valid response was received, false if no response available
bool TryGetValueResponse(int& outParamId, double& outValue, int timeoutMs) {
  twai_message_t rxframe;

  if (!SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(timeoutMs))) {
    return false;  // No message available
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

  outParamId = paramId;
  outValue = extractParameterValue(rxframe);
  return true;
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
  if (!conn.isIdle())
    return false;

  // Remove the cached JSON file to force re-download
  DeviceStorage::removeJsonCache(conn.getSerial());

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
  if (!conn.isIdle())
    return false;

  // Send reset command to the device
  SDOProtocol::setValue(conn.getNodeId(), SDOProtocol::INDEX_COMMANDS, SDOProtocol::CMD_RESET, 1U);

  DBG_OUTPUT_PORT.println("Device reset command sent");

  // The device will reset immediately and won't send an acknowledgment
  // After a short delay, trigger serial re-acquisition
  delay(500);  // Give device time to start resetting
  conn.startSerialAcquisition();

  return true;
}

// Device management functions

String ScanDevices(uint8_t startNodeId, uint8_t endNodeId) {
  uint8_t nodeId = conn.getNodeId();
  return DeviceDiscovery::instance().scanDevices(startNodeId, endNodeId, nodeId, conn.getBaudRate(), conn.getCanTxPin(),
                                                 conn.getCanRxPin());
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
  InitCAN(conn.getBaudRate(), conn.getCanTxPin(), conn.getCanRxPin());  // Use current baud rate and stored pins

  return DeviceDiscovery::instance().startContinuousScan(startNodeId, endNodeId);
}

}  // namespace OICan
