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
    cachedParamJson_.clear();
    jsonReceiveBuffer_ = "";
    jsonTotalSize_ = 0;
}

bool DeviceConnection::canSendParameterRequest() {
    unsigned long currentTime = micros();
    unsigned long timeSinceLastRequest = currentTime - lastParamRequestTime_;
    return timeSinceLastRequest >= minParamRequestIntervalUs_;
}

void DeviceConnection::markParameterRequestSent() {
    lastParamRequestTime_ = micros();
}

void DeviceConnection::checkSerialTimeout() {
    if (state_ == OBTAINSERIAL && hasStateTimedOut(OBTAINSERIAL_TIMEOUT_MS)) {
        DBG_OUTPUT_PORT.println("[DeviceConnection] OBTAINSERIAL timeout - resetting to IDLE");
        setState(IDLE);
    }
}

void DeviceConnection::processSdoResponse(twai_message_t* rxframe) {
    if (rxframe->data[0] == SDOProtocol::ABORT) { // SDO abort
        setState(ERROR);
        DBG_OUTPUT_PORT.println("Error obtaining serial number, try restarting");
        return;
    }

    switch (state_) {
        case OBTAINSERIAL:
            if ((rxframe->data[1] | rxframe->data[2] << 8) == SDOProtocol::INDEX_SERIAL && rxframe->data[3] < 4) {
                setSerialPart(rxframe->data[3], *(uint32_t*)&rxframe->data[4]);

                if (rxframe->data[3] < 3) {
                    SDOProtocol::requestElement(nodeId_, SDOProtocol::INDEX_SERIAL, rxframe->data[3] + 1);
                }
                else {
                    generateJsonFileName();
                    DBG_OUTPUT_PORT.printf("Got Serial Number %" PRIX32 ":%" PRIX32 ":%" PRIX32 ":%" PRIX32 "\r\n",
                        serial_[0], serial_[1], serial_[2], serial_[3]);

                    // Go to IDLE - JSON will be downloaded on-demand when browser requests it
                    setState(IDLE);
                    DBG_OUTPUT_PORT.println("Connection established. Parameter JSON available on request.");

                    // Notify that connection is ready (device is in IDLE state)
                    if (connectionReadyCallback_) {
                        char serialStr[64];
                        sprintf(serialStr, "%" PRIX32 ":%" PRIX32 ":%" PRIX32 ":%" PRIX32,
                            serial_[0], serial_[1], serial_[2], serial_[3]);
                        connectionReadyCallback_(nodeId_, serialStr);
                    }
                }
            }
            break;

        case OBTAIN_JSON:
            // Receiving last segment
            if ((rxframe->data[0] & SDOProtocol::SIZE_SPECIFIED) && (rxframe->data[0] & SDOProtocol::READ) == 0) {
                DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Receiving last segment");
                int size = 7 - ((rxframe->data[0] >> 1) & 0x7);
                for (int i = 0; i < size; i++) {
                    jsonReceiveBuffer_ += (char)rxframe->data[1 + i];
                }

                DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Download complete");
                DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] JSON size: %d bytes\r\n", jsonReceiveBuffer_.length());

                // Parse JSON into cachedParamJson for future use
                DeserializationError error = deserializeJson(cachedParamJson_, jsonReceiveBuffer_);
                if (error) {
                    DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] Parse error: %s\r\n", error.c_str());
                } else {
                    DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Parsed successfully");
                }

                setState(IDLE);
            }
            // Receiving a segment
            else if (rxframe->data[0] == (toggleBit_ << 4) && (rxframe->data[0] & SDOProtocol::READ) == 0) {
                DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] Segment received (buffer: %d bytes)\n", jsonReceiveBuffer_.length());
                for (int i = 0; i < 7; i++) {
                    jsonReceiveBuffer_ += (char)rxframe->data[1 + i];
                }
                toggleBit_ = !toggleBit_;
                SDOProtocol::requestNextSegment(nodeId_, toggleBit_);
            }
            // Request first segment (initiate upload response)
            else if ((rxframe->data[0] & SDOProtocol::READ) == SDOProtocol::READ) {
                DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Initiate upload response received");

                // Check if size is specified in the response (CANopen SDO protocol)
                if (rxframe->data[0] & SDOProtocol::SIZE_SPECIFIED) {
                    // Extract total size from bytes 4-7 (little-endian)
                    jsonTotalSize_ = *(uint32_t*)&rxframe->data[4];
                    DBG_OUTPUT_PORT.printf("[OBTAIN_JSON] Total size indicated: %d bytes\r\n", jsonTotalSize_);

                    // Send initial progress update (0 bytes received, but total is known)
                    if (jsonProgressCallback_) {
                        jsonProgressCallback_(0); // Will include totalBytes in the message via GetJsonTotalSize()
                    }
                } else {
                    jsonTotalSize_ = 0; // Unknown size
                    DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Total size not specified by device");
                }

                DBG_OUTPUT_PORT.println("[OBTAIN_JSON] Requesting first segment");
                SDOProtocol::requestNextSegment(nodeId_, toggleBit_);
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
    setState(OBTAINSERIAL);
    resetStateStartTime();
    DBG_OUTPUT_PORT.printf("Requesting serial number from node %d (SDO 0x5000:0)\n", nodeId);
    SDOProtocol::requestElement(nodeId_, SDOProtocol::INDEX_SERIAL, 0);
    DBG_OUTPUT_PORT.println("Connected to device");
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
