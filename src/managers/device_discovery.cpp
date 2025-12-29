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
#include "device_discovery.h"
#include "device_connection.h"
#include "device_storage.h"
#include "models/can_types.h"
#include "oi_can.h"
#include "utils/can_queue.h"
#include "protocols/sdo_protocol.h"
#include "can_task.h"
#include <ArduinoJson.h>
#include <LittleFS.h>

#define DBG_OUTPUT_PORT Serial

// SDO protocol constants (duplicated from oi_can.cpp - could be extracted to a protocol header)
#define SDO_READ              (2 << 5)
#define SDO_ABORT             0x80
#define SDO_INDEX_SERIAL      0x5000

// Singleton instance
DeviceDiscovery& DeviceDiscovery::instance() {
  static DeviceDiscovery instance;
  return instance;
}

DeviceDiscovery::DeviceDiscovery() {
  // Constructor
}

// Helper function: Check if response is valid for serial request
bool DeviceDiscovery::isValidSerialResponse(const twai_message_t& frame, uint8_t nodeId, uint8_t partIndex) const {
  uint16_t rxIndex = (frame.data[1] | (frame.data[2] << 8));
  return frame.identifier == (SDO_RESPONSE_BASE_ID | nodeId) &&
         frame.data[0] != SDO_ABORT &&
         rxIndex == SDO_INDEX_SERIAL &&
         frame.data[3] == partIndex;
}

// Helper function: Advance to next node in scan
void DeviceDiscovery::advanceToNextNode() {
  currentSerialPart = 0;
  currentNode++;
  if (currentNode > scanEnd) {
    currentNode = scanStart; // Wrap around to start
  }
}

// Helper function: Check if we should process scan now
bool DeviceDiscovery::shouldProcessScan(unsigned long currentTime) const {
  return scanActive &&
         DeviceConnection::instance().isIdle() &&
         (currentTime - lastScanTime >= SCAN_DELAY_MS);
}

// Helper function: Handle scan response
bool DeviceDiscovery::handleScanResponse(const twai_message_t& frame, unsigned long currentTime) {
  if (!isValidSerialResponse(frame, currentNode, currentSerialPart)) {
    return false;
  }

  currentSerial[currentSerialPart] = *(uint32_t*)&frame.data[4];
  currentSerialPart++;

  // If we've read all 4 parts, we found a device
  if (currentSerialPart >= 4) {
    char serialStr[40];
    sprintf(serialStr, "%08" PRIX32 ":%08" PRIX32 ":%08" PRIX32 ":%08" PRIX32,
            currentSerial[0], currentSerial[1], currentSerial[2], currentSerial[3]);

    DBG_OUTPUT_PORT.printf("Continuous scan found device at node %d: %s\n", currentNode, serialStr);

    // Update in-memory device list (not saved to file until user names it)
    addOrUpdateDevice(serialStr, currentNode, nullptr, currentTime);

    // Notify via callback (will broadcast to WebSocket clients)
    if (discoveryCallback) {
      discoveryCallback(currentNode, serialStr, currentTime);
    }

    // Move to next node
    advanceToNextNode();
  }

  return true;
}

// Helper function: Request device serial number from a node
// Returns true if all 4 serial parts were successfully received
bool DeviceDiscovery::requestDeviceSerial(uint8_t nodeId, uint32_t serialParts[4]) {
  twai_message_t tx_frame;
  tx_frame.extd = false;
  tx_frame.identifier = SDO_REQUEST_BASE_ID | nodeId;
  tx_frame.data_length_code = 8;
  tx_frame.data[0] = SDO_READ;
  tx_frame.data[4] = 0;
  tx_frame.data[5] = 0;
  tx_frame.data[6] = 0;
  tx_frame.data[7] = 0;

  for (uint8_t part = 0; part < 4; part++) {
    tx_frame.data[1] = SDO_INDEX_SERIAL & 0xFF;
    tx_frame.data[2] = SDO_INDEX_SERIAL >> 8;
    tx_frame.data[3] = part;

    if (!canQueueTransmit(&tx_frame, pdMS_TO_TICKS(10))) {
      return false;
    }

    twai_message_t rxframe;
    if (!SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(100))) {
      return false;
    }

    if (!isValidSerialResponse(rxframe, nodeId, part)) {
      return false;
    }

    serialParts[part] = *(uint32_t*)&rxframe.data[4];
  }
  return true;
}

