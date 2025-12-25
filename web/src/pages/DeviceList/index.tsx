import { useState, useEffect } from 'preact/hooks'
import { useLocation } from 'wouter'
import { useIntlayer } from 'preact-intlayer'
import { api, SavedDevice } from '@api/inverter'
import Layout from '@components/Layout'
import DeviceScanner from '@components/DeviceScanner'
import DeviceNaming from '@components/DeviceNaming'
import { useWebSocketContext } from '@contexts/WebSocketContext'
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
  const [renamingDevice, setRenamingDevice] = useState<MergedDevice | null>(null)
  const [lastConnectedSerial, setLastConnectedSerial] = useState<string | null>(null)

  // WebSocket connection for real-time device updates
  const { isConnected, sendMessage, subscribe } = useWebSocketContext()

  // Subscribe to WebSocket messages
  useEffect(() => {
    const unsubscribe = subscribe((message) => {
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

        case 'deviceDeleted':
          if (message.data.success) {
            setSavedDevices(prev => {
              const updated = { ...prev }
              delete updated[message.data.serial]
              return updated
            })
            console.log('Device deleted:', message.data.serial)
          }
          break

        case 'deviceRenamed':
          if (message.data.success) {
            setSavedDevices(prev => ({
              ...prev,
              [message.data.serial]: {
                ...prev[message.data.serial],
                name: message.data.name
              }
            }))
            setRenamingDevice(null)
            console.log('Device renamed:', message.data.serial, '->', message.data.name)
          }
          break
      }
    })

    return unsubscribe
  }, [subscribe])

  // Load devices when connected
  useEffect(() => {
    if (isConnected) {
      console.log('WebSocket connected, loading devices...')
      loadDevices()
    }
  }, [isConnected])

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

  const handleDeleteDevice = (device: MergedDevice, e: Event) => {
    e.stopPropagation()
    
    if (confirm(content.confirmDelete)) {
      sendMessage('deleteDevice', {
        serial: device.serial
      })
    }
  }

  const handleRenameClick = (device: MergedDevice, e: Event) => {
    e.stopPropagation()
    setRenamingDevice(device)
  }

  const handleRenameSubmit = (newName: string) => {
    if (!renamingDevice) return

    sendMessage('renameDevice', {
      serial: renamingDevice.serial,
      name: newName
    })

    // Update local state optimistically
    setSavedDevices(prev => ({
      ...prev,
      [renamingDevice.serial]: {
        ...prev[renamingDevice.serial],
        name: newName
      }
    }))
  }

  const mergedDevices = mergeDevices()

  return (
    <Layout pageTitle={content.title}>
      <div class="container">
        <header class="header">
          <h1>{content.title}</h1>
          <button class="btn-secondary" onClick={() => setLocation('/settings')}>
            {content.settings}
          </button>
        </header>

        {scanning && <DeviceScanner scanning={scanning} />}

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
                <div class="device-actions">
                  {device.name && (
                    <button 
                      class="btn-secondary btn-small" 
                      onClick={(e) => handleRenameClick(device, e)}
                      title={content.rename}
                    >
                      ✏️ {content.rename}
                    </button>
                  )}
                  <button 
                    class="btn-danger btn-small" 
                    onClick={(e) => handleDeleteDevice(device, e)}
                    title={content.delete}
                  >
                    🗑️ {content.delete}
                  </button>
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

      {renamingDevice && (
        <DeviceNaming
          serial={renamingDevice.serial}
          onSave={handleRenameSubmit}
          onCancel={() => setRenamingDevice(null)}
        />
      )}
      </div>
    </Layout>
  )
}
