#pragma once
#include <ArduinoJson.h>
#include <Arduino.h>

// Device storage manager for handling device list and JSON cache file operations
class DeviceStorage {
public:
  // Device list operations (devices.json)
  static bool loadDevices(JsonDocument& doc);
  static bool saveDevices(const JsonDocument& doc);
  static void updateDeviceInJson(JsonObject& savedDevices, const char* serial, uint8_t nodeId);

  // JSON parameter cache operations
  static bool hasJsonCache(const String& serial);
  static bool removeJsonCache(const String& serial);

  // Utility
  static String getJsonFileName(const String& serial);
};
