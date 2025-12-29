#pragma once

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

struct JsonProgressEvent {
  uint32_t clientId;
  int bytesReceived;
  int totalBytes;
  bool complete;
};

struct JsonReadyEvent {
  uint32_t clientId;
  uint8_t nodeId;
  bool success;
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

// Task 34: Device command event structures

struct DeviceCommandEvent {
  bool success;
};

struct ValueSetEvent {
  SetValueResult result;
  int paramId;
  double value;
};

struct CanMapClearedEvent {
  bool success;
  bool isRx;
};

struct CanMappingsReceivedEvent {
  bool success;
  char mappingsJson[2048];  // JSON string of mappings
};

struct CanMappingAddedEvent {
  SetValueResult result;
};

struct CanMappingRemovedEvent {
  SetValueResult result;
};

struct ErrorsListedEvent {
  bool success;
  char errorsJson[1024];    // JSON string of errors
};

// Event message structure
struct CANEvent {
  CANEventType type;
  uint32_t requestId;  // Matches requestId from command (0 = unsolicited event)
  union {
    DeviceDiscoveredEvent deviceDiscovered;
    ScanStatusEvent scanStatus;
    ScanProgressEvent scanProgress;
    JsonProgressEvent jsonProgress;
    JsonReadyEvent jsonReady;
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
    // Task 34: Device command events
    DeviceCommandEvent deviceCommand;       // For save/load/start/stop/reset
    ValueSetEvent valueSet;
    CanMapClearedEvent canMapCleared;
    CanMappingsReceivedEvent canMappingsReceived;
    CanMappingAddedEvent canMappingAdded;
    CanMappingRemovedEvent canMappingRemoved;
    ErrorsListedEvent errorsListed;
  } data;
};
