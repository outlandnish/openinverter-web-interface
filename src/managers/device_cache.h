#pragma once

#include <ArduinoJson.h>
#include <Arduino.h>
#include <string>

// Singleton cache for devices.json with device name lookup helper
// Caches the devices document in memory and provides efficient lookups
// Cache is invalidated when devices are modified via DeviceStorage::saveDevices()
class DeviceCache {
public:
    static DeviceCache& instance() {
        static DeviceCache cache;
        return cache;
    }

    // Look up device name by serial number
    // Returns empty string if device not found or has no name
    std::string getDeviceName(const char* serial);

    // Invalidate cache (call when devices.json is modified)
    void invalidate();

    // Get the entire cached devices document
    // Loads from disk if not cached
    JsonDocument& getDevices();

    // Check if a device exists in the cache
    bool hasDevice(const char* serial);

private:
    DeviceCache() : loaded_(false) {}
    DeviceCache(const DeviceCache&) = delete;
    DeviceCache& operator=(const DeviceCache&) = delete;

    void ensureLoaded();

    JsonDocument cachedDevices_;
    bool loaded_;
};
