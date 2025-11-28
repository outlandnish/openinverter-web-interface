/**
 * ESP32 Device Simulator
 * Simulates the ESP32 web server for local development without hardware
 */

import express from 'express'
import { WebSocketServer, WebSocket } from 'ws'
import cors from 'cors'
import {
  mockParameters,
  mockDevices,
  mockCanMappings,
  updateMockParameters,
  type MockDevice,
  type MockCanMapping,
} from './mock-data'

const PORT = 4000
const app = express()

app.use(cors())
app.use(express.json())
app.use(express.urlencoded({ extended: true }))

// State management
let connectedNodeId = 1
let scanningActive = false
let scanInterval: NodeJS.Timeout | null = null
let spotValuesActive = false
let spotValuesInterval: NodeJS.Timeout | null = null
let spotValuesParamIds: number[] = []
const savedDevices: Record<string, MockDevice> = {}

// Initialize saved devices
mockDevices.forEach((device) => {
  savedDevices[device.serial] = { ...device }
})

// WebSocket server
const wss = new WebSocketServer({ noServer: true })

// Broadcast to all WebSocket clients
function broadcast(event: string, data: any) {
  const message = JSON.stringify({ event, data })
  wss.clients.forEach((client) => {
    if (client.readyState === WebSocket.OPEN) {
      client.send(message)
    }
  })
}

// Simulate device discovery during scanning
function simulateDeviceDiscovery() {
  mockDevices.forEach((device, index) => {
    setTimeout(() => {
      const updatedDevice = {
        ...device,
        lastSeen: Date.now(),
        name: savedDevices[device.serial]?.name,
      }
      broadcast('deviceDiscovered', updatedDevice)
    }, index * 1000) // Stagger discoveries
  })
}

// Broadcast spot values
function broadcastSpotValues() {
  if (!spotValuesActive || spotValuesParamIds.length === 0) return

  updateMockParameters() // Update values with simulated changes

  const values: Record<string, number> = {}
  spotValuesParamIds.forEach((paramId) => {
    // Find parameter by ID
    const param = Object.values(mockParameters).find((p) => p.id === paramId)
    if (param && typeof param.value === 'number') {
      values[paramId.toString()] = param.value
    }
  })

  broadcast('spotValues', {
    timestamp: Date.now(),
    values,
  })
}

// REST Endpoints

app.get('/version', (req, res) => {
  res.send('1.1.R-SIM')
})

app.get('/params/json', (req, res) => {
  res.json(mockParameters)
})

app.get('/cmd', (req, res) => {
  const cmd = req.query.cmd as string

  if (!cmd) {
    return res.status(400).send('Missing cmd parameter')
  }

  console.log(`[CMD] ${cmd}`)

  // Handle 'json' command - return current parameter values
  if (cmd === 'json') {
    updateMockParameters()
    return res.json(mockParameters)
  }

  // Handle 'save' command
  if (cmd === 'save') {
    return res.send('OK')
  }

  // Handle 'set' command - set <id> <value>
  if (cmd.startsWith('set ')) {
    const parts = cmd.split(' ')
    if (parts.length >= 3) {
      const paramId = parseInt(parts[1])
      const value = parts[2]

      // Find and update parameter
      const param = Object.values(mockParameters).find((p) => p.id === paramId)
      if (param) {
        param.value = isNaN(Number(value)) ? value : Number(value)
        return res.send('OK')
      }
    }
    return res.status(400).send('Invalid set command')
  }

  // Handle 'get' command - get <id>
  if (cmd.startsWith('get ')) {
    const parts = cmd.split(' ')
    if (parts.length >= 2) {
      const paramId = parseInt(parts[1])
      const param = Object.values(mockParameters).find((p) => p.id === paramId)
      if (param) {
        return res.send(param.value.toString())
      }
    }
    return res.status(400).send('Invalid get command')
  }

  // Default response
  res.send('OK')
})

app.get('/canmap', (req, res) => {
  const add = req.query.add as string | undefined
  const remove = req.query.remove as string | undefined

  if (add) {
    try {
      const newMapping = JSON.parse(add) as MockCanMapping
      newMapping.index = mockCanMappings.length
      mockCanMappings.push(newMapping)
      console.log('[CAN] Added mapping:', newMapping)
    } catch (e) {
      return res.status(400).send('Invalid mapping JSON')
    }
  }

  if (remove) {
    try {
      const removeMapping = JSON.parse(remove) as Partial<MockCanMapping>
      const index = mockCanMappings.findIndex(
        (m) => m.index === removeMapping.index || m.subindex === removeMapping.subindex
      )
      if (index !== -1) {
        mockCanMappings.splice(index, 1)
        console.log('[CAN] Removed mapping at index:', index)
      }
    } catch (e) {
      return res.status(400).send('Invalid mapping JSON')
    }
  }

  res.json(mockCanMappings)
})

app.get('/devices', (req, res) => {
  const response = {
    devices: savedDevices,
  }
  res.json(response)
})

app.get('/scan', (req, res) => {
  const start = parseInt(req.query.start as string) || 1
  const end = parseInt(req.query.end as string) || 32

  console.log(`[SCAN] Scanning nodes ${start} to ${end}`)

  // Return devices within range
  const scannedDevices = mockDevices
    .filter((d) => d.nodeId >= start && d.nodeId <= end)
    .map((d) => ({
      nodeId: d.nodeId,
      serial: d.serial,
      lastSeen: Date.now(),
    }))

  res.json(scannedDevices)
})

