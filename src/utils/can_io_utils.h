#pragma once
#include <stdint.h>

// Build CAN IO message with bit packing
// Set useCRC=true for controlcheck=1 (StmCrc8), false for controlcheck=0 (CounterOnly)
void buildCanIoMessage(uint8_t* msg, uint16_t pot, uint16_t pot2, uint8_t canio,
                       uint8_t ctr, uint16_t cruisespeed, uint8_t regenpreset,
                       bool useCRC = false);
