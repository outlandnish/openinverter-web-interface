#include "device_cache.h"

#include "device_storage.h"

#define DBG_OUTPUT_PORT Serial

void DeviceCache::ensureLoaded() {
  if (loaded_) {
    return;
  }

  if (DeviceStorage::loadDevices(cachedDevices_)) {
    loaded_ = true;
    DBG_OUTPUT_PORT.println("[DeviceCache] Loaded devices.json into cache");
  } else {
    // Initialize with empty structure if file doesn't exist
    cachedDevices_.clear();
    cachedDevices_["devices"].to<JsonObject>();
    loaded_ = true;
    DBG_OUTPUT_PORT.println("[DeviceCache] No devices.json found, initialized empty cache");
  }
}

void DeviceCache::invalidate() {
  loaded_ = false;
  cachedDevices_.clear();
  DBG_OUTPUT_PORT.println("[DeviceCache] Cache invalidated");
}

JsonDocument& DeviceCache::getDevices() {
  ensureLoaded();
  return cachedDevices_;
}

bool DeviceCache::hasDevice(const char* serial) {
  ensureLoaded();

  if (!cachedDevices_.containsKey("devices")) {
    return false;
  }

  JsonObject devices = cachedDevices_["devices"].as<JsonObject>();
  return devices.containsKey(serial);
}

std::string DeviceCache::getDeviceName(const char* serial) {
  ensureLoaded();

  if (!cachedDevices_.containsKey("devices")) {
    return "";
  }

  JsonObject devices = cachedDevices_["devices"].as<JsonObject>();
  if (!devices.containsKey(serial)) {
    return "";
  }

  JsonObject device = devices[serial].as<JsonObject>();
  if (!device.containsKey("name")) {
    return "";
  }

  return device["name"].as<std::string>();
}
