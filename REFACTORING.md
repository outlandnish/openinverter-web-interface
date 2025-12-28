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

13. **Refactored ProcessContinuousScan() Function**
   - Added `SCAN_TIMEOUT_MS` constant (100ms) for scan response timeout
   - Created `shouldProcessScan()` helper function to validate scan conditions
   - Created `handleScanResponse()` helper function to process CAN responses
   - Simplified `ProcessContinuousScan()` from 65 lines to 29 lines
   - Improved code readability by extracting complex validation and response handling logic
   - Eliminated duplicate node advancement patterns
   - Better separation of concerns: validation, response handling, and control flow

14. **Refactored ScanDevices() Function**
   - Created `requestDeviceSerial()` helper function to request all 4 serial number parts
   - Created `loadDevicesJson()` helper function to load and parse devices.json from LittleFS
   - Created `saveDevicesJson()` helper function to save devices JSON to LittleFS
   - Created `updateDeviceInJson()` helper function to update device entries
   - Simplified `ScanDevices()` from 103 lines to 69 lines (~34 lines removed)
   - Eliminated nested loops (reduced from 3 levels deep to 1 level)
   - Better separation of concerns: File I/O, JSON parsing, and serial requests isolated
   - Improved code readability and maintainability

15. **Removed Unused REST API Endpoints**
   - Removed `/params/json` endpoint from `src/main.cpp` (replaced by WebSocket `getParamSchema` and `getParamValues`)
   - Removed `/reloadjson` endpoint from `src/main.cpp` (replaced by WebSocket `reloadParams`)
   - Removed `reloadJson()` method from `web/src/api/inverter.ts`
   - Deleted entire `web/simulator/` directory (no longer needed)
   - Removed simulator scripts from `web/package.json` (`simulator` and `dev:sim`)
   - Simplified `web/vite.config.js` to only target real hardware (removed VITE_USE_SIMULATOR logic)
   - Eliminated ~50 lines of unused REST endpoint code
   - Removed ~500 lines of simulator code
   - Cleaner codebase with reduced maintenance burden

16. **Extracted HTTP Route Handlers**
   - Created `src/http_handlers.h` with forward declarations for all HTTP handlers
   - Created `src/http_handlers.cpp` with implementations of all route handlers
   - Extracted `/version`, `/devices`, `/settings`, `/ota/upload` endpoints
   - Extracted `handleFileRequest()` file serving handler
   - Extracted helper functions: `formatBytes()`, `getContentType()`
   - Updated `src/main.h` to expose `ws`, `config`, and `totalUpdatePages` as extern
   - Replaced ~145 lines of route registration code with single `registerHttpRoutes(server)` call
   - Follows same pattern as WebSocket handlers extraction
   - Better separation of HTTP concerns from main application logic
   - Makes main.cpp setup() more focused on initialization

17. **Extracted Status LED Functions**
   - Created `src/status_led.h` with StatusLED class encapsulating LED functionality
   - Created `src/status_led.cpp` with implementation and color constants
   - Moved LED color constants to StatusLED class as static members (StatusLED::OFF, StatusLED::COMMAND, etc.)
   - Converted to singleton pattern with static `instance()` method
   - Made constructor private for better encapsulation
   - Updated all code to use StatusLED::instance() instead of global variable
   - Removed LED-related #defines from main.cpp (now in status_led.h)
   - Updated main.h to include status_led.h and use StatusLED methods
   - Better encapsulation of LED functionality with no global variables
   - Thread-safe lazy initialization using C++11 static local variables

18. **Extracted CAN IO Utilities**
   - Created `src/utils/can_utils.h` and `src/utils/can_utils.cpp` for generic CRC-32 function
   - Created `src/utils/can_io_utils.h` and `src/utils/can_io_utils.cpp` for CAN IO message building
   - Moved `crc32_word()` function to can_utils (used by both CAN IO and firmware updates)
   - Moved `buildCanIoMessage()` function to can_io_utils
   - Updated `src/main.cpp`, `src/can_task.cpp`, and `src/oi_can.cpp` to use new utilities
   - Eliminated code duplication (CRC function existed in both main.cpp and oi_can.cpp)
   - Removed ~64 lines of duplicate code from main.cpp and oi_can.cpp
   - Better organization of CAN-specific utilities
   - Easier to unit test CRC and message building

