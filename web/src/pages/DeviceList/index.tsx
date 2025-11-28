import { useState, useEffect } from 'preact/hooks'
import { useLocation } from 'wouter'
import { useIntlayer } from 'preact-intlayer'
import { api, SavedDevice } from '@api/inverter'
import DeviceScanner from '@components/DeviceScanner'
import DeviceNaming from '@components/DeviceNaming'
import { useWebSocket } from '@hooks/useWebSocket'
import { saveLastDevice, getLastDevice } from '@utils/lastDevice'

interface MergedDevice {
  serial: string
  nodeId?: number
  name?: string
  lastSeen?: number
}

export default function DeviceList() {
  const [, setLocation] = useLocation()
  const content = useIntlayer('device-list')
  const [savedDevices, setSavedDevices] = useState<Record<string, SavedDevice>>({})
  const [scanning, setScanning] = useState(false)
  const [namingDevice, setNamingDevice] = useState<MergedDevice | null>(null)
  const [lastConnectedSerial, setLastConnectedSerial] = useState<string | null>(null)

  // WebSocket connection for real-time device updates
  const { isConnected, sendMessage } = useWebSocket('/ws', {
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

  const handleScan = (fullScan: boolean = false) => {
    const start = 1
    const end = fullScan ? 127 : 32

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
    <div class="container">
      <header class="header">
        <h1>{content.title}</h1>
        <button class="btn-secondary" onClick={() => setLocation('/settings')}>
          {content.settings}
        </button>
      </header>

      <DeviceScanner
        scanning={scanning}
        onScan={handleScan}
        deviceCount={mergedDevices.length}
      />

      <div class="device-list">
        {mergedDevices.length === 0 ? (
          <div class="empty-state">
            <p>{scanning ? content.scanningForDevices : content.noDevicesFound}</p>
            {!scanning && <p class="hint">{content.scanHint}</p>}
          </div>
        ) : (
          mergedDevices.map(device => {
            const isLastConnected = device.serial === lastConnectedSerial
            return (
              <div
                key={device.serial}
                class="device-card"
                onClick={() => handleDeviceSelect(device)}
              >
                <div class="device-name">
                  {device.name || content.unnamedDevice}
                  {isLastConnected && <span class="last-connected-badge">{content.lastConnected}</span>}
                </div>
                <div class="device-info">
                  <div class="device-serial">{content.serial} {device.serial}</div>
                  {device.nodeId !== undefined && (
                    <div class="device-node">{content.nodeId} {device.nodeId}</div>
                  )}
                  {device.lastSeen && (
                    <div class="device-status online">{content.online}</div>
                  )}
                </div>
              </div>
            )
          })
        )}
      </div>

      {namingDevice && (
        <DeviceNaming
          serial={namingDevice.serial}
          onSave={handleDeviceName}
          onCancel={() => setNamingDevice(null)}
        />
      )}
    </div>
  )
}
