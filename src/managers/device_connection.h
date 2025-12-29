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
#include "freertos/semphr.h"
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
    // Connection states - non-blocking state machine
    enum State {
        IDLE,
        ERROR,
        // Serial number acquisition
        SERIAL_SENDING,      // Sending request for serial part
        SERIAL_WAITING,      // Waiting for serial part response
        // JSON download
        JSON_INIT_SENDING,   // Sending JSON initiate request
        JSON_INIT_WAITING,   // Waiting for JSON initiate response
        JSON_SEGMENT_SENDING,// Sending segment request
        JSON_SEGMENT_WAITING // Waiting for segment response
    };

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
    bool isDownloadingJson() const {
        return state_ == JSON_INIT_SENDING || state_ == JSON_INIT_WAITING ||
               state_ == JSON_SEGMENT_SENDING || state_ == JSON_SEGMENT_WAITING;
    }
    bool isAcquiringSerial() const {
        return state_ == SERIAL_SENDING || state_ == SERIAL_WAITING;
    }

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

    // Connection initialization
    bool connectToDevice(uint8_t nodeId, BaudRate baud, int txPin, int rxPin);
    bool initializeForScanning(BaudRate baud, int txPin, int rxPin);

    // JSON cache management (thread-safe)
    JsonDocument& getCachedJson() { return cachedParamJson_; }
    const JsonDocument& getCachedJson() const { return cachedParamJson_; }

    // Thread-safe JSON buffer accessors
    String getJsonReceiveBufferCopy();  // Returns a copy (thread-safe)
    int getJsonReceiveBufferLength();   // Returns length (thread-safe)
    bool isJsonBufferEmpty();           // Check if empty (thread-safe)

    // Legacy accessors (only use from CAN task or when download is complete)
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

    // Non-blocking state machine processing (called from can_task loop)
    void processConnection();

    // Start JSON download (called when browser requests JSON)
    void startJsonDownload();

    // Start JSON download for a specific client (non-blocking)
    bool startJsonDownloadAsync(uint32_t clientId);
    uint32_t getJsonRequestClientId() const { return jsonRequestClientId_; }
    void clearJsonRequestClientId() { jsonRequestClientId_ = 0; }

    // Start serial acquisition (used after device reset)
    void startSerialAcquisition();

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
    SemaphoreHandle_t jsonBufferMutex_ = nullptr;  // Protects jsonReceiveBuffer_
    uint32_t jsonRequestClientId_ = 0;  // WebSocket client that requested JSON download

    // Callbacks
    ConnectionReadyCallback connectionReadyCallback_ = nullptr;
    JsonDownloadProgressCallback jsonProgressCallback_ = nullptr;
    JsonStreamCallback jsonStreamCallback_ = nullptr;

    // Rate limiting
    unsigned long lastParamRequestTime_ = 0;
    unsigned long minParamRequestIntervalUs_ = 500; // Default: 500 microseconds

    // SDO state machine
    bool toggleBit_ = false;
    uint8_t currentSerialPart_ = 0;  // Current serial part being requested (0-3)
    unsigned long requestSentTime_ = 0;  // When we sent the current request
    File file_; // Used during firmware update

    // Constants
    static const unsigned long SDO_TIMEOUT_MS = 100;  // Timeout for SDO response
    static const unsigned long CONNECTION_TIMEOUT_MS = 5000;  // Overall connection timeout
};
