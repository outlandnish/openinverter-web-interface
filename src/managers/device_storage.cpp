#include "device_storage.h"

#include <LittleFS.h>

#include "device_cache.h"

#define DBG_OUTPUT_PORT Serial

// Load devices.json from LittleFS
bool DeviceStorage::loadDevices(JsonDocument& doc) {
  if (!LittleFS.exists("/devices.json")) {
    return false;
  }

  File file = LittleFS.open("/devices.json", "r");
  if (!file) {
    return false;
  }

  DeserializationError error = deserializeJson(doc, file);
  file.close();
  return error == DeserializationError::Ok;
}

// Save devices.json to LittleFS
bool DeviceStorage::saveDevices(const JsonDocument& doc) {
  File file = LittleFS.open("/devices.json", "w");
  if (!file) {
    return false;
  }

  serializeJson(doc, file);
  file.close();

  // Invalidate cache since devices have been modified
  DeviceCache::instance().invalidate();

  return true;
}

// Update or add a device in the devices JSON object
void DeviceStorage::updateDeviceInJson(JsonObject& savedDevices, const char* serial, uint8_t nodeId) {
  if (!savedDevices.containsKey(serial)) {
    savedDevices.createNestedObject(serial);
  }
  JsonObject savedDevice = savedDevices[serial].as<JsonObject>();
  savedDevice["nodeId"] = nodeId;
  savedDevice["lastSeen"] = millis();
}

// Check if JSON cache file exists for a device
bool DeviceStorage::hasJsonCache(const String& serial) {
  String filename = getJsonFileName(serial);
  return LittleFS.exists(filename.c_str());
}

// Remove JSON cache file for a device
bool DeviceStorage::removeJsonCache(const String& serial) {
  String filename = getJsonFileName(serial);
  if (LittleFS.exists(filename.c_str())) {
    LittleFS.remove(filename.c_str());
    DBG_OUTPUT_PORT.printf("Removed cached JSON file: %s\r\n", filename.c_str());
    return true;
  }
  return false;
}

// Get the JSON cache filename for a device serial number
String DeviceStorage::getJsonFileName(const String& serial) {
  // Extract the last part of the serial (after the last colon)
  int lastColon = serial.lastIndexOf(':');
  String serialPart = (lastColon >= 0) ? serial.substring(lastColon + 1) : serial;
  return "/" + serialPart + ".json";
}
