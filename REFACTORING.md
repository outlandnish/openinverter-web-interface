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

27. **Moved TWAI Driver Initialization to can_task.cpp**

- Created `configureTwaiDriver()` static helper function in can_task.cpp for common TWAI setup
- Added `initCanBusScanning()` public function for accepting all CAN messages
- Added `initCanBusForDevice()` public function for filtering specific device SDO responses
- Updated `Init()` and `InitCAN()` in oi_can.cpp to call can_task initialization functions
- Removed TWAI driver setup code from oi_can.cpp (~80 lines)
- Architecture now cleanly separated:
  - can_task.cpp = Complete CAN I/O layer (driver init + message RX/TX)
  - oi_can.cpp = CAN business logic (mappings, parameters, device commands)
- Better separation of concerns: hardware layer vs business logic
- Eliminated code duplication between Init() and InitCAN()

28. **Extracted ListErrors() Helper Functions**

- Created `buildErrorDescriptionMap()` helper to extract error descriptions from cached JSON
- Created `determineTickDuration()` helper to get tick duration from uptime parameter unit
- Created `requestErrorAtIndex()` helper to request timestamp and error number for a specific index
- Created `createErrorJsonObject()` helper to create JSON object for an error entry
- Simplified `ListErrors()` from 99 lines to ~35 lines
- Each helper has a single, well-defined responsibility
- Improved code readability and maintainability
- Easier to test individual error handling components
- Better separation of concerns: data extraction, SDO requests, JSON formatting

29. **Refactored GetRawJson() into Smaller Functions**

- Created `initiateJsonDownload()` helper to trigger JSON download from device
- Created `handleJsonStreamingUpdate()` helper to handle streaming callback updates
- Created `handleJsonProgressUpdate()` helper to handle progress callback updates (with throttling)
- Created `handleJsonDownloadCompletion()` helper to send final streaming and progress notifications
- Created `waitForJsonDownload()` helper for main wait loop with timeout handling
- Simplified `GetRawJson()` from ~103 lines to ~25 lines with clear, linear flow
- Each helper has a single, focused responsibility
- Improved code readability - main function now clearly shows the download flow
- Better separation of concerns: initiation, streaming, progress, completion
- Easier to test individual download phases independently

30. **Extracted Device Command Helper**

- Created `sendDeviceCommand()` helper function in `oi_can.cpp`
- Consolidated 5 nearly identical device command functions
- `SaveToFlash()`, `LoadFromFlash()`, `LoadDefaults()`, `StartDevice()`, `StopDevice()` now call single helper
- Added `DEVICE_COMMAND_TIMEOUT_MS` constant for 200ms timeout
- Reduced ~65 lines of duplicate code to ~35 lines
- Each device command function is now a one-liner
- Consistent error handling and timeout behavior

31. **Moved Init/InitCAN Logic to DeviceConnection**

- Added `connectToDevice()` method to DeviceConnection class
- Added `initializeForScanning()` method to DeviceConnection class
- Updated `device_connection.cpp` to include `can_task.h` for TWAI init functions
- `OICan::Init()` and `OICan::InitCAN()` now delegate to DeviceConnection
- Removed `can_task.h` include from `oi_can.cpp` (no longer needed)
- Fixed circular dependency: can_task.cpp → OICan → DeviceConnection → can_task TWAI init
- DeviceConnection now owns connection initialization logic
- Better separation of concerns: state management in DeviceConnection, business logic in OICan

32. **Extracted CAN Mapping JSON Builder**

- Created `retrieveCanMappingsAsJson()` helper function in `oi_can.cpp`
- Eliminated duplicate lambda callbacks in `GetCanMapping()` and `SendCanMapping()`
- Both functions now use the shared helper to build JSON document
- Removed ~15 lines of duplicate JSON object building code
- Simplified `doc.add<JsonObject>()` pattern (removed unnecessary subdoc)
- Cleaner, more maintainable code

33. **Removed Unnecessary OICan Wrapper Functions**

