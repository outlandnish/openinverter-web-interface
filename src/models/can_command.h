#pragma once

#include <cstdint>

#include "can_types.h"

// Command payload structures
struct ScanCommand {
  uint8_t start;
  uint8_t end;
};

struct ConnectCommand {
  uint8_t nodeId;
  char serial[50];
};

struct SetNodeIdCommand {
  uint8_t nodeId;
};

struct SetDeviceNameCommand {
  char serial[50];
  char name[50];
  int nodeId;
};

struct SpotValuesCommand {
  int paramIds[MAX_PARAM_IDS];
  int paramCount;
  uint32_t interval;
};

struct DeleteDeviceCommand {
  char serial[50];
};

struct RenameDeviceCommand {
  char serial[50];
  char name[50];
};

struct SendCanMessageCommand {
  uint32_t canId;
  uint8_t data[8];
  uint8_t dataLength;
};

struct StartCanIntervalCommand {
  uint32_t canId;
  uint8_t data[8];
  uint8_t dataLength;
  uint32_t intervalMs;
  char intervalId[32];  // Unique ID for this interval message
};

struct StopCanIntervalCommand {
  char intervalId[32];  // ID of interval to stop
};

struct StartCanIoIntervalCommand {
  uint32_t canId;        // CAN ID (default 0x3F)
  uint16_t pot;          // Throttle 1 (12 bits, 0-4095)
  uint16_t pot2;         // Throttle 2 (12 bits, 0-4095)
  uint8_t canio;         // Digital I/O flags (6 bits)
  uint16_t cruisespeed;  // Cruise speed (14 bits, 0-16383)
  uint8_t regenpreset;   // Regen preset (8 bits, 0-255)
  uint32_t intervalMs;   // Send interval in milliseconds
  bool useCrc;           // Use CRC-32 (true) or counter-only (false)
};

struct UpdateCanIoFlagsCommand {
  uint16_t pot;          // Throttle 1 (12 bits, 0-4095)
  uint16_t pot2;         // Throttle 2 (12 bits, 0-4095)
  uint8_t canio;         // Digital I/O flags (6 bits)
  uint16_t cruisespeed;  // Cruise speed (14 bits, 0-16383)
  uint8_t regenpreset;   // Regen preset (8 bits, 0-255)
};

// Task 34: Device command payload structures

struct StartDeviceCommand {
  uint32_t mode;  // Start mode (0 = normal)
};

struct SetValueCommand {
  int paramId;   // Parameter ID to set
  double value;  // Value to set
};

struct ClearCanMapCommand {
  bool isRx;  // true = clear RX mappings, false = clear TX mappings
};

struct AddCanMappingCommand {
  bool isRx;         // true = RX mapping, false = TX mapping
  uint32_t canId;    // CAN ID (COB-ID)
  uint32_t paramId;  // Parameter ID
  uint8_t position;  // Bit position in CAN frame
  int8_t length;     // Bit length (negative = signed)
  float gain;        // Gain multiplier
  int8_t offset;     // Offset value
};

struct RemoveCanMappingCommand {
  uint32_t index;    // SDO index of mapping to remove
  uint8_t subIndex;  // SDO subindex
};

// Command message structure
struct CANCommand {
  CANCommandType type;
  uint32_t requestId;  // Unique ID for matching async responses (0 = no response expected)
  union {
    ScanCommand scan;
    ConnectCommand connect;
    SetNodeIdCommand setNodeId;
    SetDeviceNameCommand setDeviceName;
    SpotValuesCommand spotValues;
    DeleteDeviceCommand deleteDevice;
    RenameDeviceCommand renameDevice;
    SendCanMessageCommand sendCanMessage;
    StartCanIntervalCommand startCanInterval;
    StopCanIntervalCommand stopCanInterval;
    StartCanIoIntervalCommand startCanIoInterval;
    UpdateCanIoFlagsCommand updateCanIoFlags;
    // Task 34: Device commands
    StartDeviceCommand startDevice;
    SetValueCommand setValue;
    ClearCanMapCommand clearCanMap;
    AddCanMappingCommand addCanMapping;
    RemoveCanMappingCommand removeCanMapping;
  } data;
};
