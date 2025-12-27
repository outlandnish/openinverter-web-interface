# Code Refactoring Plan

## Completed Refactorings ✅

### Phase 1: Code Organization & Safety

1. **Fixed Unsafe String Operations**
   - Replaced all `strcpy()` calls with `safeCopyString()` using `snprintf`
   - Created template helper in `src/utils/string_utils.h`
   - Total: 15+ occurrences fixed

2. **Extracted Models & Types**
   - `src/models/can_types.h` - Enums and configuration constants
   - `src/models/can_command.h` - Command structures (with named sub-structs)
   - `src/models/can_event.h` - Event structures (with named sub-structs)
   - `src/models/interval_messages.h` - Interval message structures
   - All union members now have explicit type names

3. **Replaced Magic Numbers**
   - Defined constants for all magic numbers
   - `MAX_PARAM_IDS`, `SPOT_VALUES_INTERVAL_MIN/MAX_MS`
   - `CAN_INTERVAL_MIN/MAX_MS`, `CAN_IO_INTERVAL_MIN/MAX_MS`
   - `CAN_IO_*_MASK` bit masks for message packing
   - `QUEUE_SEND_TIMEOUT_MS` for consistent queue operations

4. **Created Utils Infrastructure**
   - `src/utils/string_utils.h` - Safe string operations
   - Support for `std::string`, `const char*`, and Arduino `String`

5. **Extracted Duplicate Error Response Helpers**
   - Created `src/utils/websocket_helpers.h` with helper functions
   - `sendWebSocketError()` - General error response helper
   - `sendDeviceBusyError()` - Specialized "Device is busy" error helper
   - Replaced 9 duplicate "Device is busy" error patterns in `src/main.cpp`
   - Eliminated ~90 lines of duplicate code

6. **Extracted Queue Command Helper**
   - Created `queueCanCommand()` helper function
   - Replaced 16 duplicate queue send patterns with single helper
   - Improved error handling consistency
   - Eliminated ~80 lines of duplicate code
   - Organized all main.cpp helper functions into `src/main.h` as inline functions
   - Includes: `queueCanCommand()`, `setStatusLED()`, `statusLEDOff()`

7. **Defined CAN Protocol Constants**
   - Added CAN ID constants to `src/models/can_types.h`
   - `SDO_REQUEST_BASE_ID (0x600)`, `SDO_RESPONSE_BASE_ID (0x580)`
   - `SDO_RESPONSE_MAX_ID (0x5FF)`, `BOOTLOADER_COMMAND_ID (0x7DD)`
   - `BOOTLOADER_RESPONSE_ID (0x7DE)`
   - Replaced all magic number occurrences in `src/oi_can.cpp`
   - Total: 15+ occurrences replaced with named constants

8. **Extracted CAN Response Validation Helpers**
   - Created `isValidSerialResponse()` helper function in `src/oi_can.cpp`
   - Created `advanceScanNode()` helper function in `src/oi_can.cpp`
   - Replaced 2 complex nested validation patterns (lines 1854, 2116)
   - Replaced 3 duplicate node advancement blocks (lines 2138, 2142, 2146)
   - Eliminated ~25 lines of duplicate code
   - Improved code readability and maintainability

9. **Improved Variable Names in oi_can.cpp**
   - Renamed `scanSerialPart` → `currentSerialPartIndex` (9 occurrences)
   - Renamed `paramJson` → `cachedParamJson` (14 occurrences)
   - Renamed `jsonBuffer` → `jsonReceiveBuffer` (61 occurrences)
   - Renamed `currentPage` → `currentFlashPage` (7 occurrences)
   - Total: 91 variable references updated with more descriptive names
   - Improved code clarity and self-documentation

10. **Broke Up canTask() Function**
   - Extracted 16 command handler functions (one per command type)
   - Extracted 3 periodic task functions (spot values, interval messages, CAN IO)
   - Created `dispatchCommand()` function with clean switch statement
   - Simplified `canTask()` main loop from 341 lines to 21 lines
   - Moved all CAN handling code to dedicated `src/can_task.cpp` and `src/can_task.h` files
   - Reduced `main.cpp` by ~400 lines, improved code organization
   - Better separation of concerns and maintainability