// Scan for devices on the CAN bus (one-time scan)
String DeviceDiscovery::scanDevices(uint8_t startNode, uint8_t endNode, uint8_t& nodeId, BaudRate baudRate, int canTxPin, int canRxPin) {
  if (!DeviceConnection::instance().isIdle()) return "[]";

  JsonDocument doc;
  JsonArray devicesArray = doc.to<JsonArray>();

  // Load saved devices to update them
  JsonDocument savedDoc;
  DeviceStorage::loadDevices(savedDoc);

  // Ensure devices object exists in saved doc
  if (!savedDoc.containsKey("devices")) {
    savedDoc.createNestedObject("devices");
  }

  JsonObject savedDevices = savedDoc["devices"].as<JsonObject>();
  bool devicesUpdated = false;

  DBG_OUTPUT_PORT.printf("Scanning CAN bus for devices (nodes %d-%d)...\n", startNode, endNode);

  // Save current node ID
  uint8_t prevNodeId = nodeId;

  for (uint8_t node = startNode; node <= endNode; node++) {
    uint32_t deviceSerial[4];

    DBG_OUTPUT_PORT.printf("Probing node %d...\n", node);

    // Temporarily set the node ID for scanning
    nodeId = node;

    // Request serial number from device
    if (requestDeviceSerial(node, deviceSerial)) {
      char serialStr[40];
      sprintf(serialStr, "%08" PRIX32 ":%08" PRIX32 ":%08" PRIX32 ":%08" PRIX32,
              deviceSerial[0], deviceSerial[1], deviceSerial[2], deviceSerial[3]);

      DBG_OUTPUT_PORT.printf("Found device at node %d: %s\n", node, serialStr);

      // Add to scan results
      JsonObject device = devicesArray.add<JsonObject>();
      device["nodeId"] = node;
      device["serial"] = serialStr;
      device["lastSeen"] = millis();

      // Update saved devices with new nodeId and lastSeen
      DeviceStorage::updateDeviceInJson(savedDevices, serialStr, node);
      devicesUpdated = true;

      DBG_OUTPUT_PORT.printf("Updated stored nodeId for %s to %d\n", serialStr, node);
    }
  }

  // Restore previous node ID
  nodeId = prevNodeId;

  // Save updated devices back to LittleFS
  if (devicesUpdated) {
    if (DeviceStorage::saveDevices(savedDoc)) {
      DBG_OUTPUT_PORT.println("Updated devices.json with new nodeIds");
    }
  }

  String result;
  serializeJson(doc, result);
  DBG_OUTPUT_PORT.printf("Scan complete. Found %d devices\n", devicesArray.size());

  return result;
}

// Start continuous scanning
bool DeviceDiscovery::startContinuousScan(uint8_t startNode, uint8_t endNode) {
  if (!DeviceConnection::instance().isIdle()) {
    DBG_OUTPUT_PORT.printf("Cannot start continuous scan - device busy\n");
    return false;
  }

  // Note: CAN bus reinitialization will be handled by OICan::InitCAN() from caller
  scanActive = true;
  scanStart = startNode;
  scanEnd = endNode;
  currentNode = startNode;
  currentSerialPart = 0;
  lastScanTime = 0;

  DBG_OUTPUT_PORT.printf("Started continuous CAN scan (nodes %d-%d)\n", startNode, endNode);
  return true;
}

// Stop continuous scanning
void DeviceDiscovery::stopContinuousScan() {
  scanActive = false;
  DBG_OUTPUT_PORT.println("Stopped continuous CAN scan");
}

// Check if scan is active
bool DeviceDiscovery::isScanActive() const {
  return scanActive;
}

