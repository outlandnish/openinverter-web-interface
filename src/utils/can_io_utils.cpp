#include "can_io_utils.h"

#include "can_utils.h"

#include "models/can_types.h"

// Build CAN IO message with bit packing
// Set useCRC=true for controlcheck=1 (StmCrc8), false for controlcheck=0 (CounterOnly)
void buildCanIoMessage(uint8_t* msg, uint16_t pot, uint16_t pot2, uint8_t canio, uint8_t ctr, uint16_t cruisespeed,
                       uint8_t regenpreset, bool useCRC) {
  // Mask inputs to their bit limits
  pot &= CAN_IO_POT_MASK;             // 12 bits
  pot2 &= CAN_IO_POT_MASK;            // 12 bits
  canio &= CAN_IO_CANIO_MASK;         // 6 bits
  ctr &= CAN_IO_COUNTER_MASK;         // 2 bits
  cruisespeed &= CAN_IO_CRUISE_MASK;  // 14 bits
  regenpreset &= CAN_IO_REGEN_MASK;   // 8 bits

  // data[0] (32 bits): pot (0-11), pot2 (12-23), canio (24-29), ctr1 (30-31)
  uint32_t data0 = pot | (pot2 << 12) | (canio << 24) | (ctr << 30);
  msg[0] = data0 & 0xFF;
  msg[1] = (data0 >> 8) & 0xFF;
  msg[2] = (data0 >> 16) & 0xFF;
  msg[3] = (data0 >> 24) & 0xFF;

  // data[1] (32 bits): cruisespeed (0-13), ctr2 (14-15), regenpreset (16-23), crc (24-31)
  uint32_t data1 = cruisespeed | (ctr << 14) | (regenpreset << 16);
  msg[4] = data1 & 0xFF;
  msg[5] = (data1 >> 8) & 0xFF;
  msg[6] = (data1 >> 16) & 0xFF;

  // Calculate CRC-32 if requested, otherwise set to 0
  uint8_t crcByte = 0;
  if (useCRC) {
    uint32_t crc = 0xFFFFFFFF;
    crc = crc32_word(crc, data0);
    crc = crc32_word(crc, data1);
    crcByte = crc & 0xFF;  // Lower 8 bits
  }
  msg[7] = crcByte;
}