- Removed `OICan::IsIdle()` - callers now use `DeviceConnection::instance().isIdle()`
- Removed `OICan::GetNodeId()` - callers now use `DeviceConnection::instance().getNodeId()`
- Removed `OICan::GetBaudRate()` - callers now use `DeviceConnection::instance().getBaudRate()`
- Removed `OICan::SetConnectionReadyCallback()` - callers use DeviceConnection directly
- Removed `OICan::SetJsonDownloadProgressCallback()` - callers use DeviceConnection directly
- Removed `OICan::SetJsonStreamCallback()` - callers use DeviceConnection directly
- Removed `OICan::GetJsonTotalSize()` - callers use DeviceConnection directly
- Removed callback type definitions from `oi_can.h` (already in `device_connection.h`)
- Updated all callers: main.cpp, can_task.cpp, http_handlers.cpp, websocket_handlers.cpp, spot_values_manager.cpp, device_discovery.cpp
- Reduced OICan API surface - DeviceConnection is now primary API for connection state
- Eliminated ~30 lines of trivial wrapper code

35. **Added SDO Write-and-Wait Helper to Protocol Layer**

- Added `SDOProtocol::writeAndWait()` with two overloads (simple bool, and with response frame)
- Combines: clearPendingResponses, setValue, waitForResponse, check for ABORT
- Zero-initializes response frame on timeout to distinguish from abort
- Updated `sendDeviceCommand()`, `SetValue()`, `RemoveCanMapping()` to use helper
- Reduced boilerplate code in oi_can.cpp

36. **Added SDO Request-and-Wait Helper to Protocol Layer**

- Added `SDOProtocol::requestAndWait()` for full response frame access
- Added `SDOProtocol::requestValue()` convenience helper that extracts 32-bit value
- Updated `requestErrorAtIndex()` to use `requestValue()` (reduced from 20 lines to 8)
- Consistent API with `writeAndWait()` helpers

37. **Extracted SendJson Parameter Iterator with Callback**

- Created `iterateParameterValues()` helper with `ParameterValueCallback` type
- Fixed bug: original code serialized empty `doc` instead of populated `root`
- `SendJson()` now uses callback to populate fresh JsonDocument
- Reusable iteration logic for other parameter value use cases

38. **Refactored StreamValues to Separate Parsing from Streaming**

- Created `parseParameterIds()` helper returning `std::vector<int>`
- Created `streamParameterValues()` with `StreamValueCallback` type
- `StreamValues()` now composes these helpers with simple lambda
- Parsing logic reusable for other comma-separated inputs

39. **Added Progress Callback to ClearCanMap**

- Added `ClearMapProgressCallback` typedef to `oi_can.h`
- Updated `ClearCanMap()` signature with optional callback parameter
- Callback invoked after each successful mapping removal
- Fully backward compatible (callback defaults to nullptr)

40. **Simplified retrieveCanMappings State Machine**

- Created `CanMappingData` struct to replace 9 callback parameters
- Extracted `parseGainFromResponse()` for 24-bit signed fixed-point conversion
- Extracted `requestMappingElement()` for SDO request/wait/check-abort pattern
- Extracted `retrieveMappingsForDirection()` to handle TX or RX direction
- Replaced complex 5-state state machine with simple nested while loops
- Reduced from ~100 lines to ~60 lines with clearer linear flow

41. **Moved ReloadJson File Operations to DeviceStorage**

- Added `DeviceConnection::getSerial()` to return formatted serial string
- Updated `ReloadJson()` to use `DeviceStorage::removeJsonCache()` instead of direct LittleFS
- Removed `#include <LittleFS.h>` from oi_can.cpp (no longer needed)
- All file operations now go through centralized storage manager

42. **Extracted Parameter Value Conversion Helper**

- Created `extractParameterValue()` helper function in oi_can.cpp
- Parameters are stored as signed fixed-point with 5-bit fractional precision (scale of 32)
- Updated 3 call sites: `iterateParameterValues()`, `streamParameterValues()`, `TryGetValueResponse()`
- Fixed bug in `TryGetValueResponse()` where `uint32_t` was used instead of `int32_t` (negative values were incorrect)
- Centralized conversion logic for consistency across all parameter value reads

43. **Moved CanMappingData Struct to Models**

- Moved `CanMappingData` struct from oi_can.cpp to `src/models/can_types.h`
- Struct represents CAN mapping data during retrieval operations
- Follows pattern of centralizing data models in the models folder

