import { useState, useEffect } from 'preact/hooks'
import { useLocation } from 'wouter'
import { useIntlayer } from 'preact-intlayer'
import DeviceScanner from '@components/DeviceScanner'
import DeviceNaming from '@components/DeviceNaming'
import DeviceListItem from '@components/DeviceListItem'
import DisconnectedState from '@components/DisconnectedState'
import Layout from '@components/Layout'
import { LoadingSpinner } from '@components/LoadingSpinner'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { useDeviceContext, type MergedDevice } from '@contexts/DeviceContext'
import { api } from '@api/inverter'
import { sortDevicesByLastSeen } from '@utils/deviceSort'

export default function SystemOverview() {
  const [, setLocation] = useLocation()
  const content = useIntlayer('system-overview')
  const [namingDevice, setNamingDevice] = useState<MergedDevice | null>(null)
  const [renamingDevice, setRenamingDevice] = useState<MergedDevice | null>(null)
  const [configuredScanRange, setConfiguredScanRange] = useState({ start: 1, end: 32 })

  // Use shared WebSocket connection
  const { isConnected, isConnecting, isRetrying } = useWebSocketContext()

  // Use shared Device context
  const {
    mergedDevices,
    scanning,
    lastConnectedSerial,
    isDeviceOnline,
    startScan,
    stopScan,
    setDeviceName,
    connectToDevice,
    currentScanNode,
    scanRange: activeScanRange,
    deleteDevice,
    renameDevice
  } = useDeviceContext()

  // Auto-start scanning when page loads and WebSocket is connected
  useEffect(() => {
    if (isConnected && !scanning && configuredScanRange) {
      console.log('[SystemOverview] Auto-starting scan on mount')
      startScan(configuredScanRange.start, configuredScanRange.end)
    }
  }, [isConnected, configuredScanRange.start, configuredScanRange.end])

  // Stop scanning when component unmounts (navigating away)
  useEffect(() => {
    return () => {
      if (scanning) {
        stopScan()
        console.log('[SystemOverview] Stopped scanning on unmount')
      }
    }
  }, [scanning, stopScan])

  // Load scan range from settings
  useEffect(() => {
    const loadScanRange = async () => {
      try {
        const settings = await api.getSettings()
        setConfiguredScanRange({
          start: settings.scanStartNode,
          end: settings.scanEndNode
        })
      } catch (error) {
        console.error('Failed to load scan range settings:', error)
        // Keep defaults if loading fails
      }
    }
    loadScanRange()
  }, [])

  // Reconnect handler (WebSocket auto-reconnects, this just reloads the page)
  const handleReconnect = () => {
    window.location.reload()
  }

  const handleScan = () => {
    if (!isConnected) {
      console.warn('Cannot scan: WebSocket not connected')
      return
    }

    if (scanning) {
      // Stop scan
      stopScan()
    } else {
      // Start continuous scan with configured range
      startScan(configuredScanRange.start, configuredScanRange.end)
    }
  }

  const handleDeviceSelect = (device: MergedDevice) => {
    // If device has no name, show naming dialog
    if (!device.name) {
      setNamingDevice(device)
      return
    }

    // Connect to device
    if (device.nodeId !== undefined) {
      handleDeviceConnect(device.nodeId, device.serial)
    }
  }

  const handleDeviceName = (name: string) => {
    if (!namingDevice) return

    // Update device name in context
    setDeviceName(namingDevice.serial, name, namingDevice.nodeId)

    // Connect to device
    if (namingDevice.nodeId !== undefined) {
      handleDeviceConnect(namingDevice.nodeId, namingDevice.serial)
    }

    setNamingDevice(null)
  }

  const handleDeviceConnect = (nodeId: number, serial: string) => {
    // Update context and send WebSocket message
    connectToDevice(nodeId, serial)

    // Navigate to device settings page
    setLocation(`/devices/${serial}`)
  }

  const handleDeleteDevice = (device: MergedDevice, e: Event) => {
    e.stopPropagation()

    if (confirm(content.confirmDelete.value || content.confirmDelete)) {
      deleteDevice(device.serial)
    }
  }

  const handleRenameClick = (device: MergedDevice, e: Event) => {
    e.stopPropagation()
    setRenamingDevice(device)
  }

  const handleRenameSubmit = (newName: string) => {
    if (!renamingDevice) return
    renameDevice(renamingDevice.serial, newName)
    setRenamingDevice(null)
  }

  return (
    <Layout onQuickScan={handleScan} pageTitle={content.title}>
      <div class="container">
        <div class="page-header">
          <h1 class="page-title">{content.title}</h1>
          <p class="page-subtitle">{content.subtitle}</p>
        </div>

        <DeviceScanner
          scanning={scanning}
          currentScanNode={currentScanNode}
          {...(activeScanRange && { scanRange: activeScanRange })}
        />

        {mergedDevices.length === 0 ? (
          !isConnected ? (
            <DisconnectedState onReconnect={handleReconnect} isConnecting={isConnecting} isRetrying={isRetrying} />
          ) : (
            <div class="empty-state-centered">
              {scanning ? (
              <>
                <LoadingSpinner size="large" />
                <p class="empty-state-text">{content.scanningForDevices}</p>
                <p class="empty-state-hint">{content.searchingNodes}</p>
              </>
            ) : (
              <>
                <svg class="empty-state-icon" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.5">
                  <rect x="2" y="7" width="20" height="14" rx="2"></rect>
                  <path d="M17 2v5m-10-5v5"></path>
                </svg>
                <p class="empty-state-text">{content.noDevicesFound}</p>
                <p class="empty-state-hint">{content.startScanHint}</p>
              </>
              )}
            </div>
          )
        ) : (
          <div class="device-list">
            {sortDevicesByLastSeen(mergedDevices).map(device => (
              <DeviceListItem
                key={device.serial}
                device={device}
                isLastConnected={device.serial === lastConnectedSerial}
                isOnline={isDeviceOnline(device.serial)}
                onSelect={handleDeviceSelect}
                onRename={handleRenameClick}
                onDelete={handleDeleteDevice}
              />
            ))}
          </div>
        )}

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
            mode="rename"
          />
        )}
      </div>
    </Layout>
  )
}
