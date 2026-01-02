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
#include "sdo_protocol.h"

#include <cstring>

#include "Arduino.h"

#include "models/can_types.h"
#include "utils/can_queue.h"

namespace SDOProtocol {

// Pending write tracking for async response matching
static struct {
  bool active = false;
  uint16_t index = 0;
  uint8_t subIndex = 0;
  int paramId = 0;
  double value = 0;
  uint32_t timestamp = 0;
} pendingWrite;

// SDO Request/Response Constants
const uint8_t REQUEST_DOWNLOAD = (1 << 5);
const uint8_t REQUEST_UPLOAD = (2 << 5);
const uint8_t REQUEST_SEGMENT = (3 << 5);
const uint8_t TOGGLE_BIT = (1 << 4);
const uint8_t RESPONSE_UPLOAD = (2 << 5);
const uint8_t RESPONSE_DOWNLOAD = (3 << 5);
const uint8_t EXPEDITED = (1 << 1);
const uint8_t SIZE_SPECIFIED = 1;
const uint8_t WRITE = (REQUEST_DOWNLOAD | EXPEDITED | SIZE_SPECIFIED);
const uint8_t READ = REQUEST_UPLOAD;
const uint8_t ABORT = 0x80;
const uint8_t WRITE_REPLY = RESPONSE_DOWNLOAD;
const uint8_t READ_REPLY = (RESPONSE_UPLOAD | EXPEDITED | SIZE_SPECIFIED);

// SDO Error Codes
const uint32_t ERR_INVIDX = 0x06020000;
const uint32_t ERR_RANGE = 0x06090030;
const uint32_t ERR_GENERAL = 0x08000000;

// SDO Indexes
const uint16_t INDEX_PARAMS = 0x2000;
const uint16_t INDEX_PARAM_UID = 0x2100;
const uint16_t INDEX_MAP_TX = 0x3000;
const uint16_t INDEX_MAP_RX = 0x3001;
const uint16_t INDEX_MAP_RD = 0x3100;
const uint16_t INDEX_SERIAL = 0x5000;
const uint16_t INDEX_STRINGS = 0x5001;
const uint16_t INDEX_COMMANDS = 0x5002;
const uint16_t INDEX_ERROR_NUM = 0x5003;
const uint16_t INDEX_ERROR_TIME = 0x5004;

// SDO Commands
const uint8_t CMD_SAVE = 0;
const uint8_t CMD_LOAD = 1;
const uint8_t CMD_RESET = 2;
const uint8_t CMD_DEFAULTS = 3;
const uint8_t CMD_START = 4;
const uint8_t CMD_STOP = 5;

// SDO Request Functions

void requestElement(uint8_t nodeId, uint16_t index, uint8_t subIndex) {
  twai_message_t tx_frame;
  tx_frame.extd = false;
  tx_frame.identifier = SDO_REQUEST_BASE_ID | nodeId;
  tx_frame.data_length_code = 8;
  tx_frame.data[0] = READ;
  tx_frame.data[1] = index & 0xFF;
  tx_frame.data[2] = index >> 8;
  tx_frame.data[3] = subIndex;
  tx_frame.data[4] = 0;
  tx_frame.data[5] = 0;
  tx_frame.data[6] = 0;
  tx_frame.data[7] = 0;

  canQueueTransmit(&tx_frame, pdMS_TO_TICKS(10));
}

bool requestElementNonBlocking(uint8_t nodeId, uint16_t index, uint8_t subIndex) {
  twai_message_t tx_frame;
  tx_frame.extd = false;
  tx_frame.identifier = SDO_REQUEST_BASE_ID | nodeId;
  tx_frame.data_length_code = 8;
  tx_frame.data[0] = READ;
  tx_frame.data[1] = index & 0xFF;
  tx_frame.data[2] = index >> 8;
  tx_frame.data[3] = subIndex;
  tx_frame.data[4] = 0;
  tx_frame.data[5] = 0;
  tx_frame.data[6] = 0;
  tx_frame.data[7] = 0;

  // Non-blocking transmit (timeout = 0)
  return canQueueTransmit(&tx_frame, 0);
}

void setValue(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t value) {
  twai_message_t tx_frame;
  tx_frame.extd = false;
  tx_frame.identifier = SDO_REQUEST_BASE_ID | nodeId;
  tx_frame.data_length_code = 8;
  tx_frame.data[0] = WRITE;
  tx_frame.data[1] = index & 0xFF;
  tx_frame.data[2] = index >> 8;
  tx_frame.data[3] = subIndex;
  *(uint32_t*)&tx_frame.data[4] = value;

  canQueueTransmit(&tx_frame, pdMS_TO_TICKS(10));
}

void requestNextSegment(uint8_t nodeId, bool toggleBit) {
  twai_message_t tx_frame;
  tx_frame.extd = false;
  tx_frame.identifier = SDO_REQUEST_BASE_ID | nodeId;
  tx_frame.data_length_code = 8;
  tx_frame.data[0] = REQUEST_SEGMENT | (toggleBit ? TOGGLE_BIT : 0);
  tx_frame.data[1] = 0;
  tx_frame.data[2] = 0;
  tx_frame.data[3] = 0;
  tx_frame.data[4] = 0;
  tx_frame.data[5] = 0;
  tx_frame.data[6] = 0;
  tx_frame.data[7] = 0;

  canQueueTransmit(&tx_frame, pdMS_TO_TICKS(10));
}

bool waitForResponse(twai_message_t* response, TickType_t timeout) {
  return canQueueReceive(response, timeout);
}

void clearPendingResponses() {
  canQueueClearResponses();
}

// SDO Write-and-Wait Helpers

bool writeAndWait(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t value, twai_message_t* response,
                  TickType_t timeout) {
  // Note: Spot value responses are now routed directly to SpotValuesManager,
  // but other SDO responses may still appear (device connection, scan, etc.)
  // We loop through responses looking for one that matches our index/subindex
  setValue(nodeId, index, subIndex, value);

  TickType_t startTick = xTaskGetTickCount();
  TickType_t remainingTimeout = timeout;

  while (remainingTimeout > 0) {
    if (!waitForResponse(response, remainingTimeout)) {
      // Timeout - zero-initialize so caller can distinguish timeout from abort
      memset(response, 0, sizeof(twai_message_t));
      return false;
    }

    // Check if this response matches our request (same index/subindex)
    uint16_t respIndex = response->data[1] | (response->data[2] << 8);
    uint8_t respSubIndex = response->data[3];

    if (respIndex == index && respSubIndex == subIndex) {
      // This response is for our request
      return response->data[0] != ABORT;
    }

    // Response was for a different request, keep waiting
    // Update remaining timeout
    TickType_t elapsed = xTaskGetTickCount() - startTick;
    remainingTimeout = (elapsed < timeout) ? (timeout - elapsed) : 0;
  }

  // Timeout while waiting for matching response
  memset(response, 0, sizeof(twai_message_t));
  return false;
}

bool writeAndWait(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t value, TickType_t timeout) {
  twai_message_t response;
  return writeAndWait(nodeId, index, subIndex, value, &response, timeout);
}

// SDO Request-and-Wait Helpers

bool requestAndWait(uint8_t nodeId, uint16_t index, uint8_t subIndex, twai_message_t* response, TickType_t timeout) {
  clearPendingResponses();
  requestElement(nodeId, index, subIndex);

  if (!waitForResponse(response, timeout)) {
    // Zero-initialize on timeout so caller can distinguish timeout from abort
    memset(response, 0, sizeof(twai_message_t));
    return false;
  }

  return response->data[0] != ABORT;
}

bool requestValue(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t* outValue, TickType_t timeout) {
  twai_message_t response;

  if (!requestAndWait(nodeId, index, subIndex, &response, timeout)) {
    return false;
  }

  *outValue = *(uint32_t*)&response.data[4];
  return true;
}

// Async write support - non-blocking parameter updates

bool setValueAsync(uint8_t nodeId, int paramId, double value) {
  if (pendingWrite.active) {
    return false;  // Already have a pending write
  }

  uint16_t index = INDEX_PARAM_UID | (paramId >> 8);
  uint8_t subIndex = paramId & 0xFF;

  // Register the pending write
  pendingWrite.active = true;
  pendingWrite.index = index;
  pendingWrite.subIndex = subIndex;
  pendingWrite.paramId = paramId;
  pendingWrite.value = value;
  pendingWrite.timestamp = millis();

  Serial.printf("[SDO] setValueAsync: nodeId=%d, paramId=%d, index=0x%04X, subIndex=%d, value=%.2f\n", nodeId, paramId,
                index, subIndex, value);

  // Send the write (non-blocking)
  setValue(nodeId, index, subIndex, (uint32_t)(value * 32));
  return true;
}

bool hasPendingWrite() {
  return pendingWrite.active;
}

bool matchPendingWrite(uint16_t respIndex, uint8_t respSubIndex, bool isAbort, uint32_t errorCode, int& outParamId,
                       double& outValue, SetValueResult& outResult) {
  if (!pendingWrite.active) {
    return false;
  }

  Serial.printf("[SDO] matchPendingWrite: resp=0x%04X/%d, pending=0x%04X/%d, isAbort=%d\n", respIndex, respSubIndex,
                pendingWrite.index, pendingWrite.subIndex, isAbort);

  // Check if response matches our pending write
  if (respIndex != pendingWrite.index || respSubIndex != pendingWrite.subIndex) {
    return false;
  }

  // Match found - extract info and clear pending
  outParamId = pendingWrite.paramId;
  outValue = pendingWrite.value;

  if (isAbort) {
    if (errorCode == ERR_RANGE) {
      outResult = SetValueResult::SET_VALUE_OUT_OF_RANGE;
    } else {
      outResult = SetValueResult::SET_UNKNOWN_INDEX;
    }
  } else {
    outResult = SetValueResult::SET_OK;
  }

  pendingWrite.active = false;
  return true;
}

bool checkPendingWriteTimeout(int& outParamId, double& outValue, SetValueResult& outResult) {
  if (!pendingWrite.active) {
    return false;
  }

  const uint32_t TIMEOUT_MS = 500;
  if ((millis() - pendingWrite.timestamp) >= TIMEOUT_MS) {
    Serial.printf("[SDO] Pending write TIMEOUT: paramId=%d, index=0x%04X, subIndex=%d\n", pendingWrite.paramId,
                  pendingWrite.index, pendingWrite.subIndex);
    outParamId = pendingWrite.paramId;
    outValue = pendingWrite.value;
    outResult = SetValueResult::SET_COMM_ERROR;
    pendingWrite.active = false;
    return true;
  }

  return false;
}

void clearPendingWrite() {
  pendingWrite.active = false;
}

}  // namespace SDOProtocol