44. **Added Hardware CAN Filter for Scanning Mode**

- Updated `initCanBusScanning()` to use hardware filter instead of accept-all
- Dual filter mode accepts:
  - Filter 0: Bootloader response (0x7DE) - exact match
  - Filter 1: SDO response range (0x580-0x5FF) - masks lower 7 bits
- Reduces CPU load by filtering irrelevant CAN traffic at hardware level
- Only CANOpen SDO responses and bootloader messages reach the application

45. **Reset CAN Filter on Device Disconnect**

- Added `DeviceConnection::resetToScanningMode()` method
- Uses stored baud rate and pin config to reinitialize CAN with scanning filter
- Called from `handleDisconnect()` when client releases device lock
- Clears JSON cache and resets nodeId to 0
- Ensures CAN filter is ready to discover any device after disconnecting

### Phase 2: CAN I/O Architecture

34. **Migrated OICan Blocking CAN Operations to can_task Queue**

- All direct TWAI calls removed from oi_can.cpp, device_discovery.cpp, update_handler.cpp
- Queue-based I/O architecture implemented with layered design:
  - can_task (base) → SDO protocol layer → oi_can (business logic)
- FreeRTOS queues used for TX/RX with cooperative multitasking
- Created `canTxQueue` and `sdoResponseQueue` for message passing
- Updated SDO protocol layer to use queue-based TX/RX
- Only can_task.cpp contains direct TWAI driver calls (as intended)

---

## Remaining Refactorings 📋

### Priority 3: SDO Protocol Layer Enhancements (Medium Risk)

#### Task 35: Add SDO Write-and-Wait Helper to SDO Protocol Layer ✅

**Files:** `src/protocols/sdo_protocol.h`, `src/protocols/sdo_protocol.cpp`, `src/oi_can.cpp`
**Status:** Complete

**Implemented:**

- Added `writeAndWait()` helper with two overloads to `SDOProtocol` namespace
- Simple version returns bool (success/failure)
- Extended version populates response frame for error code inspection
- Zero-initializes response frame on timeout so callers can distinguish timeout from abort
- Updated `sendDeviceCommand()` to use `writeAndWait()` (1 line instead of 6)
- Updated `SetValue()` to use `writeAndWait()` with response inspection
- Updated `RemoveCanMapping()` to use `writeAndWait()`
- Note: `ClearCanMap()` not updated - has inverted logic where ABORT means success (done clearing)
- Note: `AddCanMapping()` not updated - has multi-step sequence with interdependent writes

---

#### Task 36: Add SDO Request-and-Wait Helper to SDO Protocol Layer ✅

**Files:** `src/protocols/sdo_protocol.h`, `src/protocols/sdo_protocol.cpp`, `src/oi_can.cpp`
**Status:** Complete

**Implemented:**

- Added `requestAndWait()` helper to `SDOProtocol` namespace
- Added `requestValue()` convenience helper that extracts 32-bit value directly
- Zero-initializes response frame on timeout for consistency with `writeAndWait()`
- Updated `requestErrorAtIndex()` to use `requestValue()` (reduced from 20 lines to 8 lines)
- Note: `retrieveCanMappings()` not updated - uses non-clearing pattern for efficiency in state machine

---

### Priority 4: OICan Callback Pattern Refactoring (Medium Risk)

#### Task 37: Extract SendJson Parameter Iterator with Callback ✅

**Files:** `src/oi_can.cpp`
**Status:** Complete

**Implemented:**

- Created `iterateParameterValues()` helper with `ParameterValueCallback` type
- Fixed bug: original code serialized empty `doc` instead of populated `root`
- `SendJson()` now uses callback to populate fresh JsonDocument
- Reusable iteration logic for other parameter value use cases

---

#### Task 38: Refactor StreamValues to Separate Parsing from Streaming ✅

**Files:** `src/oi_can.cpp`
**Status:** Complete

**Implemented:**

- Created `parseParameterIds()` helper returning `std::vector<int>`
- Created `streamParameterValues()` with `StreamValueCallback` type
- Callback provides: sampleIndex, itemIndex, numItems, value
- `StreamValues()` now composes these helpers with simple lambda
- Parsing logic reusable for other comma-separated inputs

