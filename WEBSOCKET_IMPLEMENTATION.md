# WebSocket Implementation Summary

## Overview
Switched from HTTP polling to WebSocket-based real-time communication for CAN device scanning and management.

## Key Changes

### Firmware (ESP32)

1. **Dependencies** (`platformio.ini`)
   - Added `ottowinter/ESPAsyncWebServer-esphome@^3.0.0` for async WebSocket support

2. **OICan Module** (`src/oi_can.h`, `src/oi_can.cpp`)
   - Added continuous CAN scanning functionality:
     - `StartContinuousScan(start, end)` - Begin continuous scanning
     - `StopContinuousScan()` - Stop scanning
     - `ProcessContinuousScan()` - Called in main loop to cycle through CAN IDs
     - `SetDeviceDiscoveryCallback()` - Register callback for device discoveries
   - Scanning cycles through node IDs, probing each one periodically (50ms delay)
   - Automatically updates `devices.json` with discovered devices and timestamps
   - Looks up existing device names from the device table

3. **Main Application** (`src/main.cpp`)
   - Replaced synchronous `WebServer` with `AsyncWebServer`
   - Added WebSocket endpoint at `/ws`
   - Implemented WebSocket event handlers:
     - **Connection**: Sends current scan status and saved devices
     - **Messages**:
       - `startScan` - Start continuous scanning (with configurable range)
       - `stopScan` - Stop continuous scanning
       - `connect` - Connect to a specific device by node ID
       - `setDeviceName` - Save device name
   - Device discovery callback broadcasts to all WebSocket clients
   - Simplified HTTP endpoints (kept only essential ones for backward compatibility)

### Web App

1. **WebSocket Hook** (`web/src/hooks/useWebSocket.ts`)
   - Reusable hook for WebSocket connections
   - Features:
     - Automatic reconnection on disconnect
     - Type-safe message handling
     - Connection status tracking
     - Send/receive message helpers

2. **DeviceList Page** (`web/src/pages/DeviceList.tsx`)
   - Real-time device discovery via WebSocket
   - No more manual scanning - devices appear automatically
   - Events handled:
     - `deviceDiscovered` - New device found (updates state in real-time)
     - `scanStatus` - Scan started/stopped
     - `savedDevices` - Initial device list on connection
     - `connected` - Device connection confirmed
   - Commands sent via WebSocket:
     - Device name saving
     - Device connection
     - Start/stop continuous scan

3. **DeviceScanner Component** (`web/src/components/DeviceScanner.tsx`)
   - Updated UI for continuous scanning
   - Shows "Stop Continuous Scan" button when scanning is active
   - Clearer messaging about continuous scanning behavior

## How It Works

### Continuous Scanning Flow
1. User clicks "Start Continuous Scan" button
2. Web app sends `startScan` WebSocket message with node ID range
3. Firmware starts cycling through CAN node IDs (1-32 or 1-127)
4. For each node:
   - Probes for serial number (4 parts)
   - If device responds, firmware:
     - Formats serial number
     - Looks up device name in `devices.json`
     - Updates `lastSeen` timestamp and `nodeId`
     - Broadcasts `deviceDiscovered` event to all WebSocket clients
5. Web app receives events and updates UI in real-time
6. Scanning continues indefinitely until stopped

### Device Discovery Format
```json
{
  "event": "deviceDiscovered",
  "data": {
    "nodeId": 5,
    "serial": "12345678:12345678:12345678:12345678",
    "lastSeen": 1234567890,
    "name": "Motor Controller" // If previously named
  }
}
```

## Benefits

1. **Real-time Updates**: Devices appear immediately as they're discovered
2. **Continuous Monitoring**: Scanning runs in background, always up-to-date
3. **Efficient**: No polling overhead, WebSocket maintains single connection
4. **Scalable**: Multiple web clients can connect and receive same updates
5. **Better UX**: Users see devices appear live instead of waiting for scan completion

## Testing

To test the implementation:
1. Build and upload firmware
2. Build and deploy web app
3. Open web interface
4. Click "Start Continuous Scan"
5. Connect CAN devices - they should appear automatically
6. Check that "last seen" updates when devices are re-discovered
7. Verify that device names are preserved and displayed correctly
