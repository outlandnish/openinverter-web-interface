# Multi-Threading Implementation Plan

## Overview
Separate WebSocket/HTTP server from CAN processing using FreeRTOS tasks to prevent blocking.

## Current State
✅ Queue structures defined (CANCommand, CANEvent)
✅ CAN task created (`canTask()`)
❌ Queues not initialized
❌ CAN task not spawned
❌ WebSocket handlers still call OICan directly
❌ Event processor not implemented

---

## Step 1: Initialize Queue Infrastructure
**Goal:** Create queues and spawn CAN task

### Changes in `setup()`:
```cpp
// After CAN initialization, before WebSocket setup:

// Create FreeRTOS queues
canCommandQueue = xQueueCreate(10, sizeof(CANCommand));
canEventQueue = xQueueCreate(20, sizeof(CANEvent));

if (canCommandQueue == nullptr || canEventQueue == nullptr) {
  DBG_OUTPUT_PORT.println("ERROR: Failed to create queues!");
  return;
}

DBG_OUTPUT_PORT.println("Queues created successfully");

// Update device discovery callback to post events
OICan::SetDeviceDiscoveryCallback([](uint8_t nodeId, const char* serial, uint32_t lastSeen) {
  CANEvent evt;
  evt.type = EVT_DEVICE_DISCOVERED;
  evt.data.deviceDiscovered.nodeId = nodeId;
  strncpy(evt.data.deviceDiscovered.serial, serial, sizeof(evt.data.deviceDiscovered.serial) - 1);
  evt.data.deviceDiscovered.lastSeen = lastSeen;
  evt.data.deviceDiscovered.name[0] = '\0'; // Will be filled from devices.json

  // Look up name from devices.json
  if (LittleFS.exists("/devices.json")) {
    File file = LittleFS.open("/devices.json", "r");
    if (file) {
      JsonDocument doc;
      deserializeJson(doc, file);
      file.close();
      if (doc.containsKey("devices") && doc["devices"].containsKey(serial)) {
        const char* name = doc["devices"][serial]["name"];
        if (name) {
          strncpy(evt.data.deviceDiscovered.name, name, sizeof(evt.data.deviceDiscovered.name) - 1);
        }
      }
    }
  }

  xQueueSend(canEventQueue, &evt, 0);
});

// Spawn CAN task on Core 0 (Core 1 runs WiFi/WebSocket)
xTaskCreatePinnedToCore(
  canTask,           // Task function
  "CAN_Task",        // Name
  8192,              // Stack size (bytes)
  nullptr,           // Parameters
  1,                 // Priority (1 = low, higher than idle)
  nullptr,           // Task handle
  0                  // Core 0
);

DBG_OUTPUT_PORT.println("CAN task spawned on Core 0");
```

### Testing Step 1:
1. Build and upload
2. Check serial monitor for:
   - "Queues created successfully"
   - "[CAN Task] Started"
   - "CAN task spawned on Core 0"
3. CAN processing should still work (scanning in background)

**STOP HERE AND TEST BEFORE PROCEEDING**

---

## Step 2: Implement Event Processor
**Goal:** Process events from CAN task and broadcast to WebSocket

### Add before `loop()`:
```cpp
// Process events from CAN task and broadcast to WebSocket
void processCANEvents() {
  CANEvent evt;

  // Process all pending events (non-blocking)
  while (xQueueReceive(canEventQueue, &evt, 0) == pdTRUE) {
    JsonDocument doc;
    doc["event"] = "";
    JsonObject data = doc["data"].to<JsonObject>();

    switch(evt.type) {
      case EVT_DEVICE_DISCOVERED:
        doc["event"] = "deviceDiscovered";
        data["nodeId"] = evt.data.deviceDiscovered.nodeId;
        data["serial"] = evt.data.deviceDiscovered.serial;
        data["lastSeen"] = evt.data.deviceDiscovered.lastSeen;
        if (evt.data.deviceDiscovered.name[0] != '\0') {
          data["name"] = evt.data.deviceDiscovered.name;
        }
        break;

      case EVT_SCAN_STATUS:
        doc["event"] = "scanStatus";
        data["active"] = evt.data.scanStatus.active;
        break;

      case EVT_CONNECTED:
        doc["event"] = "connected";
        data["nodeId"] = evt.data.connected.nodeId;
        data["serial"] = evt.data.connected.serial;
        break;

      case EVT_NODE_ID_INFO:
        doc["event"] = "nodeIdInfo";
        data["id"] = evt.data.nodeIdInfo.id;
        data["speed"] = evt.data.nodeIdInfo.speed;
        break;

      case EVT_NODE_ID_SET:
        doc["event"] = "nodeIdSet";
        data["id"] = evt.data.nodeIdSet.id;
        data["speed"] = evt.data.nodeIdSet.speed;
        break;

      case EVT_SPOT_VALUES_STATUS:
        doc["event"] = "spotValuesStatus";
        data["active"] = evt.data.spotValuesStatus.active;
        if (evt.data.spotValuesStatus.active) {
          data["interval"] = evt.data.spotValuesStatus.interval;
          data["paramCount"] = evt.data.spotValuesStatus.paramCount;
        }
        break;

      case EVT_DEVICE_NAME_SET:
        doc["event"] = "deviceNameSet";
        data["success"] = evt.data.deviceNameSet.success;
        data["serial"] = evt.data.deviceNameSet.serial;
        data["name"] = evt.data.deviceNameSet.name;
        break;

      default:
        continue; // Unknown event, skip
    }

    String output;
    serializeJson(doc, output);
    ws.textAll(output);
  }
}
```

