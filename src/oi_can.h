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
String GetRawJson(); // Get parameter JSON directly from device
void SendCanMapping(WiFiClient c);
SetResult AddCanMapping(String json);
SetResult RemoveCanMapping(String json);
SetResult SetValue(int paramId, double value);
double GetValue(int paramId);
bool SaveToFlash();
String StreamValues(String paramIds, int samples);
int StartUpdate(String fileName);
int GetCurrentUpdatePage();
int GetNodeId();
BaudRate GetBaudRate();
bool ReloadJson();
bool ResetDevice();

// Device management functions
String ScanDevices(uint8_t startNodeId, uint8_t endNodeId);
String GetSavedDevices();
bool SaveDeviceName(String serial, String name, int nodeId = -1);

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

// Heartbeat functions to check device status
void ProcessHeartbeat(); // Call this in Loop() to send periodic heartbeats
void UpdateDeviceLastSeen(const char* serial, uint32_t lastSeen); // Update lastSeen and notify clients

// Device list management
void LoadDevices(); // Load devices from file into memory at startup
void AddOrUpdateDevice(const char* serial, uint8_t nodeId, const char* name = nullptr, uint32_t lastSeen = 0); // Add/update device in memory

}
#endif
