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
#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <FS.h>
#include "driver/twai.h"
#include "models/can_types.h"

// Forward declarations for callback types
typedef void (*ConnectionReadyCallback)(uint8_t nodeId, const char* serial);
typedef void (*JsonDownloadProgressCallback)(int bytesReceived);
typedef void (*JsonStreamCallback)(const char* chunk, int chunkSize, bool isComplete);

/**
 * Manages the connection state and JSON cache for a single CAN device.
 * Uses singleton pattern since only one device connection is active at a time.
 */
class DeviceConnection {
public:
    // Connection states
    enum State { IDLE, ERROR, OBTAINSERIAL, OBTAIN_JSON };

    // Singleton access
    static DeviceConnection& instance();

    // Connection state management
    void setNodeId(uint8_t nodeId) { nodeId_ = nodeId; }
    uint8_t getNodeId() const { return nodeId_; }

    void setBaudRate(BaudRate baud) { baudRate_ = baud; }
    BaudRate getBaudRate() const { return baudRate_; }

    void setState(State newState);
    State getState() const { return state_; }
    bool isIdle() const { return state_ == IDLE; }

    void setCanPins(int txPin, int rxPin) {
        canTxPin_ = txPin;
        canRxPin_ = rxPin;
    }
    int getCanTxPin() const { return canTxPin_; }
    int getCanRxPin() const { return canRxPin_; }

    // Serial number management
    void setSerialPart(uint8_t index, uint32_t value);
    uint32_t getSerialPart(uint8_t index) const;
    const char* getJsonFileName() const { return jsonFileName_; }
    void generateJsonFileName(); // Generate from serial parts

    // Retry management
    void setRetries(int value) { retries_ = value; }
    void incrementRetries() { retries_++; }
    void decrementRetries() { retries_--; }
    void resetRetries() { retries_ = 0; }
    int getRetries() const { return retries_; }

    // State timeout tracking
    void resetStateStartTime();
    unsigned long getStateElapsedTime() const;
    bool hasStateTimedOut(unsigned long timeoutMs) const;

    // JSON cache management
    JsonDocument& getCachedJson() { return cachedParamJson_; }
    const JsonDocument& getCachedJson() const { return cachedParamJson_; }

    String& getJsonReceiveBuffer() { return jsonReceiveBuffer_; }
    const String& getJsonReceiveBuffer() const { return jsonReceiveBuffer_; }

    void setJsonTotalSize(int size) { jsonTotalSize_ = size; }
    int getJsonTotalSize() const { return jsonTotalSize_; }

    void clearJsonCache();

    // Callbacks
    void setConnectionReadyCallback(ConnectionReadyCallback callback) {
        connectionReadyCallback_ = callback;
    }
    void setJsonProgressCallback(JsonDownloadProgressCallback callback) {
        jsonProgressCallback_ = callback;
    }
    void setJsonStreamCallback(JsonStreamCallback callback) {
        jsonStreamCallback_ = callback;
    }

    ConnectionReadyCallback getConnectionReadyCallback() const {
        return connectionReadyCallback_;
    }
    JsonDownloadProgressCallback getJsonProgressCallback() const {
        return jsonProgressCallback_;
    }
    JsonStreamCallback getJsonStreamCallback() const {
        return jsonStreamCallback_;
    }

    // Rate limiting for parameter requests
    void setParameterRequestRateLimit(unsigned long intervalUs) {
        minParamRequestIntervalUs_ = intervalUs;
    }
    bool canSendParameterRequest();
    void markParameterRequestSent();

    // State timeout checking
    void checkSerialTimeout(); // Check if OBTAINSERIAL has timed out

    // SDO response processing
    void processSdoResponse(twai_message_t* rxframe); // Process SDO response based on state

private:
    DeviceConnection(); // Private constructor for singleton
    DeviceConnection(const DeviceConnection&) = delete;
    DeviceConnection& operator=(const DeviceConnection&) = delete;

    // Connection state
    uint8_t nodeId_ = 0;
    BaudRate baudRate_ = Baud500k;
    State state_ = IDLE;
    int canTxPin_ = -1;
    int canRxPin_ = -1;
    uint32_t serial_[4] = {0};
    char jsonFileName_[20] = {0};
    int retries_ = 0;
    unsigned long stateStartTime_ = 0;

    // JSON cache
    JsonDocument cachedParamJson_;
    String jsonReceiveBuffer_;
    int jsonTotalSize_ = 0;

    // Callbacks
    ConnectionReadyCallback connectionReadyCallback_ = nullptr;
    JsonDownloadProgressCallback jsonProgressCallback_ = nullptr;
    JsonStreamCallback jsonStreamCallback_ = nullptr;

    // Rate limiting
    unsigned long lastParamRequestTime_ = 0;
    unsigned long minParamRequestIntervalUs_ = 500; // Default: 500 microseconds

    // SDO response state (used by processSdoResponse)
    bool toggleBit_ = false;
    File file_; // Used during firmware update

    // Constants
    static const unsigned long OBTAINSERIAL_TIMEOUT_MS = 5000;
};
