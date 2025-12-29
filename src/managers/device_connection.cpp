/*
 * This file is part of the openinverter web interface
 *
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
#include "device_connection.h"
#include "device_discovery.h"
#include "protocols/sdo_protocol.h"
#include "can_task.h"
#include <Arduino.h>

#define DBG_OUTPUT_PORT Serial

DeviceConnection::DeviceConnection() {
    // Initialize arrays
    for (int i = 0; i < 4; i++) {
        serial_[i] = 0;
    }
    jsonFileName_[0] = '\0';

    // Create mutex for JSON buffer protection
    jsonBufferMutex_ = xSemaphoreCreateMutex();
}

DeviceConnection& DeviceConnection::instance() {
    static DeviceConnection instance;
    return instance;
}

void DeviceConnection::setState(State newState) {
    state_ = newState;
    resetStateStartTime();
}

void DeviceConnection::setSerialPart(uint8_t index, uint32_t value) {
    if (index < 4) {
        serial_[index] = value;
    }
}

uint32_t DeviceConnection::getSerialPart(uint8_t index) const {
    return (index < 4) ? serial_[index] : 0;
}

void DeviceConnection::generateJsonFileName() {
    snprintf(jsonFileName_, sizeof(jsonFileName_), "/%" PRIx32 ".json", serial_[3]);
}

void DeviceConnection::resetStateStartTime() {
    stateStartTime_ = millis();
}

unsigned long DeviceConnection::getStateElapsedTime() const {
    return millis() - stateStartTime_;
}

bool DeviceConnection::hasStateTimedOut(unsigned long timeoutMs) const {
    return getStateElapsedTime() > timeoutMs;
}

void DeviceConnection::clearJsonCache() {
    if (xSemaphoreTake(jsonBufferMutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        cachedParamJson_.clear();
        jsonReceiveBuffer_ = "";
        jsonTotalSize_ = 0;
        xSemaphoreGive(jsonBufferMutex_);
    }
}

// Thread-safe JSON buffer accessors
String DeviceConnection::getJsonReceiveBufferCopy() {
    String copy;
    if (xSemaphoreTake(jsonBufferMutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        copy = jsonReceiveBuffer_;
        xSemaphoreGive(jsonBufferMutex_);
    }
    return copy;
}

int DeviceConnection::getJsonReceiveBufferLength() {
    int len = 0;
    if (xSemaphoreTake(jsonBufferMutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        len = jsonReceiveBuffer_.length();
        xSemaphoreGive(jsonBufferMutex_);
    }
    return len;
}

bool DeviceConnection::isJsonBufferEmpty() {
    bool empty = true;
    if (xSemaphoreTake(jsonBufferMutex_, pdMS_TO_TICKS(100)) == pdTRUE) {
        empty = jsonReceiveBuffer_.isEmpty();
        xSemaphoreGive(jsonBufferMutex_);
    }
    return empty;
}

bool DeviceConnection::canSendParameterRequest() {
    unsigned long currentTime = micros();
    unsigned long timeSinceLastRequest = currentTime - lastParamRequestTime_;
    return timeSinceLastRequest >= minParamRequestIntervalUs_;
}

void DeviceConnection::markParameterRequestSent() {
    lastParamRequestTime_ = micros();
}

// Start JSON download (called when browser requests JSON)
void DeviceConnection::startJsonDownload() {
    if (state_ != IDLE) {
        DBG_OUTPUT_PORT.println("[DeviceConnection] Cannot start JSON download - not in IDLE state");
        return;
    }

    jsonReceiveBuffer_ = "";
    jsonTotalSize_ = 0;
    toggleBit_ = false;
    setState(JSON_INIT_SENDING);
}

// Start serial acquisition (used after device reset)
void DeviceConnection::startSerialAcquisition() {
    if (state_ != IDLE) {
        DBG_OUTPUT_PORT.println("[DeviceConnection] Cannot start serial acquisition - not in IDLE state");
        return;
    }

    currentSerialPart_ = 0;
    toggleBit_ = false;
    setState(SERIAL_SENDING);
    DBG_OUTPUT_PORT.printf("[DeviceConnection] Starting serial acquisition for node %d\n", nodeId_);
}

// Non-blocking state machine processing (called from can_task loop)
void DeviceConnection::processConnection() {
    unsigned long currentTime = millis();
    twai_message_t rxframe;

    switch (state_) {
        case IDLE:
        case ERROR:
            // Nothing to do
            break;

        // =====================================================================
        // Serial number acquisition states
        // =====================================================================
        case SERIAL_SENDING:
            // Clear any stale responses
            SDOProtocol::clearPendingResponses();

            // Send request for current serial part
            SDOProtocol::requestElement(nodeId_, SDOProtocol::INDEX_SERIAL, currentSerialPart_);
            requestSentTime_ = currentTime;
            state_ = SERIAL_WAITING;
            break;

        case SERIAL_WAITING:
            // Non-blocking check for response
            if (SDOProtocol::waitForResponse(&rxframe, 0)) {
                // Check for abort
                if (rxframe.data[0] == SDOProtocol::ABORT) {
                    DBG_OUTPUT_PORT.println("[DeviceConnection] SDO abort - error obtaining serial");
                    setState(ERROR);
                    break;
                }

                // Validate response is for serial index
                uint16_t rxIndex = rxframe.data[1] | (rxframe.data[2] << 8);
                if (rxIndex == SDOProtocol::INDEX_SERIAL && rxframe.data[3] == currentSerialPart_) {
                    setSerialPart(currentSerialPart_, *(uint32_t*)&rxframe.data[4]);
                    currentSerialPart_++;

                    if (currentSerialPart_ < 4) {
                        // More parts to fetch
                        state_ = SERIAL_SENDING;
                    } else {
                        // Got all 4 parts
                        generateJsonFileName();
                        DBG_OUTPUT_PORT.printf("Got Serial Number %" PRIX32 ":%" PRIX32 ":%" PRIX32 ":%" PRIX32 "\r\n",
                            serial_[0], serial_[1], serial_[2], serial_[3]);

                        setState(IDLE);
                        DBG_OUTPUT_PORT.println("Connection established. Parameter JSON available on request.");

                        // Notify that connection is ready
                        if (connectionReadyCallback_) {
                            char serialStr[64];
                            sprintf(serialStr, "%" PRIX32 ":%" PRIX32 ":%" PRIX32 ":%" PRIX32,
                                serial_[0], serial_[1], serial_[2], serial_[3]);
                            connectionReadyCallback_(nodeId_, serialStr);
                        }
                    }
                }
            } else if ((currentTime - requestSentTime_) >= SDO_TIMEOUT_MS) {
                // Timeout - retry or error
                if (hasStateTimedOut(CONNECTION_TIMEOUT_MS)) {
                    DBG_OUTPUT_PORT.println("[DeviceConnection] Connection timeout");
                    setState(ERROR);
                } else {
                    // Retry current part
                    state_ = SERIAL_SENDING;
                }
            }
            break;

        // =====================================================================
        // JSON download states
        // =====================================================================
        case JSON_INIT_SENDING:
            // Clear any stale responses
            SDOProtocol::clearPendingResponses();

            // Send initiate upload request for strings index
            SDOProtocol::requestElement(nodeId_, SDOProtocol::INDEX_STRINGS, 0);
            requestSentTime_ = currentTime;
            state_ = JSON_INIT_WAITING;
            break;

        case JSON_INIT_WAITING:
            if (SDOProtocol::waitForResponse(&rxframe, 0)) {
                if (rxframe.data[0] == SDOProtocol::ABORT) {
                    DBG_OUTPUT_PORT.println("[DeviceConnection] SDO abort during JSON init");
                    setState(ERROR);
                    break;
                }

                // Check for initiate upload response
                if ((rxframe.data[0] & SDOProtocol::READ) == SDOProtocol::READ) {
                    DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Initiate upload response received");

                    if (rxframe.data[0] & SDOProtocol::SIZE_SPECIFIED) {
                        jsonTotalSize_ = *(uint32_t*)&rxframe.data[4];
                        DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] Total size: %d bytes\r\n", jsonTotalSize_);

                        if (jsonProgressCallback_) {
                            jsonProgressCallback_(0);
                        }
                    } else {
                        jsonTotalSize_ = 0;
                    }

                    // Request first segment
                    state_ = JSON_SEGMENT_SENDING;
                }
            } else if ((currentTime - requestSentTime_) >= SDO_TIMEOUT_MS) {
                DBG_OUTPUT_PORT.println("[DeviceConnection] JSON init timeout");
                setState(ERROR);
            }
            break;

        case JSON_SEGMENT_SENDING:
            SDOProtocol::requestNextSegment(nodeId_, toggleBit_);
            requestSentTime_ = currentTime;
            state_ = JSON_SEGMENT_WAITING;
            break;

        case JSON_SEGMENT_WAITING:
            if (SDOProtocol::waitForResponse(&rxframe, 0)) {
                if (rxframe.data[0] == SDOProtocol::ABORT) {
                    DBG_OUTPUT_PORT.println("[DeviceConnection] SDO abort during JSON download");
                    setState(ERROR);
                    break;
                }

                // Check for last segment
                if ((rxframe.data[0] & SDOProtocol::SIZE_SPECIFIED) && (rxframe.data[0] & SDOProtocol::READ) == 0) {
                    // Last segment - protect buffer access
                    if (xSemaphoreTake(jsonBufferMutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
                        int size = 7 - ((rxframe.data[0] >> 1) & 0x7);
                        for (int i = 0; i < size; i++) {
                            jsonReceiveBuffer_ += (char)rxframe.data[1 + i];
                        }

                        DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Download complete");
                        DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] JSON size: %d bytes\r\n", jsonReceiveBuffer_.length());

                        // Parse JSON
                        DeserializationError error = deserializeJson(cachedParamJson_, jsonReceiveBuffer_);
                        if (error) {
                            DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] Parse error: %s\r\n", error.c_str());
                        } else {
                            DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Parsed successfully");
                        }
                        xSemaphoreGive(jsonBufferMutex_);
                    }

                    setState(IDLE);
                }
                // Normal segment
                else if ((rxframe.data[0] & 0xE0) == 0 && rxframe.data[0] == (toggleBit_ << 4)) {
                    // Protect buffer access
                    if (xSemaphoreTake(jsonBufferMutex_, pdMS_TO_TICKS(50)) == pdTRUE) {
                        for (int i = 0; i < 7; i++) {
                            jsonReceiveBuffer_ += (char)rxframe.data[1 + i];
                        }
                        xSemaphoreGive(jsonBufferMutex_);
                    }
                    toggleBit_ = !toggleBit_;
                    state_ = JSON_SEGMENT_SENDING;
                }
            } else if ((currentTime - requestSentTime_) >= SDO_TIMEOUT_MS) {
                // Timeout - retry
                DBG_OUTPUT_PORT.println("[DeviceConnection] JSON segment timeout, retrying");
                state_ = JSON_SEGMENT_SENDING;
            }
            break;
    }
}

bool DeviceConnection::connectToDevice(uint8_t nodeId, BaudRate baud, int txPin, int rxPin) {
    setCanPins(txPin, rxPin);
    setBaudRate(baud);

    if (!initCanBusForDevice(nodeId, baud, txPin, rxPin)) {
        DBG_OUTPUT_PORT.println("Failed to initialize CAN bus");
        return false;
    }

    // Clear cached JSON when switching to a different device
    if (nodeId_ != nodeId) {
        clearJsonCache();
        DBG_OUTPUT_PORT.println("Cleared cached JSON (switching devices)");
    }

    nodeId_ = nodeId;
    currentSerialPart_ = 0;
    toggleBit_ = false;
    setState(SERIAL_SENDING);  // Start the serial acquisition state machine
    DBG_OUTPUT_PORT.printf("Connecting to node %d...\n", nodeId);
    return true;
}

bool DeviceConnection::initializeForScanning(BaudRate baud, int txPin, int rxPin) {
    setCanPins(txPin, rxPin);
    setBaudRate(baud);

    if (!initCanBusScanning(baud, txPin, rxPin)) {
        DBG_OUTPUT_PORT.println("Failed to initialize CAN bus for scanning");
        return false;
    }

    nodeId_ = 0; // No specific device connected yet
    setState(IDLE);
    DBG_OUTPUT_PORT.println("CAN bus initialized (no device connected)");

    // Load saved devices into memory
    DeviceDiscovery::instance().loadDevices();
    return true;
}