// Process continuous scan (called from main loop)
void DeviceDiscovery::processScan() {
  unsigned long currentTime = millis();

  if (!shouldProcessScan(currentTime)) {
    return;
  }

  lastScanTime = currentTime;

  DBG_OUTPUT_PORT.printf("[Scan] Probing node %d, part %d\n", currentNode, currentSerialPart);

  // Clear any stale responses before sending new request
  SDOProtocol::clearPendingResponses();

  // Request serial number part from current scan node
  twai_message_t tx_frame;
  tx_frame.extd = false;
  tx_frame.identifier = SDO_REQUEST_BASE_ID | currentNode;
  tx_frame.data_length_code = 8;
  tx_frame.data[0] = SDO_READ;
  tx_frame.data[1] = SDO_INDEX_SERIAL & 0xFF;
  tx_frame.data[2] = SDO_INDEX_SERIAL >> 8;
  tx_frame.data[3] = currentSerialPart;
  tx_frame.data[4] = 0;
  tx_frame.data[5] = 0;
  tx_frame.data[6] = 0;
  tx_frame.data[7] = 0;

  bool txResult = canQueueTransmit(&tx_frame, pdMS_TO_TICKS(10));
  DBG_OUTPUT_PORT.printf("[Scan] TX queued: %s\n", txResult ? "OK" : "FAILED");

  // Flush TX queue immediately so the frame is transmitted before we wait
  flushCanTxQueue();

  // Notify scan progress when starting a new node (first serial part)
  if (currentSerialPart == 0 && progressCallback) {
    progressCallback(currentNode, scanStart, scanEnd);
  }

  twai_message_t rxframe;
  bool gotResponse = SDOProtocol::waitForResponse(&rxframe, pdMS_TO_TICKS(SCAN_TIMEOUT_MS));
  DBG_OUTPUT_PORT.printf("[Scan] waitForResponse: %s\n", gotResponse ? "GOT RESPONSE" : "TIMEOUT");

  if (gotResponse) {
    DBG_OUTPUT_PORT.printf("[Scan] Response ID=0x%03lX Data[0]=0x%02X\n",
                           (unsigned long)rxframe.identifier, rxframe.data[0]);
    if (!handleScanResponse(rxframe, currentTime)) {
      // No response or error - move to next node
      DBG_OUTPUT_PORT.println("[Scan] Invalid response, advancing to next node");
      advanceToNextNode();
    }
  } else {
    // Timeout - move to next node
    advanceToNextNode();
  }
}

// Set discovery callback
void DeviceDiscovery::setDiscoveryCallback(DiscoveryCallback cb) {
  discoveryCallback = cb;
}

// Set progress callback
void DeviceDiscovery::setProgressCallback(ProgressCallback cb) {
  progressCallback = cb;
}

// Load devices from file into memory
void DeviceDiscovery::loadDevices() {
  devices.clear();

  JsonDocument doc;
  if (!DeviceStorage::loadDevices(doc)) {
    DBG_OUTPUT_PORT.println("No devices.json file, starting with empty device list");
    return;
  }

  if (!doc.containsKey("devices")) {
    DBG_OUTPUT_PORT.println("No 'devices' key in devices.json");
    return;
  }

  JsonObject devicesObj = doc["devices"].as<JsonObject>();
  int count = 0;

  for (JsonPair kv : devicesObj) {
    Device dev;
    dev.serial = kv.key().c_str();

    JsonObject deviceObj = kv.value().as<JsonObject>();
    dev.nodeId = deviceObj["nodeId"] | 0;
    dev.name = deviceObj["name"] | "";
    dev.lastSeen = deviceObj["lastSeen"] | 0;

    devices[dev.serial] = dev;
    count++;
  }

  DBG_OUTPUT_PORT.printf("Loaded %d devices from file\n", count);
}

// Add or update device in memory
void DeviceDiscovery::addOrUpdateDevice(const char* serial, uint8_t nodeId, const char* name, uint32_t lastSeen) {
  Device dev;

  // Check if device already exists
  if (devices.count(serial) > 0) {
    dev = devices[serial];
  } else {
    dev.serial = serial;
  }

  // Update fields
  if (nodeId > 0) {
    dev.nodeId = nodeId;
  }
  if (name != nullptr && strlen(name) > 0) {
    dev.name = name;
  }
  if (lastSeen > 0) {
    dev.lastSeen = lastSeen;
  }

  devices[serial] = dev;
}

// Update device last seen timestamp
void DeviceDiscovery::updateLastSeen(const char* serial, uint32_t lastSeen) {
  // Update in-memory device list only (not saved to file)
  if (devices.count(serial) > 0) {
    devices[serial].lastSeen = lastSeen;

    // Notify via callback (will broadcast to WebSocket clients)
    if (discoveryCallback) {
      uint8_t nodeId = devices[serial].nodeId;
      discoveryCallback(nodeId, serial, lastSeen);
    }
  }
}

