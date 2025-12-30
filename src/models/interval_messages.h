#pragma once

#include <cstdint>

#include <WString.h>  // Arduino String

// Interval CAN message sending
struct IntervalCanMessage {
  String id;
  uint32_t canId;
  uint8_t data[8];
  uint8_t dataLength;
  uint32_t intervalMs;
  uint32_t lastSentTime;
};

// CAN IO interval message sending
struct CanIoInterval {
  bool active;
  uint32_t canId;
  uint16_t pot;
  uint16_t pot2;
  uint8_t canio;
  uint16_t cruisespeed;
  uint8_t regenpreset;
  uint32_t intervalMs;
  uint32_t lastSentTime;
  uint8_t sequenceCounter;  // 2-bit counter (0-3)
  bool useCrc;              // Use CRC-32 (true) or counter-only (false)
};