19. **Extracted WiFi Setup**
   - Created `src/wifi_setup.h` with WiFiSetup class
   - Created `src/wifi_setup.cpp` with implementation
   - Extracted `loadCredentials()` - loads WiFi credentials from LittleFS
   - Extracted `connectStation()` - connects to WiFi in station mode with retry logic
   - Extracted `startAccessPoint()` - starts WiFi in AP mode with MAC-based SSID
   - Extracted `initialize()` - high-level method that tries STA mode, falls back to AP
   - Updated `src/main.cpp` to use WiFiSetup::initialize()
   - Reduced setup() function by ~110 lines
   - Uses StatusLED::instance() directly for better encapsulation
   - Better separation of concerns for WiFi logic
   - Easier to extend with features like WiFi Manager

20. **Cleaned Up Unused Code**
   - Removed commented out code: `HardwareSerial Inverter`, `DynamicJsonDocument jsonDoc`
   - Removed unused globals: `fastUart`, `fastUartAvailable`, `uartMessBuff`, `jsonFileName` (in main.cpp)
   - Removed unused UART functions: `uart_readUntill()`, `uart_readStartsWith()` (~25 lines)
   - Total: ~35 lines of dead code removed from main.cpp
   - Cleaner codebase with reduced confusion
   - Easier to understand what's actually used

21. **Extracted File I/O Manager**
   - Created `src/managers/device_storage.h` with DeviceStorage class
   - Created `src/managers/device_storage.cpp` with static methods
   - Centralized all device list file operations (devices.json)
   - Methods: `loadDevices()`, `saveDevices()`, `updateDeviceInJson()`
   - Added JSON cache utilities: `hasJsonCache()`, `removeJsonCache()`, `getJsonFileName()`
   - Removed 3 static helper functions from oi_can.cpp (~40 lines)
   - Updated `src/oi_can.cpp`: ScanDevices, SaveDeviceName, DeleteDevice, LoadDevices
   - Updated `src/main.cpp`: 2 device name lookup locations
   - Eliminated 17+ duplicate LittleFS operations
   - Centralized file I/O in one place for easier testing and maintenance

22. **Extracted Firmware Update Handler**
   - Created `src/firmware/update_handler.h` with FirmwareUpdateHandler class
   - Created `src/firmware/update_handler.cpp` with implementation
   - Encapsulated firmware update state machine and logic
   - Extracted `handleUpdate()` function (~115 lines) into class methods
   - Removed static variables: `updstate`, `updateFile`, `currentFlashPage`
   - Removed `UpdState` enum from oi_can.cpp
   - Updated `StartUpdate()`, `GetCurrentUpdatePage()`, `IsUpdateInProgress()` to use handler
   - Updated `Loop()` to call `FirmwareUpdateHandler::instance().processResponse()`
   - Implemented singleton pattern for thread-safe access
   - Better separation of concerns between CAN communication and firmware update logic
   - Easier to test firmware update state machine independently

