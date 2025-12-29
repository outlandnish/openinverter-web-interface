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
#include "sdo_protocol.h"
#include "models/can_types.h"
#include "utils/can_queue.h"

namespace SDOProtocol {

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

} // namespace SDOProtocol
