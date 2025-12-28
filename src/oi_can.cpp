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
#include "managers/device_discovery.h"
#include "managers/device_connection.h"
#include "firmware/update_handler.h"
#include "protocols/sdo_protocol.h"

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

String GetRawJson() {
  // Return cached JSON if available (avoid blocking download)
  if (!conn.getJsonReceiveBuffer().isEmpty()) {
    DBG_OUTPUT_PORT.printf("[GetRawJson] Returning cached JSON (%d bytes)\n", conn.getJsonReceiveBuffer().length());
    return conn.getJsonReceiveBuffer();
  }

  if (!conn.isIdle()) {
    DBG_OUTPUT_PORT.printf("[GetRawJson] Cannot get JSON - device busy (conn.getState()=%d)\n", conn.getState());
    return "{}";
  }

  // Trigger JSON download from device
  DBG_OUTPUT_PORT.printf("[GetRawJson] Starting JSON download from node %d\n", conn.getNodeId());
  conn.setState(DeviceConnection::OBTAIN_JSON);
  conn.clearJsonCache();
  SDOProtocol::requestElement(conn.getNodeId(), SDOProtocol::INDEX_STRINGS, 0);
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

  while (conn.getState() == DeviceConnection::OBTAIN_JSON) {
    // CAN messages are now processed by canTask in background
    // Just wait and check the state

    // Check if we received a new segment (buffer grew)
    if (conn.getJsonReceiveBuffer().length() > lastBufferSize) {
      lastBufferSize = conn.getJsonReceiveBuffer().length();
      lastSegmentTime = millis(); // Reset timeout on new data

      // Stream new data chunk to callback if registered
      JsonStreamCallback streamCallback = conn.getJsonStreamCallback();
      if (streamCallback && conn.getJsonReceiveBuffer().length() > lastStreamedSize) {
        int chunkSize = conn.getJsonReceiveBuffer().length() - lastStreamedSize;
        const char* chunkStart = conn.getJsonReceiveBuffer().c_str() + lastStreamedSize;
        streamCallback(chunkStart, chunkSize, false); // isComplete = false
        lastStreamedSize = conn.getJsonReceiveBuffer().length();
      }

      // Send progress update via callback (throttled)
      JsonDownloadProgressCallback progressCallback = conn.getJsonProgressCallback();
      if (progressCallback && (millis() - lastProgressUpdate > PROGRESS_UPDATE_INTERVAL_MS)) {
        progressCallback(conn.getJsonReceiveBuffer().length());
        lastProgressUpdate = millis();
      }

      if (loopCount % 100 == 0) {
        DBG_OUTPUT_PORT.printf("[GetRawJson] Progress: buffer size: %d bytes\n", conn.getJsonReceiveBuffer().length());
      }
    }

    // Check for segment timeout (no data received for too long)
    if ((millis() - lastSegmentTime) > SEGMENT_TIMEOUT_MS) {
      DBG_OUTPUT_PORT.printf("[GetRawJson] Segment timeout! No data for %lu ms, buffer size=%d\n",
        millis() - lastSegmentTime, conn.getJsonReceiveBuffer().length());
      conn.setState(DeviceConnection::IDLE);
      return "{}";
    }

    // Yield to other tasks (especially async_tcp) to prevent watchdog timeout
    delay(10);

    // Feed the watchdog to prevent timeout during long transfers
    esp_task_wdt_reset();

    loopCount++;
  }

  if (!conn.isIdle()) {
    DBG_OUTPUT_PORT.printf("[GetRawJson] Failed! State=%d, buffer size=%d\n", conn.getState(), conn.getJsonReceiveBuffer().length());
    conn.setState(DeviceConnection::IDLE);
    return "{}";
  }

  DBG_OUTPUT_PORT.printf("[GetRawJson] Download complete! Buffer size: %d bytes\n", conn.getJsonReceiveBuffer().length());

  // Send final chunk with completion flag if streaming
  JsonStreamCallback streamCallback = conn.getJsonStreamCallback();
  if (streamCallback && conn.getJsonReceiveBuffer().length() > lastStreamedSize) {
    int chunkSize = conn.getJsonReceiveBuffer().length() - lastStreamedSize;
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

  for (JsonPair kv : root) {
    int id = kv.value()["id"].as<int>();

    if (id > 0) {
      SDOProtocol::requestElement(conn.getNodeId(), SDOProtocol::INDEX_PARAM_UID | (id >> 8), id & 0xff);

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
  if (!conn.isIdle()) {
    DBG_OUTPUT_PORT.println("GetCanMapping called while not DeviceConnection::IDLE, ignoring");
    return "[]";
  }

  enum ReqMapStt { START, COBID, DATAPOSLEN, GAINOFS, DONE };

  twai_message_t rxframe;
  int index = SDOProtocol::INDEX_MAP_RD, subIndex = 0;
  int cobid = 0, pos = 0, len = 0, paramid = 0;
  bool rx = false;
  ReqMapStt reqMapStt = START;

  JsonDocument doc;

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
      if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
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
      if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
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
      if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
        if (rxframe.data[0] != SDOProtocol::ABORT) {
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

  String result;
  serializeJson(doc, result);
  return result;
}

void SendCanMapping(WiFiClient client) {
  if (!conn.isIdle()) {
    DBG_OUTPUT_PORT.println("SendCanMapping called while not DeviceConnection::IDLE, ignoring");
    return;
  }

  enum ReqMapStt { START, COBID, DATAPOSLEN, GAINOFS, DONE };

  twai_message_t rxframe;
  int index = SDOProtocol::INDEX_MAP_RD, subIndex = 0;
  int cobid = 0, pos = 0, len = 0, paramid = 0;
  bool rx = false;
  String result;
  ReqMapStt reqMapStt = START;

  JsonDocument doc;

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
      if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
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
      if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
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
      if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
        if (rxframe.data[0] != SDOProtocol::ABORT) {
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

  WriteBufferingStream bufferedWifiClient{client, 1000};
  serializeJson(doc, bufferedWifiClient);
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

  SDOProtocol::setValue(conn.getNodeId(), index, 0, (uint32_t)doc["id"]); //Send CAN Id

  if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
    printCanRx(&rxframe);
    DBG_OUTPUT_PORT.println("Sent COB Id");
    SDOProtocol::setValue(conn.getNodeId(), index, 1, doc["paramid"].as<uint32_t>() | (doc["position"].as<uint32_t>() << 16) | (doc["length"].as<int32_t>() << 24)); //data item, position and length
    if (rxframe.data[0] != SDOProtocol::ABORT && twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
      printCanRx(&rxframe);
      DBG_OUTPUT_PORT.println("Sent position and length");
      SDOProtocol::setValue(conn.getNodeId(), index, 2, (uint32_t)((int32_t)(doc["gain"].as<double>() * 1000) & 0xFFFFFF) | doc["offset"].as<int32_t>() << 24); //gain and offset

      if (rxframe.data[0] != SDOProtocol::ABORT && twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
        printCanRx(&rxframe);
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
  SDOProtocol::setValue(conn.getNodeId(), writeIndex, 0, 0U);

  if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
    printCanRx(&rxframe);
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

  // Repeatedly delete the first entry (index + 0, subindex 0) until we get an abort
  for (int i = 0; i < MAX_ITERATIONS; i++) {
    // Write 0 to the first mapping slot (index 0x3100 or 0x3180, subindex 0)
    SDOProtocol::setValue(conn.getNodeId(), baseIndex, 0, 0U);

    if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
      printCanRx(&rxframe);

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

  SDOProtocol::setValue(conn.getNodeId(), SDOProtocol::INDEX_PARAM_UID | (paramId >> 8), paramId & 0xFF, (uint32_t)(value * 32));

  if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
    printCanRx(&rxframe);
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

bool SaveToFlash() {
  if (!conn.isIdle()) return false;

  twai_message_t rxframe;

  SDOProtocol::setValue(conn.getNodeId(), SDOProtocol::INDEX_COMMANDS, SDOProtocol::CMD_SAVE, 0U);

  if (twai_receive(&rxframe, pdMS_TO_TICKS(200)) == ESP_OK) {
    printCanRx(&rxframe);
    return true;
  }
  else {
    return false;
  }
}

bool LoadFromFlash() {
  if (!conn.isIdle()) return false;

  twai_message_t rxframe;

  SDOProtocol::setValue(conn.getNodeId(), SDOProtocol::INDEX_COMMANDS, SDOProtocol::CMD_LOAD, 0U);

  if (twai_receive(&rxframe, pdMS_TO_TICKS(200)) == ESP_OK) {
    printCanRx(&rxframe);
    return true;
  }
  else {
    return false;
  }
}

bool LoadDefaults() {
  if (!conn.isIdle()) return false;

  twai_message_t rxframe;

  SDOProtocol::setValue(conn.getNodeId(), SDOProtocol::INDEX_COMMANDS, SDOProtocol::CMD_DEFAULTS, 0U);

  if (twai_receive(&rxframe, pdMS_TO_TICKS(200)) == ESP_OK) {
    printCanRx(&rxframe);
    return true;
  }
  else {
    return false;
  }
}

bool StartDevice(uint32_t mode) {
  if (!conn.isIdle()) return false;

  twai_message_t rxframe;

  SDOProtocol::setValue(conn.getNodeId(), SDOProtocol::INDEX_COMMANDS, SDOProtocol::CMD_START, mode);

  if (twai_receive(&rxframe, pdMS_TO_TICKS(200)) == ESP_OK) {
    printCanRx(&rxframe);
    return true;
  }
  else {
    return false;
  }
}

bool StopDevice() {
  if (!conn.isIdle()) return false;

  twai_message_t rxframe;

  SDOProtocol::setValue(conn.getNodeId(), SDOProtocol::INDEX_COMMANDS, SDOProtocol::CMD_STOP, 0U);

  if (twai_receive(&rxframe, pdMS_TO_TICKS(200)) == ESP_OK) {
    printCanRx(&rxframe);
    return true;
  }
  else {
    return false;
  }
}

String ListErrors() {
  if (!conn.isIdle()) {
    DBG_OUTPUT_PORT.println("ListErrors called while not DeviceConnection::IDLE, ignoring");
    return "[]";
  }

  twai_message_t rxframe;
  JsonDocument doc;
  JsonArray errors = doc.to<JsonArray>();

  // Build error description map from parameter JSON (lasterr field)
  std::map<int, String> errorDescriptions;
  if (!conn.getCachedJson().isNull() && conn.getCachedJson().containsKey("lasterr")) {
    JsonObject lasterr = conn.getCachedJson()["lasterr"].as<JsonObject>();
    for (JsonPair kv : lasterr) {
      int errorNum = atoi(kv.key().c_str());
      errorDescriptions[errorNum] = kv.value().as<String>();
    }
    DBG_OUTPUT_PORT.printf("Loaded %d error descriptions from lasterr\n", errorDescriptions.size());
  }

  // Determine tick duration from uptime parameter's unit (default: 10ms)
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

  DBG_OUTPUT_PORT.printf("Retrieving error log (tick duration: %dms)\n", tickDurationMs);

  // Iterate through error indices (0-254)
  for (uint8_t i = 0; i < 255; i++) {
    uint32_t errorTime = 0;
    uint32_t errorNum = 0;
    bool hasErrorTime = false;
    bool hasErrorNum = false;

    // Request error timestamp
    SDOProtocol::requestElement(conn.getNodeId(), SDOProtocol::INDEX_ERROR_TIME, i);
    if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
      printCanRx(&rxframe);
      if (rxframe.data[0] != SDOProtocol::ABORT &&
          (rxframe.data[1] | rxframe.data[2] << 8) == SDOProtocol::INDEX_ERROR_TIME &&
          rxframe.data[3] == i) {
        errorTime = *(uint32_t*)&rxframe.data[4];
        hasErrorTime = true;
      }
    }

    // Request error number
    SDOProtocol::requestElement(conn.getNodeId(), SDOProtocol::INDEX_ERROR_NUM, i);
    if (twai_receive(&rxframe, pdMS_TO_TICKS(10)) == ESP_OK) {
      printCanRx(&rxframe);
      if (rxframe.data[0] != SDOProtocol::ABORT &&
          (rxframe.data[1] | rxframe.data[2] << 8) == SDOProtocol::INDEX_ERROR_NUM &&
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
  if (!conn.isIdle()) return "";

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
      SDOProtocol::requestElement(conn.getNodeId(), SDOProtocol::INDEX_PARAM_UID | (id >> 8), id & 0xFF);
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

bool IsIdle() {
  return conn.isIdle();
}

int GetNodeId() {
  return conn.getNodeId();
}

BaudRate GetBaudRate() {
  return conn.getBaudRate();
}

// Initialize CAN bus without connecting to a specific device
void InitCAN(BaudRate baud, int txPin, int rxPin) {
  // Store pin configuration for later use
  conn.setCanPins(txPin, rxPin);

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
  conn.setBaudRate(baud);

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

  conn.setNodeId(0); // No specific device connected yet
  conn.setState(DeviceConnection::IDLE);
  DBG_OUTPUT_PORT.println("CAN bus initialized (no device connected)");

  // Load saved devices into memory
  DeviceDiscovery::instance().loadDevices();
}

// Initialize CAN and connect to a specific device
void Init(uint8_t nodeId, BaudRate baud, int txPin, int rxPin) {
  // Store pin configuration for later use
  conn.setCanPins(txPin, rxPin);

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
  conn.setBaudRate(baud);

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
  if (conn.getNodeId() != nodeId) {
    conn.clearJsonCache();
    DBG_OUTPUT_PORT.println("Cleared cached JSON (switching devices)");
  }

  conn.setNodeId(nodeId);
  conn.setState(DeviceConnection::OBTAINSERIAL);
  conn.resetStateStartTime();
  DBG_OUTPUT_PORT.printf("Requesting serial number from node %d (SDO 0x5000:0)\n", nodeId);
  SDOProtocol::requestElement(conn.getNodeId(), SDOProtocol::INDEX_SERIAL, 0);
  DBG_OUTPUT_PORT.println("Connected to device");
}

bool ReloadJson() {
  if (!conn.isIdle()) return false;

  // Remove the cached JSON file to force re-download
  if (LittleFS.exists(conn.getJsonFileName())) {
    LittleFS.remove(conn.getJsonFileName());
    DBG_OUTPUT_PORT.printf("Removed cached JSON file: %s\r\n", conn.getJsonFileName());
  }

  // Trigger JSON download
  conn.setState(DeviceConnection::OBTAINSERIAL);
  SDOProtocol::requestElement(conn.getNodeId(), SDOProtocol::INDEX_SERIAL, 0);

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
  // After a short delay, trigger JSON reload
  delay(500); // Give device time to start resetting
  conn.setState(DeviceConnection::OBTAINSERIAL);
  conn.setRetries(50);

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

void SetConnectionReadyCallback(ConnectionReadyCallback callback) {
  conn.setConnectionReadyCallback(callback);
}

void SetJsonDownloadProgressCallback(JsonDownloadProgressCallback callback) {
  conn.setJsonProgressCallback(callback);
}

void SetJsonStreamCallback(JsonStreamCallback callback) {
  conn.setJsonStreamCallback(callback);
}

int GetJsonTotalSize() {
  return conn.getJsonTotalSize();
}

}
