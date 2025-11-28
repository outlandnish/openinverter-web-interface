# ESP32 Device Simulator

A development tool that simulates the ESP32 web server and CAN bus devices, allowing you to develop and test the web interface without physical hardware.

## Features

- **Full REST API**: Simulates all ESP32 endpoints (`/cmd`, `/canmap`, `/devices`, `/scan`, etc.)
- **WebSocket Support**: Real-time communication with device discovery and spot values streaming
- **Mock CAN Devices**: Pre-configured simulated devices on the CAN bus
- **Dynamic Parameters**: Realistic parameter values that change over time
- **Easy Integration**: Runs alongside the web app with a single command

## Quick Start

### 1. Install Dependencies

From the `web/` directory:

```bash
npm install
```

### 2. Run with Simulator

```bash
npm run dev:sim
```

This will:
- Start the simulator on `http://localhost:4000`
- Start the web app on `http://localhost:3000` (proxied to the simulator)
- Open your browser to test the web interface

### 3. Access the Web Interface

Open [http://localhost:3000](http://localhost:3000) in your browser.

## Usage Modes

### With Simulator (No Hardware)

```bash
npm run dev:sim
```

The web app will connect to the simulator instead of real hardware.

### With Real Hardware

```bash
npm run dev
```

The web app will proxy to `http://inverter.local` (your ESP32 device).

### Run Simulator Only

```bash
npm run simulator
```

Useful if you want to run the simulator separately for testing.

## Simulated Data

### Mock Devices

The simulator includes three pre-configured CAN devices:

| Node ID | Serial | Name | Type |
|---------|--------|------|------|
| 1 | OI-INV-001234 | Main Inverter | Inverter |
| 2 | OI-CHG-005678 | Charger | Charger |
| 3 | OI-BMS-009012 | (unnamed) | BMS |

### Mock Parameters

Includes realistic inverter parameters:

- **System**: Operating mode, version, DC voltage limits
- **Measurements**: Voltage, current, power, speed, temperatures
- **Motor**: Boost, field weakening settings
- **Live Values**: Parameters update dynamically to simulate real-world changes

### Mock CAN Mappings

Pre-configured CAN mappings for testing the CAN mapping interface.

## API Endpoints

The simulator implements all ESP32 endpoints:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/version` | GET | Returns firmware version |
| `/params/json` | GET | Returns parameter definitions |
| `/cmd` | GET | Execute commands (json, set, get, save) |
| `/canmap` | GET | Get/add/remove CAN mappings |
| `/devices` | GET | Get saved devices |
| `/scan` | GET | Scan for CAN devices |
| `/reloadjson` | GET | Reload parameter JSON |
| `/resetdevice` | GET | Reset device |
| `/list` | GET | List files |
| `/wifi` | GET | Get WiFi settings |
| `/settings` | GET/POST | Get/update device settings |
| `/ws` | WebSocket | Real-time communication |

## WebSocket Events

### Client → Server (Actions)

```json
{"action": "startScan", "start": 1, "end": 32}
{"action": "stopScan"}
{"action": "connect", "nodeId": 1, "serial": "OI-INV-001234"}
{"action": "setDeviceName", "serial": "...", "name": "...", "nodeId": 1}
{"action": "getNodeId"}
{"action": "setNodeId", "id": 1}
{"action": "startSpotValues", "paramIds": [10, 11, 12], "interval": 1000}
{"action": "stopSpotValues"}
```

### Server → Client (Events)

```json
{"event": "scanStatus", "data": {"active": true}}
{"event": "deviceDiscovered", "data": {"nodeId": 1, "serial": "...", "lastSeen": 123456}}
{"event": "connected", "data": {"nodeId": 1, "serial": "..."}}
{"event": "deviceNameSet", "data": {"success": true, "serial": "...", "name": "..."}}
{"event": "spotValues", "data": {"timestamp": 123456, "values": {"10": 350.5}}}
```

## Customization

### Adding More Devices

Edit `simulator/mock-data.ts`:

```typescript
export const mockDevices: MockDevice[] = [
  { nodeId: 1, serial: 'OI-INV-001234', name: 'Main Inverter', lastSeen: Date.now() },
  { nodeId: 2, serial: 'OI-CHG-005678', name: 'Charger', lastSeen: Date.now() },
  // Add your devices here
]
```

### Adding/Modifying Parameters

Edit `mockParameters` in `simulator/mock-data.ts`:

```typescript
export const mockParameters: MockParameterList = {
  myParam: {
    value: 100,
    name: 'My Parameter',
    unit: 'V',
    minimum: 0,
    maximum: 500,
    default: 100,
    isparam: true,
    category: 'Custom',
    id: 100,
    i: 50,
  },
  // ...existing parameters
}
```

### Customizing Value Updates

The `updateMockParameters()` function in `simulator/mock-data.ts` controls how parameter values change over time. Modify it to create different simulation behaviors.

## Architecture

```
┌─────────────────┐      ┌─────────────────┐      ┌─────────────────┐
│   Browser       │      │   Vite Dev      │      │   Simulator     │
│   (port 3000)   │◄────►│   (port 3000)   │◄────►│   (port 4000)   │
│                 │      │   Proxy Server  │      │                 │
└─────────────────┘      └─────────────────┘      └─────────────────┘
                                                    • REST API
                                                    • WebSocket
                                                    • Mock Data
```

When `dev:sim` runs:
1. Simulator starts on port 4000
2. Vite dev server starts on port 3000
3. Vite proxies all `/cmd`, `/ws`, etc. requests to the simulator
4. Browser talks to Vite, which forwards to the simulator

## Troubleshooting

### Port Already in Use

If port 4000 is already in use, edit `simulator/index.ts` and change:

```typescript
const PORT = 4000  // Change to another port
```

Also update `vite.config.js` to match.

### WebSocket Connection Failed

Make sure both the simulator and Vite dev server are running. The `dev:sim` command should start both automatically.

### Changes Not Reflected

The simulator uses `tsx watch`, so changes to simulator code will auto-reload. If you modify mock data, the simulator will restart automatically.

## Development Tips

1. **Check Simulator Logs**: The simulator logs all API calls and WebSocket messages to the console
2. **Use Browser DevTools**: Monitor network traffic and WebSocket messages
3. **Test Edge Cases**: Modify mock data to test error conditions and edge cases
4. **Add Logging**: Add `console.log()` in the simulator to debug specific behaviors

## Files

- `simulator/index.ts` - Main server implementation
- `simulator/mock-data.ts` - Mock parameters, devices, and CAN mappings
- `simulator/README.md` - This file

## License

Same as the main project (GPL-3.0)