11. **Replaced WebSocket If-Else Chain with Dispatch Table**
   - Created `WebSocketHandler` function pointer type definition
   - Built dispatch table (`wsHandlers`) mapping action strings to handler functions
   - Replaced 65-line if-else chain with 7-line dispatch table lookup
   - Added forward declarations for all 31 WebSocket handler functions
   - Improved maintainability and follows Open/Closed Principle
   - Hash map lookup provides better performance than sequential string comparisons
   - Easy to add new handlers (just add to the dispatch table)
   - Extracted all WebSocket handlers to dedicated `src/websocket_handlers.cpp` and `src/websocket_handlers.h` files
   - Moved ~900 lines of WebSocket handler code from main.cpp to websocket_handlers.cpp
   - Reduced main.cpp by ~900 lines, improved code organization
   - Better separation of concerns similar to CAN task extraction

12. **Replaced SmartLeds with Adafruit NeoPixel Library**
   - Updated platformio.ini to use Adafruit NeoPixel library instead of SmartLeds
   - Resolved namespace collision between SmartLeds and ArduinoJson
   - Updated main.h and main.cpp to use Adafruit_NeoPixel API
   - Changed LED color definitions from `Rgb` to `uint32_t` using `Color()` function
   - Added `statusLED.begin()` call in setup()
   - Simplified websocket_handlers.cpp to include main.h directly (no more workarounds)
   - Eliminated code duplication by using shared helper functions from main.h

---

## Remaining Refactorings 📋

### Priority 3: Extract Validation Helpers (Medium Risk)

---

### Priority 4: Function Decomposition (High Risk, High Value)

#### Task 8: Refactor ProcessContinuousScan()
**File:** `src/oi_can.cpp`
**Lines:** 2068-2148 (81 lines)
**Effort:** 2 hours
**Risk:** Medium

**Problem:**
Complex state machine with:
- Multiple exit conditions
- Timing logic
- Request/response handling
- 3 identical node advancement blocks (code duplication)

**Solution:**

1. **Extract helpers** (see Task 4 - already defined `advanceScanNode()`)

2. **Extract validation**:
```cpp
bool shouldProcessScan(unsigned long currentTime) {
  return continuousScanActive &&
         state == IDLE &&
         (currentTime - lastScanTime >= SCAN_DELAY_MS);
}
```

3. **Extract response handling**:
```cpp
bool handleScanResponse(const twai_message_t& frame) {
  if (!isValidSerialResponse(frame, currentScanNode, scanSerialPart)) {
    return false;
  }

  scanDeviceSerial[scanSerialPart] = *(uint32_t*)&frame.data[4];
  scanSerialPart++;

  if (scanSerialPart == 4) {
    // Serial complete
    char serial[50];
    sprintf(serial, "%08lX-%08lX-%08lX-%08lX", ...);
    AddOrUpdateDevice(serial, currentScanNode, nullptr, millis());
    advanceScanNode();
  }

  return true;
}
```

4. **Simplified main function**:
```cpp
void ProcessContinuousScan() {
  unsigned long currentTime = millis();

  if (!shouldProcessScan(currentTime)) {
    return;
  }

  lastScanTime = currentTime;
  requestSdoElement(_nodeId, SDO_INDEX_SERIAL, scanSerialPart);

  twai_message_t rxframe;
  if (twai_receive(&rxframe, pdMS_TO_TICKS(SCAN_TIMEOUT_MS)) == ESP_OK) {
    if (!handleScanResponse(rxframe)) {
      advanceScanNode();
    }
  } else {
    advanceScanNode();
  }
}
```

---

#### Task 9: Refactor ScanDevices()
**File:** `src/oi_can.cpp`
**Lines:** 1790-1896 (106 lines)
**Effort:** 2-3 hours
**Risk:** Medium

**Problem:**
Doing too many things:
- File I/O (read/write devices.json)
- JSON parsing and building
- Device serial request/response handling
- State saving/restoration
- Nested loops (3 levels deep!)

**Solution:**

