/*
 * This file is part of the esp32 web interface
 *
 * Copyright (C) 2018 Johannes Huebner <dev@johanneshuebner.com>
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
#ifndef OI_CAN_H
#define OI_CAN_H
#include <WiFiClient.h>

namespace OICan {
enum SetResult { Ok, UnknownIndex, ValueOutOfRange, CommError };
enum BaudRate { Baud125k, Baud250k, Baud500k };

void InitCAN(BaudRate baud, int txPin, int rxPin); // Initialize CAN bus only
void Init(uint8_t nodeId, BaudRate baud, int txPin, int rxPin); // Initialize and connect to device
void Loop();
bool SendJson(WiFiClient c);
String GetRawJson(); // Get parameter JSON from currently connected device
String GetRawJson(uint8_t nodeId); // Get parameter JSON from specific device by nodeId
void SendCanMapping(WiFiClient c);
String GetCanMapping(); // Get CAN mappings as JSON string
SetResult AddCanMapping(String json);
SetResult RemoveCanMapping(String json);
SetResult SetValue(int paramId, double value);
double GetValue(int paramId);
void RequestValue(int paramId); // Send SDO request without waiting (async)
bool TryGetValueResponse(int& outParamId, double& outValue, int timeoutMs); // Try to receive response (async)
bool IsIdle(); // Check if CAN state machine is idle
bool SaveToFlash();
bool SendCanMessage(uint32_t canId, const uint8_t* data, uint8_t dataLength); // Send arbitrary CAN message
String StreamValues(String paramIds, int samples);
int StartUpdate(String fileName);
int GetCurrentUpdatePage();
bool IsUpdateInProgress();
int GetNodeId();
BaudRate GetBaudRate();
bool ReloadJson(); // Reload JSON for currently connected device
bool ReloadJson(uint8_t nodeId); // Reload JSON for specific device by nodeId
bool ResetDevice();

// Device management functions
String ScanDevices(uint8_t startNodeId, uint8_t endNodeId);
String GetSavedDevices();
bool SaveDeviceName(String serial, String name, int nodeId = -1);
bool DeleteDevice(String serial);

// Continuous scanning functions
bool StartContinuousScan(uint8_t startNodeId = 1, uint8_t endNodeId = 32); // Returns true if scan started successfully
void StopContinuousScan();
bool IsContinuousScanActive();
void ProcessContinuousScan(); // Call this in Loop() to process scanning

// Callback type for device discoveries
typedef void (*DeviceDiscoveryCallback)(uint8_t nodeId, const char* serial, uint32_t lastSeen);
void SetDeviceDiscoveryCallback(DeviceDiscoveryCallback callback);

// Callback type for scan progress
typedef void (*ScanProgressCallback)(uint8_t currentNode, uint8_t startNode, uint8_t endNode);
void SetScanProgressCallback(ScanProgressCallback callback);

// Callback type for when connection is fully established (state = IDLE)
typedef void (*ConnectionReadyCallback)(uint8_t nodeId, const char* serial);
void SetConnectionReadyCallback(ConnectionReadyCallback callback);

// Callback type for JSON download progress
typedef void (*JsonDownloadProgressCallback)(int bytesReceived);
void SetJsonDownloadProgressCallback(JsonDownloadProgressCallback callback);

// Callback type for streaming JSON data chunks as they arrive from CAN
typedef void (*JsonStreamCallback)(const char* chunk, int chunkSize, bool isComplete);
void SetJsonStreamCallback(JsonStreamCallback callback);

// Heartbeat functions to check device status
void ProcessHeartbeat(); // Legacy - may be removed in future (now using passive heartbeats)
void UpdateDeviceLastSeen(const char* serial, uint32_t lastSeen); // Update lastSeen and notify clients
void UpdateDeviceLastSeenByNodeId(uint8_t nodeId, uint32_t lastSeen); // Update lastSeen by nodeId (passive heartbeat)

// Device list management
void LoadDevices(); // Load devices from file into memory at startup
void AddOrUpdateDevice(const char* serial, uint8_t nodeId, const char* name = nullptr, uint32_t lastSeen = 0); // Add/update device in memory

// JSON download info
int GetJsonTotalSize(); // Get total size of JSON being downloaded (0 if unknown)

}
#endif