---

#### Task 39: Add Progress Callback to ClearCanMap ✅

**Files:** `src/oi_can.h`, `src/oi_can.cpp`
**Status:** Complete

**Implemented:**

- Added `ClearMapProgressCallback` typedef to `oi_can.h`
- Updated `ClearCanMap()` signature with optional callback parameter
- Callback invoked after each successful mapping removal
- Fully backward compatible (callback defaults to nullptr)

---

### Priority 5: HTTP & Main.cpp Cleanup (Low-Medium Risk)

**All tasks in this section completed! ✅**

---

### Priority 6: OICan Module Refactoring (Medium-High Risk)

**All tasks in this section completed! ✅** (See Tasks 24, 25, and 26 in Completed Refactorings)

---

### Priority 7: State Management (Highest Risk, Highest Value)

#### Task 12: Encapsulate Global State into Manager Classes ✅

**Status:** Complete

**Completed:**

- `SpotValuesManager` - Created and integrated
- `CanIntervalManager` - Created and integrated
- `ClientLockManager` - Created and integrated
- `DeviceCache` - Created and integrated
- `DeviceConnection` - Created and integrated (connection state, JSON cache, callbacks)
- `DeviceDiscovery` - Created and integrated (scanning state, device list)
- `FirmwareUpdateHandler` - Created and integrated (update state machine, progress tracking)

**Final cleanup:**

- Moved firmware update progress tracking (`lastReportedPage`, `wasInProgress`) from `event_processor.cpp` into `FirmwareUpdateHandler`
- Added `checkProgressUpdate()` and `checkCompletion()` methods to `FirmwareUpdateHandler`
- Remaining globals in `main.cpp` are infrastructure (queues, server, websocket, config) - appropriate as globals

---

### Priority 8: CAN I/O Architecture (Highest Risk, Highest Value)

#### Task 34: Migrate OICan Blocking CAN Operations to can_task Queue ✅

**Files:** `src/oi_can.cpp`, `src/can_task.cpp`, `src/models/can_command.h`, `src/models/can_event.h`
**Status:** Complete

**Problem:**
Many OICan functions directly call `twai_receive()` and `twai_transmit()`, blocking the calling thread (usually the main/websocket task). This creates potential threading issues since `can_task` is also accessing TWAI in the background.

Functions that directly access TWAI:

- `SendJson()` - reads parameter values synchronously
- `retrieveCanMappings()` - state machine with multiple receives
- `AddCanMapping()` / `RemoveCanMapping()` / `ClearCanMap()` - modify CAN mappings
- `SetValue()` - sets parameter values
- `sendDeviceCommand()` - save, load, start, stop commands
- `ListErrors()` - reads error log
- `SendCanMessage()` - sends arbitrary CAN message
- `StreamValues()` - streams parameter values
- `TryGetValueResponse()` - receives SDO responses

**Solution:**

1. **Add new command types to `can_command.h`**:

```cpp
CMD_SET_VALUE,
CMD_GET_VALUE,
CMD_ADD_CAN_MAPPING,
CMD_REMOVE_CAN_MAPPING,
CMD_CLEAR_CAN_MAP,
CMD_SAVE_TO_FLASH,
CMD_LOAD_FROM_FLASH,
CMD_LOAD_DEFAULTS,
CMD_START_DEVICE,
CMD_STOP_DEVICE,
CMD_LIST_ERRORS,
CMD_GET_CAN_MAPPINGS,
// etc.
```

2. **Add corresponding event types to `can_event.h`**:

```cpp
EVT_VALUE_SET,
EVT_VALUE_RECEIVED,
EVT_CAN_MAPPING_ADDED,
EVT_CAN_MAPPING_REMOVED,
EVT_CAN_MAP_CLEARED,
EVT_FLASH_SAVED,
EVT_FLASH_LOADED,
EVT_DEFAULTS_LOADED,
EVT_DEVICE_STARTED,
EVT_DEVICE_STOPPED,
EVT_ERRORS_LISTED,
EVT_CAN_MAPPINGS_RECEIVED,
// etc.
```