23. **Extracted Device Discovery Manager**
   - Created `src/managers/device_discovery.h` with DeviceDiscovery class
   - Created `src/managers/device_discovery.cpp` with implementation
   - Encapsulated device scanning state machine and in-memory device list
   - Extracted scanning functions: `ScanDevices()`, `StartContinuousScan()`, `StopContinuousScan()`, `ProcessContinuousScan()`
   - Extracted device management: `LoadDevices()`, `AddOrUpdateDevice()`, `GetSavedDevices()`, `SaveDeviceName()`, `DeleteDevice()`
   - Extracted heartbeat functions: `UpdateDeviceLastSeen()`, `UpdateDeviceLastSeenByNodeId()`
   - Removed helper functions: `isValidSerialResponse()`, `advanceScanNode()`, `shouldProcessScan()`, `handleScanResponse()`, `requestDeviceSerial()`
   - Removed static variables: scanning state, device list, discovery/progress callbacks
   - Removed legacy `ProcessHeartbeat()` function (now using passive heartbeat tracking)
   - Moved `BaudRate` enum to `models/can_types.h` to avoid circular dependencies
   - Updated `oi_can.h` to use `BaudRate` from `can_types.h` via using declarations
   - Implemented singleton pattern for thread-safe access
   - Better separation of concerns between CAN communication and device discovery
   - Clean dependency structure: `can_types.h` → `oi_can.h` ← `device_discovery.h`

24. **Extracted SDO Protocol Layer**
   - Created `src/protocols/sdo_protocol.h` with SDOProtocol namespace
   - Created `src/protocols/sdo_protocol.cpp` with implementation
   - Defined all SDO constants in SDOProtocol namespace (32 constants total):
     - Request/Response constants: `REQUEST_DOWNLOAD`, `REQUEST_UPLOAD`, `REQUEST_SEGMENT`, `TOGGLE_BIT`, etc.
     - SDO Indexes: `INDEX_PARAMS`, `INDEX_PARAM_UID`, `INDEX_SERIAL`, `INDEX_STRINGS`, `INDEX_COMMANDS`, etc.
     - SDO Commands: `CMD_SAVE`, `CMD_LOAD`, `CMD_RESET`, `CMD_DEFAULTS`, `CMD_START`, `CMD_STOP`
     - Error codes: `ERR_INVIDX`, `ERR_RANGE`, `ERR_GENERAL`
   - Extracted SDO request functions: `requestElement()`, `requestElementNonBlocking()`, `setValue()`, `requestNextSegment()`
   - Updated all SDO function implementations to create their own `twai_message_t` frames (removed dependency on global `tx_frame`)
   - Removed 32 SDO constant #defines from `src/oi_can.cpp`
   - Removed 4 static SDO functions from `src/oi_can.cpp` (~70 lines)
   - Updated all SDO constant references throughout `oi_can.cpp` to use `SDOProtocol::` namespace
   - Updated all SDO function calls throughout `oi_can.cpp` to use `SDOProtocol::` namespace
   - Encapsulated CANopen SDO protocol details in dedicated layer
   - Better separation of concerns between protocol and business logic
   - Reusable protocol layer for other CAN-based projects
   - Easier to test protocol layer independently

25. **Consolidated CAN State Variables**
   - Created `src/managers/device_connection.h` with DeviceConnection class
   - Created `src/managers/device_connection.cpp` with implementation
   - Consolidated connection state variables into DeviceConnection singleton:
     - Connection state: `nodeId_`, `baudRate_`, `state_`, `canTxPin_`, `canRxPin_`
     - Serial number: `serial_[4]`, `jsonFileName_[20]`
     - Retry management: `retries_`, state timeout tracking
     - JSON cache: `cachedParamJson_`, `jsonReceiveBuffer_`, `jsonTotalSize_`
     - Callbacks: `connectionReadyCallback_`, `jsonProgressCallback_`, `jsonStreamCallback_`
     - Rate limiting: `lastParamRequestTime_`, `minParamRequestIntervalUs_`
   - Removed 20+ static variables from `src/oi_can.cpp`
   - Implemented singleton pattern with C++11 static local variables for thread-safe initialization
   - Added state management methods: `setState()`, `getState()`, `isIdle()`, `resetStateStartTime()`, `hasStateTimedOut()`
   - Added serial number management: `setSerialPart()`, `getSerialPart()`, `generateJsonFileName()`
   - Added retry methods: `setRetries()`, `incrementRetries()`, `decrementRetries()`, `resetRetries()`
   - Added rate limiting: `canSendParameterRequest()`, `markParameterRequestSent()`, `setParameterRequestRateLimit()`
   - Updated all variable references throughout `oi_can.cpp` to use DeviceConnection methods
   - Simplified `GetRawJson(nodeId)` to remove complex node-switching logic
   - Better state encapsulation and clearer ownership of connection-related data
   - Easier to reason about state changes and prevent state corruption
   - Foundation for future multi-device support with better state isolation

