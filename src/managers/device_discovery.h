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
#pragma once

#include <map>
#include <functional>
#include <Arduino.h>
#include "driver/twai.h"
#include "models/can_types.h"

class DeviceDiscovery {
public:
  // Callback types
  using DiscoveryCallback = std::function<void(uint8_t nodeId, const char* serial, uint32_t lastSeen)>;
  using ProgressCallback = std::function<void(uint8_t currentNode, uint8_t startNode, uint8_t endNode)>;

  // Device info structure
  struct Device {
    String serial;
    uint8_t nodeId;
    String name;
    uint32_t lastSeen;
  };

  // Singleton instance
  static DeviceDiscovery& instance();

  // Scanning operations
  String scanDevices(uint8_t startNode, uint8_t endNode, uint8_t& nodeId, BaudRate baudRate, int canTxPin, int canRxPin);
  bool startContinuousScan(uint8_t startNode = 1, uint8_t endNode = 32);
  void stopContinuousScan();
  bool isScanActive() const;
  void processScan(); // Called from main loop

  // Callbacks
  void setDiscoveryCallback(DiscoveryCallback cb);
  void setProgressCallback(ProgressCallback cb);

  // Device list management
  void loadDevices();
  void addOrUpdateDevice(const char* serial, uint8_t nodeId, const char* name = nullptr, uint32_t lastSeen = 0);
  void updateLastSeen(const char* serial, uint32_t lastSeen);
  void updateLastSeenByNodeId(uint8_t nodeId, uint32_t lastSeen);
  const std::map<String, Device>& getDevices() const;

  // Device persistence
  String getSavedDevices();
  bool saveDeviceName(String serial, String name, int nodeId = -1);
  bool deleteDevice(String serial);

private:
  DeviceDiscovery();
  DeviceDiscovery(const DeviceDiscovery&) = delete;
  DeviceDiscovery& operator=(const DeviceDiscovery&) = delete;

  // Scanning state
  bool scanActive = false;
  uint8_t scanStart = 1;
  uint8_t scanEnd = 32;
  uint8_t currentNode = 1;
  uint8_t currentSerialPart = 0;
  uint32_t currentSerial[4];
  unsigned long lastScanTime = 0;

  // Throttle passive heartbeat updates to prevent flooding WebSocket
  static const unsigned long PASSIVE_HEARTBEAT_THROTTLE_MS = 1000; // Update at most once per second
  std::map<uint8_t, unsigned long> lastPassiveHeartbeatByNode; // nodeId -> last update time

  // Device list
  std::map<String, Device> devices;

  // Callbacks
  DiscoveryCallback discoveryCallback = nullptr;
  ProgressCallback progressCallback = nullptr;

  // Helper functions
  void advanceToNextNode();
  bool handleScanResponse(const twai_message_t& frame, unsigned long currentTime);
  bool shouldProcessScan(unsigned long currentTime) const;
  bool isValidSerialResponse(const twai_message_t& frame, uint8_t nodeId, uint8_t partIndex) const;
  bool requestDeviceSerial(uint8_t nodeId, uint32_t serialParts[4]);

  // Constants
  static const unsigned long SCAN_DELAY_MS = 50; // Delay between node probes
  static const unsigned long SCAN_TIMEOUT_MS = 100; // Timeout for scan response
};
