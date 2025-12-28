#include "can_utils.h"

// CRC-32 calculation for CAN operations (STM32 polynomial 0x04C11DB7)
// This matches the IEEE 802.3 / Ethernet CRC-32 polynomial
uint32_t crc32_word(uint32_t crc, uint32_t word) {
  const uint32_t polynomial = 0x04C11DB7;
  crc ^= word;
  for (int i = 0; i < 32; i++) {
    if (crc & 0x80000000) {
      crc = (crc << 1) ^ polynomial;
    } else {
      crc = crc << 1;
    }
  }
  return crc;
}