26. **Extracted CAN Debug and Validation Utilities**
   - Moved debug helper functions to `src/utils/can_utils.h` and `src/utils/can_utils.cpp`
   - Added `printCanTx()` function for transmitted CAN frame debugging (currently disabled)
   - Added `printCanRx()` function for received CAN frame debugging (currently disabled)
   - Added `isValidSdoResponse()` validation helper for SDO response checking
   - Removed static `printCanRx()` function from `src/oi_can.cpp`
   - Updated `src/firmware/update_handler.cpp` to use `printCanTx()` from can_utils
   - All debug helpers can be easily enabled for debugging by uncommenting code
   - Better organization of CAN-specific utilities alongside existing `crc32_word()` function
   - Reusable validation utilities
   - Easier to unit test debug and validation helpers

---

## Remaining Refactorings 📋

### Priority 3: Extract Validation Helpers (Medium Risk)

---

### Priority 4: Function Decomposition (High Risk, High Value)

---

### Priority 4: HTTP & Main.cpp Cleanup (Low-Medium Risk)

**All tasks in this section completed! ✅**

---

### Priority 5: OICan Module Refactoring (Medium-High Risk)

**All tasks in this section completed! ✅** (See Tasks 24, 25, and 26 in Completed Refactorings)

---

### Priority 6: State Management (Highest Risk, Highest Value)

#### Task 12: Encapsulate Global State into Manager Classes
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
3. ✅ ~~Task 8: Refactor ProcessContinuousScan() (2 hours)~~ - **COMPLETED**
4. ✅ ~~Task 9: Refactor ScanDevices() (2-3 hours)~~ - **COMPLETED**

**Total: ~9-10 hours, Medium-High risk, Improves main.cpp and oi_can.cpp**
**Completed: 9-10 hours | All tasks complete! ✅**

### Week 3: Major Restructuring (High Risk)
1. ✅ ~~Task 7: WebSocket dispatch table (2-3 hours + testing)~~ - **COMPLETED**
2. ✅ ~~Task 6: Remove Unused REST API Endpoints (30 min)~~ - **COMPLETED**

**Total: ~2.5-3.5 hours + thorough testing**
**Completed: 2.5-3.5 hours | All tasks complete! ✅**

### Week 4: HTTP & Main.cpp Cleanup (Low-Medium Risk)
1. ✅ ~~Task 7: Extract HTTP Route Handlers (2-3 hours)~~ - **COMPLETED**
2. ✅ ~~Task 8: Extract Status LED Functions (1 hour)~~ - **COMPLETED**
3. ✅ ~~Task 9: Extract CAN IO Utilities (1 hour)~~ - **COMPLETED**
4. ✅ ~~Task 10: Extract WiFi Setup (1.5-2 hours)~~ - **COMPLETED**
5. ✅ ~~Task 11: Clean Up Unused Code (30 min)~~ - **COMPLETED**

**Total: ~6.5-8.5 hours**
**Completed: 6.5-8.5 hours | All tasks complete! ✅**

### Week 5: File I/O & Architecture (Low-High Risk)
1. ✅ ~~Task 14: Extract File I/O Manager (1.5-2 hours)~~ - **COMPLETED**
2. ✅ ~~Task 15: Extract Firmware Update Handler (2-3 hours)~~ - **COMPLETED**
3. ✅ ~~Task 16: Extract Device Discovery Manager (2-3 hours)~~ - **COMPLETED**
4. ✅ ~~Task 17: Consolidate CAN State Variables (2-3 hours)~~ - **COMPLETED**
5. Task 12: Encapsulate global state (4-6 hours + extensive testing)

**Total: ~12-17 hours + extensive testing**
**Completed: 9.5-13 hours**

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
