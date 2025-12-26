#ifndef CAN_COMMAND_H
#define CAN_COMMAND_H

#include "can_types.h"
#include <cstdint>

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
  char intervalId[32]; // Unique ID for this interval message
};

struct StopCanIntervalCommand {
  char intervalId[32]; // ID of interval to stop
};

struct StartCanIoIntervalCommand {
  uint32_t canId;           // CAN ID (default 0x3F)
  uint16_t pot;             // Throttle 1 (12 bits, 0-4095)
  uint16_t pot2;            // Throttle 2 (12 bits, 0-4095)
  uint8_t canio;            // Digital I/O flags (6 bits)
  uint16_t cruisespeed;     // Cruise speed (14 bits, 0-16383)
  uint8_t regenpreset;      // Regen preset (8 bits, 0-255)
  uint32_t intervalMs;      // Send interval in milliseconds
  bool useCrc;              // Use CRC-32 (true) or counter-only (false)
};

struct UpdateCanIoFlagsCommand {
  uint16_t pot;             // Throttle 1 (12 bits, 0-4095)
  uint16_t pot2;            // Throttle 2 (12 bits, 0-4095)
  uint8_t canio;            // Digital I/O flags (6 bits)
  uint16_t cruisespeed;     // Cruise speed (14 bits, 0-16383)
  uint8_t regenpreset;      // Regen preset (8 bits, 0-255)
};

// Command message structure
struct CANCommand {
  CANCommandType type;
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
  } data;
};

#endif // CAN_COMMAND_H
