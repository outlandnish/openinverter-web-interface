#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include "../models/interval_messages.h"

/**
 * Manages periodic CAN message sending - both generic interval messages
 * and the specialized CAN IO interval protocol.
 * Uses singleton pattern since there's only one set of active intervals.
 */
class CanIntervalManager {
public:
    // Singleton access
    static CanIntervalManager& instance();

    // Generic interval message management
    void startInterval(const char* intervalId, uint32_t canId, const uint8_t* data,
                       uint8_t dataLength, uint32_t intervalMs);
    void stopInterval(const char* intervalId);
    void clearAllIntervals();
    bool hasInterval(const char* intervalId) const;
    size_t getIntervalCount() const { return intervalMessages_.size(); }

    // Send pending interval messages (called from CAN task loop)
    void sendPendingMessages();

    // CAN IO interval management
    void startCanIoInterval(uint32_t canId, uint16_t pot, uint16_t pot2, uint8_t canio,
                            uint16_t cruisespeed, uint8_t regenpreset,
                            uint32_t intervalMs, bool useCrc);
    void stopCanIoInterval();
    void updateCanIoFlags(uint16_t pot, uint16_t pot2, uint8_t canio,
                          uint16_t cruisespeed, uint8_t regenpreset);
    bool isCanIoActive() const { return canIoInterval_.active; }
    uint32_t getCanIoIntervalMs() const { return canIoInterval_.intervalMs; }

    // Send CAN IO message if interval elapsed (called from CAN task loop)
    void sendCanIoMessage();

private:
    CanIntervalManager();
    CanIntervalManager(const CanIntervalManager&) = delete;
    CanIntervalManager& operator=(const CanIntervalManager&) = delete;

    std::vector<IntervalCanMessage> intervalMessages_;
    CanIoInterval canIoInterval_;
};
