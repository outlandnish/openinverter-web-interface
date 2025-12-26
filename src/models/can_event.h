#ifndef CAN_EVENT_H
#define CAN_EVENT_H

#include "can_types.h"
#include <cstdint>

// Event payload structures
struct DeviceDiscoveredEvent {
  uint8_t nodeId;
  char serial[50];
  uint32_t lastSeen;
  char name[50];
};

struct ScanStatusEvent {
  bool active;
};

struct ScanProgressEvent {
  uint8_t currentNode;
  uint8_t startNode;
  uint8_t endNode;
};

struct ConnectedEvent {
  uint8_t nodeId;
  char serial[50];
};

struct NodeIdInfoEvent {
  uint8_t id;
  uint8_t speed;
};

struct NodeIdSetEvent {
  uint8_t id;
  uint8_t speed;
};

struct SpotValuesStatusEvent {
  bool active;
  uint32_t interval;
  int paramCount;
};

struct SpotValuesEvent {
  uint32_t timestamp;
  char valuesJson[1024]; // JSON string of values
};

struct DeviceNameSetEvent {
  bool success;
  char serial[50];
  char name[50];
};

struct ErrorEvent {
  char message[200];
};

struct DeviceDeletedEvent {
  bool success;
  char serial[50];
};

struct DeviceRenamedEvent {
  bool success;
  char serial[50];
  char name[50];
};

struct CanMessageSentEvent {
  bool success;
  uint32_t canId;
};

struct CanIntervalStatusEvent {
  bool active;
  char intervalId[32];
  uint32_t intervalMs;
};

struct CanIoIntervalStatusEvent {
  bool active;
  uint32_t intervalMs;
};

// Event message structure
struct CANEvent {
  CANEventType type;
  union {
    DeviceDiscoveredEvent deviceDiscovered;
    ScanStatusEvent scanStatus;
    ScanProgressEvent scanProgress;
    ConnectedEvent connected;
    NodeIdInfoEvent nodeIdInfo;
    NodeIdSetEvent nodeIdSet;
    SpotValuesStatusEvent spotValuesStatus;
    SpotValuesEvent spotValues;
    DeviceNameSetEvent deviceNameSet;
    ErrorEvent error;
    DeviceDeletedEvent deviceDeleted;
    DeviceRenamedEvent deviceRenamed;
    CanMessageSentEvent canMessageSent;
    CanIntervalStatusEvent canIntervalStatus;
    CanIoIntervalStatusEvent canIoIntervalStatus;
  } data;
};

#endif // CAN_EVENT_H