1. **Extract device serial request**:
```cpp
bool requestDeviceSerial(uint8_t nodeId, uint32_t serialParts[4]) {
  for (uint8_t part = 0; part < 4; part++) {
    requestSdoElement(_nodeId, SDO_INDEX_SERIAL, part);

    twai_message_t rxframe;
    if (twai_receive(&rxframe, pdMS_TO_TICKS(100)) != ESP_OK) {
      return false;
    }

    if (!isValidSerialResponse(rxframe, nodeId, part)) {
      return false;
    }

    serialParts[part] = *(uint32_t*)&rxframe.data[4];
  }
  return true;
}
```

2. **Extract JSON file operations**:
```cpp
bool loadDevicesJson(JsonDocument& doc) {
  File file = LittleFS.open("/devices.json", "r");
  if (!file) return false;

  DeserializationError error = deserializeJson(doc, file);
  file.close();
  return error == DeserializationError::Ok;
}

bool saveDevicesJson(const JsonDocument& doc) {
  File file = LittleFS.open("/devices.json", "w");
  if (!file) return false;

  serializeJson(doc, file);
  file.close();
  return true;
}
```

3. **Extract device update logic**:
```cpp
void updateDeviceInJson(JsonDocument& doc, const char* serial, uint8_t nodeId) {
  if (!doc["devices"].containsKey(serial)) {
    doc["devices"][serial]["name"] = "";
  }
  doc["devices"][serial]["nodeId"] = nodeId;
  doc["devices"][serial]["lastSeen"] = millis();
}
```

4. **Simplified main function**:
```cpp
String ScanDevices(uint8_t startNodeId, uint8_t endNodeId) {
  JsonDocument doc;
  loadDevicesJson(doc);

  // Save and temporarily change node ID
  uint8_t originalNodeId = _nodeId;

  for (uint8_t nodeId = startNodeId; nodeId <= endNodeId; nodeId++) {
    _nodeId = nodeId;

    uint32_t serialParts[4];
    if (requestDeviceSerial(nodeId, serialParts)) {
      char serial[50];
      sprintf(serial, "%08lX-%08lX-%08lX-%08lX",
              serialParts[0], serialParts[1], serialParts[2], serialParts[3]);

      updateDeviceInJson(doc, serial, nodeId);
      AddOrUpdateDevice(serial, nodeId);
    }
  }

  // Restore original node ID
  _nodeId = originalNodeId;
  ReloadJson();

  saveDevicesJson(doc);

  String result;
  serializeJson(doc["devices"], result);
  return result;
}
```

---

### Priority 5: State Management (Highest Risk, Highest Value)

#### Task 10: Encapsulate Global State into Manager Classes
**Files:** `src/main.cpp`, potentially new files
**Effort:** 4-6 hours
**Risk:** Very High (fundamental architecture change)

**Problem:**
Global state scattered throughout main.cpp:
```cpp
// Spot values state (5 variables)
std::vector<int> spotValuesParamIds;
uint32_t spotValuesInterval = 1000;
uint32_t lastSpotValuesTime = 0;
std::map<int, double> spotValuesBatch;
std::map<int, double> latestSpotValues;

// Interval messages state (1 vector)
std::vector<IntervalCanMessage> intervalCanMessages;

// CAN IO interval state (1 struct)
CanIoInterval canIoInterval = {...};

// Device locking state (2 maps)
std::map<uint8_t, uint32_t> deviceLocks;
std::map<uint32_t, uint8_t> clientDevices;
```

**Solution:**

Create manager classes to encapsulate related state:

1. **Create `src/managers/spot_values_manager.h`**:
```cpp
class SpotValuesManager {
private:
  std::vector<int> paramIds;
  std::map<int, double> batch;
  std::map<int, double> cache;
  uint32_t interval;
  uint32_t lastProcessTime;

public:
  void start(const std::vector<int>& params, uint32_t intervalMs);
  void stop();
  void processBatch();
  void addValue(int paramId, double value);
  bool isActive() const;
  uint32_t getInterval() const;
  const std::map<int, double>& getCache() const;
};
```