3. **Create command handlers in can_task.cpp** for each operation

4. **Convert OICan functions to async pattern**:
   - Send command to queue
   - Either return immediately (fire-and-forget) or wait for response event
   - WebSocket handlers receive events and send responses to client

**Benefits:**

- Thread safety: All CAN I/O on single task
- No blocking in websocket handlers
- Cleaner separation of concerns
- Easier to add timeouts and error handling
- Can add request prioritization

**Challenges:**

- Large refactoring effort
- Need to handle async responses in websocket handlers
- Some operations currently return values synchronously
- May need to add request IDs for matching responses

**Phased Approach:**

1. Start with simple fire-and-forget commands (SaveToFlash, etc.)
2. Move to request/response patterns for value get/set
3. Handle complex state machines (CAN mappings, error list)
4. Update all websocket handlers to use async pattern

**Testing Strategy:**

- Test each migrated operation individually
- Verify no race conditions
- Check timeout handling
- Integration test full workflows
- Load test with concurrent operations

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
5. ✅ ~~Task 12: Encapsulate global state~~ - **PARTIALLY COMPLETE** (managers created)

**Total: ~12-17 hours + extensive testing**
**Completed: All tasks complete! ✅**

### Week 6: OICan Cleanup (Low-Medium Risk)

1. ✅ ~~Task 30: Extract sendDeviceCommand() helper (30 min)~~ - **COMPLETED**
2. ✅ ~~Task 31: Move Init/InitCAN to DeviceConnection (1 hour)~~ - **COMPLETED**
3. ✅ ~~Task 32: Extract CAN mapping JSON builder (30 min)~~ - **COMPLETED**
4. ✅ ~~Task 33: Remove unnecessary OICan wrappers (1 hour)~~ - **COMPLETED**

**Total: ~3 hours**
**Completed: All tasks complete! ✅**

### Future: CAN I/O Architecture (Very High Risk) ✅ COMPLETED

1. Task 34: Migrate OICan blocking operations to can_task queue ✅

**Status: COMPLETED**

- All direct TWAI calls removed from oi_can.cpp, device_discovery.cpp, update_handler.cpp
- Queue-based I/O architecture implemented with layered design:
  - can_task (base) → SDO protocol layer → oi_can (business logic)
- FreeRTOS queues used for TX/RX with cooperative multitasking

---

### Task 34 Subtask Breakdown

**Architecture: Layered CAN Stack with Queue-Based I/O**

```
┌─────────────────────────────────────┐
│  oi_can (business logic)            │  ← Device commands, mappings, errors
├─────────────────────────────────────┤
│  SDO protocol layer                 │  ← SDO request/response, segmented transfers
├─────────────────────────────────────┤
│  can_task (CAN I/O)                 │  ← Raw TX/RX queues, TWAI driver
└─────────────────────────────────────┘
```

The goal is to eliminate direct `twai_transmit()`/`twai_receive()` calls from oi_can.cpp by introducing queue-based I/O at the base layer. Business logic stays in oi_can, SDO protocol operations use queues.

#### Phase 1: Infrastructure (34.1-34.3) ✅ COMPLETED

**34.1: Add New Command/Event Types** ✅

- Added command enum values for device operations
- Added event enum values and `SetValueResult` enum
- Added command/event payload structs

**34.2: Add Request ID Tracking** ✅

- Added `requestId` field to `CANCommand` and `CANEvent` structs
- Created `src/utils/request_id.h` with thread-safe ID generator

#### Phase 2: CAN Queue Infrastructure (34.4-34.6) ✅ COMPLETED

**34.4: Add CAN TX/RX Queues to can_task** ✅

- Created `canTxQueue` - queue of `twai_message_t` for transmission
- Created `sdoResponseQueue` - queue for SDO responses routed to callers
- Exported queues in `can_task.h`
- Updated `canTask()` main loop with `processTxQueue()` function

**34.5: Create CAN Queue Helper Functions** ✅

- Created `src/utils/can_queue.h` with helper functions:
  - `canQueueTransmit()` - send frame via TX queue
  - `canQueueReceive()` - receive from SDO response queue
  - `canQueueClearResponses()` - clear pending responses

