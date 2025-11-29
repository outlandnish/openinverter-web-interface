import { createContext, ComponentChildren } from 'preact'
import { useContext, useEffect, useState, useCallback } from 'preact/hooks'
import { useWebSocketContext } from './WebSocketContext'
import { useToast } from '@hooks/useToast'
import { api, SavedDevice } from '@api/inverter'
import { saveLastDevice, getLastDevice } from '@utils/lastDevice'

export interface MergedDevice {
  serial: string
  nodeId?: number
  name?: string
  lastSeen?: number
}

export interface DeviceContextValue {
  // Device data
  savedDevices: Record<string, SavedDevice>
  mergedDevices: MergedDevice[]

  // Scanning state
  scanning: boolean
  devicesSeenInCurrentScan: Set<string>
  lastScanStartTime: number
  currentScanNode: number | null
  scanRange: { start: number; end: number } | null

  // Last connected device
  lastConnectedSerial: string | null

  // Actions
  loadDevices: () => Promise<void>
  startScan: (start: number, end: number) => void
  stopScan: () => void
  setDeviceName: (serial: string, name: string, nodeId?: number) => void
  connectToDevice: (nodeId: number, serial: string) => void
  isDeviceOnline: (serial: string) => boolean
}

const DeviceContext = createContext<DeviceContextValue | null>(null)

interface DeviceProviderProps {
  children: ComponentChildren
}

export function DeviceProvider({ children }: DeviceProviderProps) {
  const [savedDevices, setSavedDevices] = useState<Record<string, SavedDevice>>({})
  const [scanning, setScanning] = useState(false)
  const [lastConnectedSerial, setLastConnectedSerial] = useState<string | null>(null)
  const [lastScanStartTime, setLastScanStartTime] = useState<number>(0)
  const [devicesSeenInCurrentScan, setDevicesSeenInCurrentScan] = useState<Set<string>>(new Set())
  const [currentScanNode, setCurrentScanNode] = useState<number | null>(null)
  const [scanRange, setScanRange] = useState<{ start: number; end: number } | null>(null)

  // Use shared WebSocket connection
  const { isConnected, sendMessage, subscribe } = useWebSocketContext()

  // Use toast for error notifications
  const { showError } = useToast()

  // Subscribe to WebSocket messages for device updates
  useEffect(() => {
    const unsubscribe = subscribe((message: any) => {
      console.log('[DeviceContext] WebSocket event:', message.event, message.data)

      switch (message.event) {
        case 'error':
          console.error('[DeviceContext] Error:', message.data.message)
          showError(message.data.message)
          break

        case 'deviceDiscovered':
          console.log('Device discovered:', {
            serial: message.data.serial,
            nodeId: message.data.nodeId,
            name: message.data.name || 'Unnamed',
            lastSeen: message.data.lastSeen
          })

          // Track device as seen in current scan
          setDevicesSeenInCurrentScan(prev => {
            const updated = new Set(prev)
            updated.add(message.data.serial)
            console.log('Devices seen in scan:', Array.from(updated))
            return updated
          })

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
          // Track when scan starts
          if (message.data.active) {
            const scanStartTime = Date.now()
            console.log('Scan started at:', scanStartTime, new Date(scanStartTime).toLocaleString())
            setLastScanStartTime(scanStartTime)
            // Clear the devices seen in current scan
            setDevicesSeenInCurrentScan(new Set())
          } else {
            // Clear scan progress when scan stops
            setCurrentScanNode(null)
            setScanRange(null)
          }
          break

        case 'scanProgress':
          setCurrentScanNode(message.data.currentNode)
          setScanRange({
            start: message.data.startNode,
            end: message.data.endNode
          })
          break

        case 'savedDevices':
          if (message.data.devices) {
            setSavedDevices(message.data.devices)
          }
          break
      }
    })

    return unsubscribe
  }, [subscribe])

  // Load devices when WebSocket connects
  useEffect(() => {
    if (isConnected) {
      console.log('[DeviceContext] WebSocket connected, loading devices...')
      loadDevices()
    } else {
      // Stop scanning if connection is lost
      setScanning(false)
    }
  }, [isConnected])

  // Load last connected device from localStorage on mount
  useEffect(() => {
    const lastDevice = getLastDevice()
    if (lastDevice) {
      setLastConnectedSerial(lastDevice.serial)
    }
    loadDevices()
  }, [])

  const loadDevices = useCallback(async () => {
    try {
      const response = await api.getSavedDevices()
      setSavedDevices(response.devices || {})
    } catch (error) {
      console.error('[DeviceContext] Failed to load devices:', error)
    }
  }, [])

  const startScan = useCallback((start: number, end: number) => {
    if (!isConnected) {
      console.warn('[DeviceContext] Cannot scan: WebSocket not connected')
      return
    }
    sendMessage('startScan', { start, end })
  }, [isConnected, sendMessage])

  const stopScan = useCallback(() => {
    sendMessage('stopScan')
  }, [sendMessage])

  const setDeviceName = useCallback((serial: string, name: string, nodeId?: number) => {
    // Send via WebSocket
    sendMessage('setDeviceName', {
      serial,
      name,
      nodeId
    })

    // Update local state
    setSavedDevices(prev => ({
      ...prev,
      [serial]: {
        ...prev[serial],
        name
      }
    }))
  }, [sendMessage])

  const connectToDevice = useCallback((nodeId: number, serial: string) => {
    // Save to localStorage as last connected device
    saveLastDevice(serial, nodeId)
    setLastConnectedSerial(serial)

    // Send connection command via WebSocket
    sendMessage('connect', {
      nodeId,
      serial
    })
  }, [sendMessage])

  const isDeviceOnline = useCallback((serial: string): boolean => {
    // Device is online if:
    // 1. It was seen in the current scan, OR
    // 2. It has a recent lastSeen timestamp (within last 30 seconds)

    if (devicesSeenInCurrentScan.has(serial)) {
      return true
    }

    const device = savedDevices[serial]
    if (!device || !device.lastSeen) {
      return false
    }

    const ONLINE_THRESHOLD_MS = 30000 // 30 seconds
    const now = Date.now()
    const timeSinceLastSeen = now - device.lastSeen

    return timeSinceLastSeen < ONLINE_THRESHOLD_MS
  }, [devicesSeenInCurrentScan, savedDevices])

  const mergedDevices = useCallback((): MergedDevice[] => {
    // Convert saved devices object to array
    return Object.entries(savedDevices).map(([serial, device]) => ({
      serial,
      name: device.name,
      nodeId: device.nodeId,
      lastSeen: device.lastSeen,
    }))
  }, [savedDevices])

  const value: DeviceContextValue = {
    savedDevices,
    mergedDevices: mergedDevices(),
    scanning,
    devicesSeenInCurrentScan,
    lastScanStartTime,
    currentScanNode,
    scanRange,
    lastConnectedSerial,
    loadDevices,
    startScan,
    stopScan,
    setDeviceName,
    connectToDevice,
    isDeviceOnline,
  }

  return (
    <DeviceContext.Provider value={value}>
      {children}
    </DeviceContext.Provider>
  )
}

export function useDeviceContext() {
  const context = useContext(DeviceContext)
  if (!context) {
    throw new Error('useDeviceContext must be used within a DeviceProvider')
  }
  return context
}