// Update device last seen by node ID
void DeviceDiscovery::updateLastSeenByNodeId(uint8_t nodeId, uint32_t lastSeen) {
  // Throttle updates to prevent flooding WebSocket with too many messages
  // Only update if enough time has passed since last update for this node
  if (lastPassiveHeartbeatByNode.count(nodeId) > 0) {
    unsigned long timeSinceLastUpdate = lastSeen - lastPassiveHeartbeatByNode[nodeId];
    if (timeSinceLastUpdate < PASSIVE_HEARTBEAT_THROTTLE_MS) {
      return; // Too soon, skip this update
    }
  }

  // Record this update time
  lastPassiveHeartbeatByNode[nodeId] = lastSeen;

  // Find device by nodeId and update its lastSeen
  for (auto& kv : devices) {
    if (kv.second.nodeId == nodeId) {
      updateLastSeen(kv.first.c_str(), lastSeen);
      return;
    }
  }
}

// Get devices map
const std::map<String, DeviceDiscovery::Device>& DeviceDiscovery::getDevices() const {
  return devices;
}

// Get saved devices as JSON string
String DeviceDiscovery::getSavedDevices() {
  JsonDocument doc;
  JsonObject devicesObj = doc["devices"].to<JsonObject>();

  // Build JSON from in-memory device list
  for (auto& kv : devices) {
    const Device& dev = kv.second;
    JsonObject deviceObj = devicesObj[dev.serial].to<JsonObject>();
    deviceObj["nodeId"] = dev.nodeId;
    deviceObj["name"] = dev.name;
    deviceObj["lastSeen"] = dev.lastSeen;
  }

  String result;
  serializeJson(doc, result);
  return result;
}

// Save device name to file
bool DeviceDiscovery::saveDeviceName(String serial, String name, int nodeId) {
  JsonDocument doc;

  // Load existing devices
  DeviceStorage::loadDevices(doc);

  // Ensure devices object exists
  if (!doc.containsKey("devices")) {
    doc.createNestedObject("devices");
  }

  JsonObject devicesObj = doc["devices"].as<JsonObject>();

  // Get or create device object (serial is the key)
  if (!devicesObj.containsKey(serial)) {
    devicesObj.createNestedObject(serial);
  }

  JsonObject device = devicesObj[serial].as<JsonObject>();
  device["name"] = name;

  if (nodeId >= 0) {
    device["nodeId"] = nodeId;
  }

  DBG_OUTPUT_PORT.printf("Saved device: %s -> %s (nodeId: %d)\n", serial.c_str(), name.c_str(), nodeId);

  // Save back to file
  if (!DeviceStorage::saveDevices(doc)) {
    DBG_OUTPUT_PORT.println("Failed to save devices file");
    return false;
  }

  // Also update in-memory list
  addOrUpdateDevice(serial.c_str(), nodeId >= 0 ? nodeId : 0, name.c_str(), 0);

  DBG_OUTPUT_PORT.println("Saved devices file and updated in-memory list");
  return true;
}

// Delete device from file and memory
bool DeviceDiscovery::deleteDevice(String serial) {
  JsonDocument doc;

  // Load existing devices
  DeviceStorage::loadDevices(doc);

  // Check if devices object exists
  if (!doc.containsKey("devices")) {
    DBG_OUTPUT_PORT.println("No devices to delete");
    return false;
  }

  JsonObject devicesObj = doc["devices"].as<JsonObject>();

  // Check if device exists
  if (!devicesObj.containsKey(serial)) {
    DBG_OUTPUT_PORT.printf("Device %s not found\n", serial.c_str());
    return false;
  }

  // Remove device
  devicesObj.remove(serial);

  DBG_OUTPUT_PORT.printf("Deleted device: %s\n", serial.c_str());

  // Save back to file
  if (!DeviceStorage::saveDevices(doc)) {
    DBG_OUTPUT_PORT.println("Failed to save devices file");
    return false;
  }

  // Also remove from in-memory list
  devices.erase(serial.c_str());

  DBG_OUTPUT_PORT.println("Deleted device from file and in-memory list");
  return true;
}
