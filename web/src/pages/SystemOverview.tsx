import { useEffect, useState } from 'preact/hooks'
import { useLocation } from 'wouter'
import { api, SavedDevice } from '../api/inverter'
import DeviceScanner from '../components/DeviceScanner'
import DeviceNaming from '../components/DeviceNaming'
import DisconnectedState from '../components/DisconnectedState'
import Layout from '../components/Layout'
import { useWebSocket } from '../hooks/useWebSocket'
import { saveLastDevice, getLastDevice } from '../utils/lastDevice'

interface MergedDevice {
  serial: string
  nodeId?: number
  name?: string
  lastSeen?: number
}

export default function SystemOverview() {
  const [, setLocation] = useLocation()
  const [savedDevices, setSavedDevices] = useState<Record<string, SavedDevice>>({})
  const [scanning, setScanning] = useState(false)
  const [namingDevice, setNamingDevice] = useState<MergedDevice | null>(null)
  const [lastConnectedSerial, setLastConnectedSerial] = useState<string | null>(null)

  // WebSocket connection for real-time device updates
  const { isConnected, sendMessage, reconnect } = useWebSocket('/ws', {
    onMessage: (message) => {
      console.log('WebSocket event:', message.event, message.data)

      switch (message.event) {
        case 'deviceDiscovered':
          // Update device in saved devices
          setSavedDevices(prev => {
            const serial = message.data.serial
            const existing = prev[serial] || {}
            return {
              ...prev,
              [serial]: {
                ...existing,
                nodeId: message.data.nodeId,
                lastSeen: message.data.lastSeen,
                name: message.data.name || existing.name,
              }
            }
          })
          break

        case 'scanStatus':
          setScanning(message.data.active)
          break

        case 'savedDevices':
          if (message.data.devices) {
            setSavedDevices(message.data.devices)
          }
          break

        case 'connected':
          console.log('Connected to device:', message.data)
          break
      }
    },
    onOpen: () => {
      console.log('WebSocket connected, loading devices...')
      loadDevices()
    },
    onClose: () => {
      // Stop scanning if connection is lost
      setScanning(false)
    }
  })

  useEffect(() => {
    loadDevices()

    // Load last connected device from localStorage
    const lastDevice = getLastDevice()
    if (lastDevice) {
      setLastConnectedSerial(lastDevice.serial)
    }
  }, [])

  const loadDevices = async () => {
    try {
      const response = await api.getSavedDevices()
      setSavedDevices(response.devices || {})
    } catch (error) {
      console.error('Failed to load devices:', error)
    }
  }

  const handleScan = () => {
    if (!isConnected) {
      console.warn('Cannot scan: WebSocket not connected')
      return
    }

    const start = 0
    const end = 255

    if (scanning) {
      // Stop scan
      sendMessage('stopScan')
    } else {
      // Start continuous scan
      sendMessage('startScan', { start, end })
    }
  }

  const mergeDevices = (): MergedDevice[] => {
    // Convert saved devices object to array
    return Object.entries(savedDevices).map(([serial, device]) => ({
      serial,
      name: device.name,
      nodeId: device.nodeId,
      lastSeen: device.lastSeen,
    }))
  }

  const handleDeviceSelect = (device: MergedDevice) => {
    // If device has no name, show naming dialog
    if (!device.name) {
      setNamingDevice(device)
      return
    }

    // Connect to device
    if (device.nodeId !== undefined) {
      connectToDevice(device.nodeId, device.serial)
    }
  }

  const handleDeviceName = (name: string) => {
    if (!namingDevice) return

    // Send via WebSocket
    sendMessage('setDeviceName', {
      serial: namingDevice.serial,
      name: name,
      nodeId: namingDevice.nodeId
    })

    // Update local state
    setSavedDevices(prev => ({
      ...prev,
      [namingDevice.serial]: {
        ...prev[namingDevice.serial],
        name: name
      }
    }))

    // Connect to device
    if (namingDevice.nodeId !== undefined) {
      connectToDevice(namingDevice.nodeId, namingDevice.serial)
    }

    setNamingDevice(null)
  }

  const connectToDevice = (nodeId: number, serial: string) => {
    // Save to localStorage as last connected device
    saveLastDevice(serial, nodeId)
    setLastConnectedSerial(serial)

    // Send connection command via WebSocket
    sendMessage('connect', {
      nodeId: nodeId,
      serial: serial
    })

    // Navigate to device settings page
    setLocation(`/devices/${serial}`)
  }

  const mergedDevices = mergeDevices()

  return (
    <Layout onQuickScan={handleScan}>
      <div class="container">
        <div class="page-header">
          <h1 class="page-title">System Overview</h1>
          <p class="page-subtitle">Scan and manage Open Inverter devices on your CAN bus</p>
        </div>

        <DeviceScanner
          scanning={scanning}
          onScan={handleScan}
          deviceCount={mergedDevices.length}
          disabled={!isConnected}
        />

        {mergedDevices.length === 0 ? (
          !isConnected ? (
            <DisconnectedState onReconnect={reconnect} />
          ) : (
            <div class="empty-state-centered">
              {scanning ? (
              <>
                <div class="spinner-large"></div>
                <p class="empty-state-text">Scanning for devices...</p>
                <p class="empty-state-hint">Searching nodes 0-255 on the CAN bus</p>
              </>
            ) : (
              <>
                <svg class="empty-state-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
                  <rect x="2" y="7" width="20" height="14" rx="2"></rect>
                  <path d="M17 2v5m-10-5v5"></path>
                </svg>
                <p class="empty-state-text">No devices found</p>
                <p class="empty-state-hint">Start a scan to discover devices on your CAN bus</p>
                <button class="btn-with-icon" onClick={handleScan}>
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                    <path d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z"></path>
                  </svg>
                  <span>Scan Devices</span>
                </button>
              </>
              )}
            </div>
          )
        ) : (
          <div class="device-list">
            {mergedDevices.map(device => {
              const isLastConnected = device.serial === lastConnectedSerial
              return (
                <div
                  key={device.serial}
                  class="device-card"
                  onClick={() => handleDeviceSelect(device)}
                >
                  <div class="device-name">
                    {device.name || 'Unnamed Device'}
                    {isLastConnected && <span class="last-connected-badge">Last Connected</span>}
                  </div>
                  <div class="device-info">
                    <div class="device-serial">Serial: {device.serial}</div>
                    {device.nodeId !== undefined && (
                      <div class="device-node">Node ID: {device.nodeId}</div>
                    )}
                    {device.lastSeen && (
                      <div class="device-status online">Online</div>
                    )}
                  </div>
                </div>
              )
            })}
          </div>
        )}

        {namingDevice && (
          <DeviceNaming
            serial={namingDevice.serial}
            onSave={handleDeviceName}
            onCancel={() => setNamingDevice(null)}
          />
        )}
      </div>
    </Layout>
  )
}