### Update `loop()`:
```cpp
void loop(void){
  ws.cleanupClients();
  ArduinoOTA.handle();

  // Process events from CAN task
  processCANEvents();

  // NOTE: OICan::Loop() now runs in CAN task, don't call here
}
```

### Testing Step 2:
1. Build and upload
2. Start a scan from web UI
3. Verify devices are discovered and shown in UI
4. Check that WebSocket messages are being sent

**STOP HERE AND TEST BEFORE PROCEEDING**

---

## Step 3: Update WebSocket Handlers to Use Queue
**Goal:** Make WebSocket handlers non-blocking by posting commands to queue

### Replace in `onWebSocketEvent()`:

#### startScan:
```cpp
if (action == "startScan") {
  uint8_t start = doc["start"] | 1;
  uint8_t end = doc["end"] | 32;

  CANCommand cmd;
  cmd.type = CMD_START_SCAN;
  cmd.data.scan.start = start;
  cmd.data.scan.end = end;

  if (xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE) {
    DBG_OUTPUT_PORT.println("[WebSocket] Scan command queued");
  } else {
    DBG_OUTPUT_PORT.println("[WebSocket] ERROR: Failed to queue scan command");
  }
}
```

#### stopScan:
```cpp
else if (action == "stopScan") {
  CANCommand cmd;
  cmd.type = CMD_STOP_SCAN;
  xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100));
}
```

#### connect:
```cpp
else if (action == "connect") {
  uint8_t nodeId = doc["nodeId"];
  String serial = doc["serial"] | "";

  CANCommand cmd;
  cmd.type = CMD_CONNECT;
  cmd.data.connect.nodeId = nodeId;
  strncpy(cmd.data.connect.serial, serial.c_str(), sizeof(cmd.data.connect.serial) - 1);

  xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100));
}
```

#### setDeviceName:
```cpp
else if (action == "setDeviceName") {
  String serial = doc["serial"];
  String name = doc["name"];
  int nodeId = doc["nodeId"] | -1;

  CANCommand cmd;
  cmd.type = CMD_SET_DEVICE_NAME;
  strncpy(cmd.data.setDeviceName.serial, serial.c_str(), sizeof(cmd.data.setDeviceName.serial) - 1);
  strncpy(cmd.data.setDeviceName.name, name.c_str(), sizeof(cmd.data.setDeviceName.name) - 1);
  cmd.data.setDeviceName.nodeId = nodeId;

  xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100));
}
```

#### getNodeId:
```cpp
else if (action == "getNodeId") {
  CANCommand cmd;
  cmd.type = CMD_GET_NODE_ID;
  xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100));
}
```

#### setNodeId:
```cpp
else if (action == "setNodeId") {
  int id = doc["id"];

  CANCommand cmd;
  cmd.type = CMD_SET_NODE_ID;
  cmd.data.setNodeId.nodeId = id;

  xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100));
}
```

#### startSpotValues:
```cpp
else if (action == "startSpotValues") {
  CANCommand cmd;
  cmd.type = CMD_START_SPOT_VALUES;

  if (doc.containsKey("paramIds")) {
    JsonArray paramIds = doc["paramIds"].as<JsonArray>();
    cmd.data.spotValues.paramCount = 0;
    for (JsonVariant id : paramIds) {
      if (cmd.data.spotValues.paramCount < 100) {
        cmd.data.spotValues.paramIds[cmd.data.spotValues.paramCount++] = id.as<int>();
      }
    }
  }

  if (doc.containsKey("interval")) {
    cmd.data.spotValues.interval = doc["interval"].as<uint32_t>();
    if (cmd.data.spotValues.interval < 100) cmd.data.spotValues.interval = 100;
    if (cmd.data.spotValues.interval > 10000) cmd.data.spotValues.interval = 10000;
  } else {
    cmd.data.spotValues.interval = 1000;
  }

  xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100));
}
```

#### stopSpotValues:
```cpp
else if (action == "stopSpotValues") {
  CANCommand cmd;
  cmd.type = CMD_STOP_SPOT_VALUES;
  xQueueSend(canCommandQueue, &cmd, pdMS_TO_TICKS(100));
}
```

### Testing Step 3:
1. Build and upload
2. Test ALL WebSocket actions:
   - Start/stop scan
   - Connect to device
   - Set device name
   - Get/set node ID
   - Start/stop spot values monitoring
3. Verify UI is responsive (no blocking)
4. Check serial monitor for "[WebSocket] command queued" messages

**STOP HERE AND TEST BEFORE PROCEEDING**

---

## Step 4: Final Cleanup
**Goal:** Remove any remaining blocking code

### Changes:
1. ✅ `OICan::Loop()` already removed from `loop()` (now in CAN task)
2. Remove old `broadcastDeviceDiscovery()` if not using callback
3. Verify no direct OICan calls remain in WebSocket handlers

### Final Testing:
1. Full system test with multiple devices
2. Verify WebSocket stays responsive during heavy CAN traffic
3. Monitor for any crashes or deadlocks
4. Check memory usage is acceptable

---

## Benefits After Completion

✅ **Non-blocking WebSocket** - UI always responsive
✅ **Continuous CAN processing** - No interruptions from web traffic
✅ **Better performance** - Each task on separate core
✅ **More reliable** - No race conditions between web and CAN
✅ **Easier to debug** - Clear separation of concerns

---

## Rollback Plan

If issues occur:
1. `git revert HEAD` to return to pre-threading state
2. Or disable threading by commenting out task creation
3. Restore `OICan::Loop()` call in main `loop()`

---

## Notes

- Queue sizes: Command queue (10), Event queue (20) - adjust if needed
- Task priority: CAN=1 (low), WebSocket=default (higher)
- Stack size: 8KB for CAN task - monitor for overflow
- Core affinity: CAN on Core 0, WebSocket on Core 1
