#pragma once
#include <stdint.h>

// CRC-32 calculation for CAN operations (STM32 polynomial 0x04C11DB7)
// This matches the IEEE 802.3 / Ethernet CRC-32 polynomial
// Used for both CAN IO messages and firmware updates
uint32_t crc32_word(uint32_t crc, uint32_t word);