app.get('/reloadjson', (req, res) => {
  console.log('[CMD] Reload JSON')
  res.send('OK')
})

app.get('/resetdevice', (req, res) => {
  console.log('[CMD] Reset device')
  res.send('OK')
})

app.get('/list', (req, res) => {
  res.json([
    { type: 'file', name: 'wifi.txt' },
    { type: 'file', name: 'devices.json' },
    { type: 'file', name: 'params.json' },
  ])
})

app.get('/wifi', (req, res) => {
  res.send('MyNetwork\npassword123')
})

app.get('/settings', (req, res) => {
  const canRXPin = req.query.canRXPin
  const canTXPin = req.query.canTXPin
  const canEnablePin = req.query.canEnablePin
  const canSpeed = req.query.canSpeed

  if (canRXPin || canTXPin || canEnablePin || canSpeed) {
    console.log('[SETTINGS] Updated:', { canRXPin, canTXPin, canEnablePin, canSpeed })
    return res.send('OK')
  }

  res.send('<html>Settings page</html>')
})

// WebSocket connection handling
wss.on('connection', (ws) => {
  console.log('[WS] Client connected')

  // Send initial status
  ws.send(
    JSON.stringify({
      event: 'scanStatus',
      data: { active: scanningActive },
    })
  )

  // Send saved devices
  ws.send(
    JSON.stringify({
      event: 'savedDevices',
      data: { devices: savedDevices },
    })
  )

  ws.on('message', (data) => {
    try {
      const message = JSON.parse(data.toString())
      const { action } = message

      console.log('[WS] Action:', action)

      switch (action) {
        case 'startScan':
          scanningActive = true
          broadcast('scanStatus', { active: true })
          simulateDeviceDiscovery()
          break

        case 'stopScan':
          scanningActive = false
          broadcast('scanStatus', { active: false })
          break

        case 'connect':
          connectedNodeId = message.nodeId || 1
          broadcast('connected', {
            nodeId: connectedNodeId,
            serial: message.serial || '',
          })
          console.log(`[CONNECT] Connected to node ${connectedNodeId}`)
          break

        case 'setDeviceName': {
          const { serial, name, nodeId } = message
          if (savedDevices[serial]) {
            savedDevices[serial].name = name
            if (nodeId !== undefined && nodeId !== -1) {
              savedDevices[serial].nodeId = nodeId
            }
          } else {
            savedDevices[serial] = { nodeId, serial, name, lastSeen: Date.now() }
          }
          ws.send(
            JSON.stringify({
              event: 'deviceNameSet',
              data: { success: true, serial, name },
            })
          )
          console.log(`[DEVICE] Set name for ${serial}: ${name}`)
          break
        }

        case 'getNodeId':
          ws.send(
            JSON.stringify({
              event: 'nodeIdInfo',
              data: { id: connectedNodeId, speed: 250 },
            })
          )
          break

        case 'setNodeId':
          connectedNodeId = message.id || 1
          ws.send(
            JSON.stringify({
              event: 'nodeIdSet',
              data: { id: connectedNodeId, speed: 250 },
            })
          )
          console.log(`[NODE] Set node ID to ${connectedNodeId}`)
          break

        case 'startSpotValues': {
          spotValuesParamIds = message.paramIds || []
          const interval = Math.max(100, Math.min(10000, message.interval || 1000))

          spotValuesActive = true

          if (spotValuesInterval) {
            clearInterval(spotValuesInterval)
          }
          spotValuesInterval = setInterval(broadcastSpotValues, interval)

          ws.send(
            JSON.stringify({
              event: 'spotValuesStatus',
              data: {
                active: true,
                interval,
                paramCount: spotValuesParamIds.length,
              },
            })
          )
          console.log(`[SPOT] Started streaming ${spotValuesParamIds.length} params at ${interval}ms`)
          break
        }

        case 'stopSpotValues':
          spotValuesActive = false
          if (spotValuesInterval) {
            clearInterval(spotValuesInterval)
            spotValuesInterval = null
          }
          spotValuesParamIds = []
          ws.send(
            JSON.stringify({
              event: 'spotValuesStatus',
              data: { active: false },
            })
          )
          console.log('[SPOT] Stopped streaming')
          break

        default:
          console.log('[WS] Unknown action:', action)
      }
    } catch (e) {
      console.error('[WS] Error processing message:', e)
    }
  })

  ws.on('close', () => {
    console.log('[WS] Client disconnected')
  })
})

// Start server
const server = app.listen(PORT, () => {
  console.log(`\n🚀 ESP32 Device Simulator running on http://localhost:${PORT}`)
  console.log(`   WebSocket server ready at ws://localhost:${PORT}/ws`)
  console.log(`\n💡 Configure Vite to proxy to localhost:${PORT} in vite.config.js`)
  console.log(`   Or run: npm run dev:sim\n`)
})

// Handle WebSocket upgrade
server.on('upgrade', (request, socket, head) => {
  if (request.url === '/ws') {
    wss.handleUpgrade(request, socket, head, (ws) => {
      wss.emit('connection', ws, request)
    })
  } else {
    socket.destroy()
  }
})

// Graceful shutdown
process.on('SIGINT', () => {
  console.log('\n👋 Shutting down simulator...')
  if (scanInterval) clearInterval(scanInterval)
  if (spotValuesInterval) clearInterval(spotValuesInterval)
  server.close(() => {
    process.exit(0)
  })
})