**34.6: Update can_task Message Routing** ✅

- Updated `receiveAndProcessCanMessages()` to route SDO responses to `sdoResponseQueue`
- Firmware update responses still go to `FirmwareUpdateHandler`
- Device discovery responses still update last seen times

#### Phase 3: SDO Protocol Layer Refactor (34.7-34.9) ✅ COMPLETED

**34.7: Refactor SDO Protocol TX Functions** ✅

- Updated `SDOProtocol::requestElement()` to use `canQueueTransmit()`
- Updated `SDOProtocol::setValue()` to use `canQueueTransmit()`
- Updated `SDOProtocol::requestNextSegment()` to use `canQueueTransmit()`
- Added `requestElementNonBlocking()` for async operations

**34.8: Add SDO Response Helper** ✅

- Created `SDOProtocol::waitForResponse(twai_message_t* response, TickType_t timeout)`
- Created `SDOProtocol::clearPendingResponses()`
- Uses FreeRTOS queue with cooperative multitasking (yields to other tasks)

**34.9: Add Async SDO Operations (Optional)** ✅

- Created `requestElementNonBlocking()` for streaming operations

#### Phase 4: Update oi_can to Use Queue-Based SDO (34.10-34.13) ✅ COMPLETED

**34.10: Update Simple Device Commands** ✅

- Refactored `sendDeviceCommand()` to use `SDOProtocol::waitForResponse()`
- Updated: `SaveToFlash()`, `LoadFromFlash()`, `LoadDefaults()`, `StartDevice()`, `StopDevice()`

**34.11: Update SetValue/GetValue** ✅

- Refactored `SetValue()` to use queue-based SDO
- Refactored `GetValue()` to use queue-based SDO
- Updated `TryGetValueResponse()` to use `SDOProtocol::waitForResponse()`

**34.12: Update CAN Mapping Operations** ✅

- Refactored `retrieveCanMappings()` state machine to use queues
- Refactored `AddCanMapping()` multi-step sequence to use queues
- Refactored `RemoveCanMapping()` to use queues
- Refactored `ClearCanMap()` to use queues

**34.13: Update Remaining Operations** ✅

- Refactored `ListErrors()` to use queue-based SDO
- Refactored `SendCanMessage()` to use TX queue
- Verified `SendJson()` / `StreamValues()` work with queues
- Updated `device_discovery.cpp` to use queue-based operations
- Updated `update_handler.cpp` to use queue-based TX

#### Phase 5: Cleanup and Testing (34.14-34.15)

**34.14: Remove Direct TWAI Calls from oi_can** ✅

- Verified no remaining `twai_transmit()` or `twai_receive()` in oi_can.cpp
- Verified no remaining direct TWAI calls in update_handler.cpp or device_discovery.cpp
- All CAN I/O now goes through can_task queues
- Only can_task.cpp contains direct TWAI driver calls (as intended)

**34.15: Integration Testing** Partial

- [x] Test all WebSocket operations end-to-end
- [x] Verify spot values streaming works
- [x] Test device scanning
- [ ] Test firmware updates
- [x] Check for race conditions with concurrent operations
- [x] Verify timeout handling

#### Implementation Notes

**Key Insight:**
Business logic stays in oi_can.cpp - we're not moving code to can_task. We're just replacing direct TWAI driver calls with queue operations. This is a much smaller change.

**Queue Sizing:**

- `canTxQueue`: 10-20 frames (burst capacity)
- `sdoResponseQueue`: 5-10 frames (typically 1 outstanding request)

**Thread Safety:**

- TX queue: Multiple producers (oi_can, interval manager), single consumer (can_task)
- RX queue: Single producer (can_task), single consumer (calling thread in oi_can)

**Timeout Handling:**

- Queue operations have timeouts just like direct TWAI calls
- Existing timeout values can be preserved

**Order of Implementation:**

1. Phase 2 (34.4-34.6) - Add queues to can_task
2. Phase 3 (34.7-34.9) - Refactor SDO protocol
3. Phase 4 (34.10-34.13) - Update oi_can (incremental, one function at a time)
4. Phase 5 (34.14-34.15) - Cleanup and testing

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
