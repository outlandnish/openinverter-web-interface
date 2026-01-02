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

#include <WiFiClient.h>

#include <functional>

#include "models/can_types.h"

namespace OICan {

// Callback type for ClearCanMap progress
typedef std::function<void(int removedCount)> ClearMapProgressCallback;
enum SetResult { Ok, UnknownIndex, ValueOutOfRange, CommError };
// BaudRate is defined in models/can_types.h
using ::Baud125k;
using ::Baud250k;
using ::Baud500k;
using ::BaudRate;

void InitCAN(BaudRate baud, int txPin, int rxPin);               // Initialize CAN bus only
void Init(uint8_t nodeId, BaudRate baud, int txPin, int rxPin);  // Initialize and connect to device
bool SendJson(WiFiClient c);
String GetRawJson();                // Get parameter JSON from currently connected device
String GetRawJson(uint8_t nodeId);  // Get parameter JSON from specific device by nodeId
void SendCanMapping(WiFiClient c);
String GetCanMapping();  // Get CAN mappings as JSON string
SetResult AddCanMapping(String json);
SetResult RemoveCanMapping(String json);
SetResult SetValue(int paramId, double value);
bool RequestValue(int paramId);  // Send SDO request without waiting (async, non-blocking with rate limiting, returns
                                 // false if TX queue full)
bool TryGetValueResponse(int& outParamId, double& outValue, int timeoutMs);  // Try to receive response (async)
void SetParameterRequestRateLimit(
    unsigned long intervalUs);  // Configure minimum interval between parameter requests (default: 500us)
bool SaveToFlash();
bool LoadFromFlash();
bool LoadDefaults();
bool StartDevice(uint32_t mode = 0);
bool StopDevice();
bool ClearCanMap(bool isRx, ClearMapProgressCallback onProgress = nullptr);
String ListErrors();
bool SendCanMessage(uint32_t canId, const uint8_t* data, uint8_t dataLength);  // Send arbitrary CAN message
String StreamValues(String paramIds, int samples);
int StartUpdate(String fileName);
bool ReloadJson();                // Reload JSON for currently connected device
bool ReloadJson(uint8_t nodeId);  // Reload JSON for specific device by nodeId
bool ResetDevice();

// Device management functions
String ScanDevices(uint8_t startNodeId, uint8_t endNodeId);

// Continuous scanning functions
bool StartContinuousScan(uint8_t startNodeId = 1, uint8_t endNodeId = 32);  // Returns true if scan started successfully

}  // namespace OICan
