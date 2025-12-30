#pragma once

#include <cstdint>
#include <map>

/**
 * Manages device locks for multi-client WebSocket support.
 * Ensures only one client can control a device at a time.
 * Uses singleton pattern since there's only one locking system.
 */
class ClientLockManager {
public:
  // Singleton access
  static ClientLockManager& instance();

  // Lock management
  bool tryAcquireLock(uint8_t nodeId, uint32_t clientId);
  void releaseLock(uint8_t nodeId);
  void releaseClientLocks(uint32_t clientId);

  // Query methods
  bool isDeviceLocked(uint8_t nodeId) const;
  bool isDeviceLockedByClient(uint8_t nodeId, uint32_t clientId) const;
  uint32_t getLockHolder(uint8_t nodeId) const;
  uint8_t getClientDevice(uint32_t clientId) const;
  bool hasClientLock(uint32_t clientId) const;

private:
  ClientLockManager() = default;
  ClientLockManager(const ClientLockManager&) = delete;
  ClientLockManager& operator=(const ClientLockManager&) = delete;

  std::map<uint8_t, uint32_t> deviceLocks_;    // nodeId -> WebSocket client ID
  std::map<uint32_t, uint8_t> clientDevices_;  // WebSocket client ID -> nodeId
};