2. **Create `src/managers/interval_messages_manager.h`**:
```cpp
class IntervalMessagesManager {
private:
  std::vector<IntervalCanMessage> messages;
  CanIoInterval ioInterval;

public:
  void startInterval(const String& id, uint32_t canId, const uint8_t* data,
                     uint8_t length, uint32_t intervalMs);
  void stopInterval(const String& id);
  void startCanIoInterval(const CanIoInterval& config);
  void stopCanIoInterval();
  void sendPendingMessages();
  void sendCanIoMessage();
};
```

3. **Create `src/managers/device_lock_manager.h`**:
```cpp
class DeviceLockManager {
private:
  std::map<uint8_t, uint32_t> locks;        // nodeId -> clientId
  std::map<uint32_t, uint8_t> clientToNode; // clientId -> nodeId

public:
  bool acquire(uint8_t nodeId, uint32_t clientId);
  void release(uint8_t nodeId);
  void releaseClient(uint32_t clientId);
  bool isLocked(uint8_t nodeId) const;
  uint32_t getLockedBy(uint8_t nodeId) const;
  std::optional<uint8_t> getClientDevice(uint32_t clientId) const;
};
```

4. **Update main.cpp**:
```cpp
// Replace global variables with:
SpotValuesManager spotValuesManager;
IntervalMessagesManager intervalMessagesManager;
DeviceLockManager deviceLockManager;

// Update all references to use manager methods
```

**Benefits:**
- Encapsulated state
- Clear ownership
- Easier to test
- Better code organization
- Prevents accidental state corruption

**Testing Strategy:**
- Unit test each manager class independently
- Integration test with WebSocket handlers
- Verify all spot values, intervals, and locking still work
- Check for memory leaks

---

## Implementation Order Recommendation

### Week 1: Low-Hanging Fruit
1. ✅ ~~Task 1: Extract error response helpers (30 min)~~ - **COMPLETED**
2. ✅ ~~Task 2: Extract queue command helper (30 min)~~ - **COMPLETED**
3. ✅ ~~Task 3: Define CAN protocol constants (45 min)~~ - **COMPLETED**
4. ✅ ~~Task 5: Improve variable names (30 min)~~ - **COMPLETED**

**Total: ~2.5 hours, Low risk, Good cleanup**
**Completed: 2.5 hours | All tasks complete! ✅**

### Week 2: Validation & Helper Extraction
1. ✅ ~~Task 4: Extract CAN response validation (1 hour)~~ - **COMPLETED**
2. ✅ ~~Task 6: Break up canTask() (3-4 hours + testing)~~ - **COMPLETED**
3. Task 8: Refactor ProcessContinuousScan() (2 hours)
4. Task 9: Refactor ScanDevices() (2-3 hours)

**Total: ~9-10 hours, Medium-High risk, Improves main.cpp and oi_can.cpp**
**Completed: 4-5 hours | Remaining: ~4-5 hours**

### Week 3: Major Restructuring (High Risk)
1. ✅ ~~Task 7: WebSocket dispatch table (2-3 hours + testing)~~ - **COMPLETED**

**Total: ~2-3 hours + thorough testing**
**Completed: 2-3 hours | All tasks complete! ✅**

### Week 4: Architecture Improvement (Very High Risk)
1. Task 10: Encapsulate global state (4-6 hours + extensive testing)

**Total: ~4-6 hours + extensive testing**

---

## Testing Checklist

After each task, verify:
- [ ] Code compiles without errors
- [ ] All WebSocket commands still work
- [ ] Spot values streaming works
- [ ] CAN interval messages send correctly
- [ ] Device scanning works
- [ ] Device locking works correctly
- [ ] No memory leaks (check with heap monitoring)
- [ ] Performance is not degraded

---

## Notes

- Each task can be done independently in most cases
- Tasks 1-5 can be done in any order
- Task 6 should be done before Task 10
- Task 7 can be done independently
- Tasks 8-9 can be done in any order
- Task 10 is the most invasive - save for last
- Always commit after completing each task
- Test thoroughly before moving to next task

---

## Rollback Strategy

If any refactoring causes issues:
1. Git revert to previous working commit
2. Re-analyze the problem
3. Try a more incremental approach
4. Consider breaking the task into smaller sub-tasks
