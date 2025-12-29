#include "can_interval_manager.h"
#include "../oi_can.h"
#include "../utils/can_io_utils.h"
#include <Arduino.h>

#define DBG_OUTPUT_PORT Serial

CanIntervalManager& CanIntervalManager::instance() {
    static CanIntervalManager instance;
    return instance;
}

CanIntervalManager::CanIntervalManager() {
    // Initialize CAN IO interval to default inactive state
    canIoInterval_.active = false;
    canIoInterval_.canId = 0x3F;
    canIoInterval_.pot = 0;
    canIoInterval_.pot2 = 0;
    canIoInterval_.canio = 0;
    canIoInterval_.cruisespeed = 0;
    canIoInterval_.regenpreset = 0;
    canIoInterval_.intervalMs = 100;
    canIoInterval_.lastSentTime = 0;
    canIoInterval_.sequenceCounter = 0;
    canIoInterval_.useCrc = false;
}

void CanIntervalManager::startInterval(const char* intervalId, uint32_t canId,
                                        const uint8_t* data, uint8_t dataLength,
                                        uint32_t intervalMs) {
    // Remove existing interval with same ID
    stopInterval(intervalId);

    // Add new interval message
    IntervalCanMessage msg;
    msg.id = String(intervalId);
    msg.canId = canId;
    msg.dataLength = dataLength;
    for (uint8_t i = 0; i < dataLength && i < 8; i++) {
        msg.data[i] = data[i];
    }
    msg.intervalMs = intervalMs;
    msg.lastSentTime = millis();
    intervalMessages_.push_back(msg);

    DBG_OUTPUT_PORT.printf("[CanIntervalManager] Started interval: ID=%s, CAN=0x%03lX, Interval=%lums\n",
                           intervalId, (unsigned long)canId, (unsigned long)intervalMs);
}

void CanIntervalManager::stopInterval(const char* intervalId) {
    for (auto it = intervalMessages_.begin(); it != intervalMessages_.end(); ) {
        if (it->id == intervalId) {
            DBG_OUTPUT_PORT.printf("[CanIntervalManager] Stopped interval: ID=%s\n", intervalId);
            it = intervalMessages_.erase(it);
        } else {
            ++it;
        }
    }
}

void CanIntervalManager::clearAllIntervals() {
    if (!intervalMessages_.empty()) {
        DBG_OUTPUT_PORT.printf("[CanIntervalManager] Clearing %d interval message(s)\n",
                               intervalMessages_.size());
        intervalMessages_.clear();
    }
}

bool CanIntervalManager::hasInterval(const char* intervalId) const {
    for (const auto& msg : intervalMessages_) {
        if (msg.id == intervalId) {
            return true;
        }
    }
    return false;
}

void CanIntervalManager::sendPendingMessages() {
    uint32_t currentTime = millis();
    for (auto& msg : intervalMessages_) {
        if ((currentTime - msg.lastSentTime) >= msg.intervalMs) {
            msg.lastSentTime = currentTime;
            OICan::SendCanMessage(msg.canId, msg.data, msg.dataLength);
        }
    }
}

void CanIntervalManager::startCanIoInterval(uint32_t canId, uint16_t pot, uint16_t pot2,
                                             uint8_t canio, uint16_t cruisespeed,
                                             uint8_t regenpreset, uint32_t intervalMs,
                                             bool useCrc) {
    canIoInterval_.active = true;
    canIoInterval_.canId = canId;
    canIoInterval_.pot = pot;
    canIoInterval_.pot2 = pot2;
    canIoInterval_.canio = canio;
    canIoInterval_.cruisespeed = cruisespeed;
    canIoInterval_.regenpreset = regenpreset;
    canIoInterval_.intervalMs = intervalMs;
    canIoInterval_.useCrc = useCrc;
    canIoInterval_.lastSentTime = millis();
    // Start with counter=1 to avoid matching the last message from a previous session
    canIoInterval_.sequenceCounter = 1;

    DBG_OUTPUT_PORT.printf("[CanIntervalManager] Started CAN IO interval: CAN=0x%03lX, canio=0x%02X, Interval=%lums\n",
                           (unsigned long)canIoInterval_.canId, canIoInterval_.canio,
                           (unsigned long)canIoInterval_.intervalMs);
}

void CanIntervalManager::stopCanIoInterval() {
    canIoInterval_.active = false;
    DBG_OUTPUT_PORT.println("[CanIntervalManager] Stopped CAN IO interval");
}

void CanIntervalManager::updateCanIoFlags(uint16_t pot, uint16_t pot2, uint8_t canio,
                                           uint16_t cruisespeed, uint8_t regenpreset) {
    if (canIoInterval_.active) {
        canIoInterval_.pot = pot;
        canIoInterval_.pot2 = pot2;
        canIoInterval_.canio = canio;
        canIoInterval_.cruisespeed = cruisespeed;
        canIoInterval_.regenpreset = regenpreset;
        DBG_OUTPUT_PORT.printf("[CanIntervalManager] Updated CAN IO flags (canio=0x%02X)\n", canio);
    } else {
        DBG_OUTPUT_PORT.println("[CanIntervalManager] Ignoring update - CAN IO interval not active");
    }
}

void CanIntervalManager::sendCanIoMessage() {
    if (!canIoInterval_.active) {
        return;
    }

    uint32_t currentTime = millis();
    if ((currentTime - canIoInterval_.lastSentTime) >= canIoInterval_.intervalMs) {
        canIoInterval_.lastSentTime = currentTime;

        // Build the CAN IO message with current state and sequence counter
        uint8_t msgData[8];
        buildCanIoMessage(msgData, canIoInterval_.pot, canIoInterval_.pot2,
                          canIoInterval_.canio, canIoInterval_.sequenceCounter,
                          canIoInterval_.cruisespeed, canIoInterval_.regenpreset,
                          canIoInterval_.useCrc);

        // Send the message
        OICan::SendCanMessage(canIoInterval_.canId, msgData, 8);

        // Increment sequence counter (0-3)
        canIoInterval_.sequenceCounter = (canIoInterval_.sequenceCounter + 1) & 0x03;
    }
}
