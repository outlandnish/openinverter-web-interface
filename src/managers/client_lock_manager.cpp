#include "client_lock_manager.h"

#include <Arduino.h>

#define DBG_OUTPUT_PORT Serial

ClientLockManager& ClientLockManager::instance() {
  static ClientLockManager instance;
  return instance;
}

bool ClientLockManager::tryAcquireLock(uint8_t nodeId, uint32_t clientId) {
  // Check if device is already locked by another client
  if (deviceLocks_.count(nodeId) > 0 && deviceLocks_[nodeId] != clientId) {
    return false;
  }

  // Release any previous device lock held by this client
  if (clientDevices_.count(clientId) > 0) {
    uint8_t oldNodeId = clientDevices_[clientId];
    deviceLocks_.erase(oldNodeId);
    DBG_OUTPUT_PORT.printf("[ClientLockManager] Released previous lock for node %d (client switching devices)\n",
                           oldNodeId);
  }

  // Acquire lock for new device
  deviceLocks_[nodeId] = clientId;
  clientDevices_[clientId] = nodeId;
  DBG_OUTPUT_PORT.printf("[ClientLockManager] Client #%lu acquired lock for node %d\n", (unsigned long)clientId,
                         nodeId);

  return true;
}

void ClientLockManager::releaseLock(uint8_t nodeId) {
  if (deviceLocks_.count(nodeId) > 0) {
    uint32_t clientId = deviceLocks_[nodeId];
    deviceLocks_.erase(nodeId);
    clientDevices_.erase(clientId);
    DBG_OUTPUT_PORT.printf("[ClientLockManager] Released lock for node %d\n", nodeId);
  }
}

void ClientLockManager::releaseClientLocks(uint32_t clientId) {
  if (clientDevices_.count(clientId) > 0) {
    uint8_t nodeId = clientDevices_[clientId];
    deviceLocks_.erase(nodeId);
    clientDevices_.erase(clientId);
    DBG_OUTPUT_PORT.printf("[ClientLockManager] Released lock for node %d (client #%lu disconnected)\n", nodeId,
                           (unsigned long)clientId);
  }
}

bool ClientLockManager::isDeviceLocked(uint8_t nodeId) const {
  return deviceLocks_.count(nodeId) > 0;
}

bool ClientLockManager::isDeviceLockedByClient(uint8_t nodeId, uint32_t clientId) const {
  auto it = deviceLocks_.find(nodeId);
  return it != deviceLocks_.end() && it->second == clientId;
}

uint32_t ClientLockManager::getLockHolder(uint8_t nodeId) const {
  auto it = deviceLocks_.find(nodeId);
  return it != deviceLocks_.end() ? it->second : 0;
}

uint8_t ClientLockManager::getClientDevice(uint32_t clientId) const {
  auto it = clientDevices_.find(clientId);
  return it != clientDevices_.end() ? it->second : 0;
}

bool ClientLockManager::hasClientLock(uint32_t clientId) const {
  return clientDevices_.count(clientId) > 0;
}
