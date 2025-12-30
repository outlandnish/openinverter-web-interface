#pragma once
#include <stdint.h>

#include "driver/twai.h"

// CRC-32 calculation for CAN operations (STM32 polynomial 0x04C11DB7)
// This matches the IEEE 802.3 / Ethernet CRC-32 polynomial
// Used for both CAN IO messages and firmware updates
uint32_t crc32_word(uint32_t crc, uint32_t word);

// Debug utilities for CAN message tracing
// Note: Currently disabled for production, but can be enabled for debugging
void printCanTx(const twai_message_t* frame);
void printCanRx(const twai_message_t* frame);

// Response validation utilities
bool isValidSdoResponse(const twai_message_t& frame, uint8_t nodeId, uint16_t index);
