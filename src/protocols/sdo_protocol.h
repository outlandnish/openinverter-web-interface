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
#pragma once

#include <cstdint>

#include "driver/twai.h"

namespace SDOProtocol {

// SDO Request/Response Constants
extern const uint8_t REQUEST_DOWNLOAD;
extern const uint8_t REQUEST_UPLOAD;
extern const uint8_t REQUEST_SEGMENT;
extern const uint8_t TOGGLE_BIT;
extern const uint8_t RESPONSE_UPLOAD;
extern const uint8_t RESPONSE_DOWNLOAD;
extern const uint8_t EXPEDITED;
extern const uint8_t SIZE_SPECIFIED;
extern const uint8_t WRITE;
extern const uint8_t READ;
extern const uint8_t ABORT;
extern const uint8_t WRITE_REPLY;
extern const uint8_t READ_REPLY;

// SDO Error Codes
extern const uint32_t ERR_INVIDX;
extern const uint32_t ERR_RANGE;
extern const uint32_t ERR_GENERAL;

// SDO Indexes
extern const uint16_t INDEX_PARAMS;
extern const uint16_t INDEX_PARAM_UID;
extern const uint16_t INDEX_MAP_TX;
extern const uint16_t INDEX_MAP_RX;
extern const uint16_t INDEX_MAP_RD;
extern const uint16_t INDEX_SERIAL;
extern const uint16_t INDEX_STRINGS;
extern const uint16_t INDEX_COMMANDS;
extern const uint16_t INDEX_ERROR_NUM;
extern const uint16_t INDEX_ERROR_TIME;

// SDO Commands
extern const uint8_t CMD_SAVE;
extern const uint8_t CMD_LOAD;
extern const uint8_t CMD_RESET;
extern const uint8_t CMD_DEFAULTS;
extern const uint8_t CMD_START;
extern const uint8_t CMD_STOP;

// SDO Request Functions
void requestElement(uint8_t nodeId, uint16_t index, uint8_t subIndex);
bool requestElementNonBlocking(uint8_t nodeId, uint16_t index, uint8_t subIndex);
void setValue(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t value);
void requestNextSegment(uint8_t nodeId, bool toggleBit);

// SDO Response Functions (queue-based)
bool waitForResponse(twai_message_t* response, TickType_t timeout);
void clearPendingResponses();

// SDO Write-and-Wait Helpers
// Combines: clearPendingResponses, setValue, waitForResponse, check for ABORT
// Returns true if write succeeded (response received and not aborted)
bool writeAndWait(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t value,
                  TickType_t timeout = pdMS_TO_TICKS(10));

// Version that also returns the response frame for error code inspection
bool writeAndWait(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t value, twai_message_t* response,
                  TickType_t timeout = pdMS_TO_TICKS(10));

// SDO Request-and-Wait Helpers
// Combines: clearPendingResponses, requestElement, waitForResponse, check for ABORT
// Returns true if read succeeded (response received and not aborted)
bool requestAndWait(uint8_t nodeId, uint16_t index, uint8_t subIndex, twai_message_t* response,
                    TickType_t timeout = pdMS_TO_TICKS(10));

// Convenience version that extracts the 32-bit value directly
bool requestValue(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t* outValue,
                  TickType_t timeout = pdMS_TO_TICKS(10));

}  // namespace SDOProtocol
