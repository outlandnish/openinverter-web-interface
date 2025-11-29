import { useState } from 'preact/hooks'
import { useLocation } from 'wouter'
import { useIntlayer } from 'preact-intlayer'
import DeviceScanner from '@components/DeviceScanner'
import DeviceNaming from '@components/DeviceNaming'
import DisconnectedState from '@components/DisconnectedState'
import Layout from '@components/Layout'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { useDeviceContext, type MergedDevice } from '@contexts/DeviceContext'

export default function SystemOverview() {
  const [, setLocation] = useLocation()
  const content = useIntlayer('system-overview')
  const [namingDevice, setNamingDevice] = useState<MergedDevice | null>(null)

  // Use shared WebSocket connection
  const { isConnected } = useWebSocketContext()

  // Use shared Device context
  const {
    mergedDevices,
    scanning,
    lastConnectedSerial,
    isDeviceOnline,
    startScan,
    stopScan,
    setDeviceName,
    connectToDevice
  } = useDeviceContext()

  // Reconnect handler (WebSocket auto-reconnects, this just reloads the page)
  const handleReconnect = () => {
    window.location.reload()
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
      stopScan()
    } else {
      // Start continuous scan
      startScan(start, end)
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

  return (
    <Layout onQuickScan={handleScan}>
      <div class="container">
        <div class="page-header">
          <h1 class="page-title">{content.title}</h1>
          <p class="page-subtitle">{content.subtitle}</p>
        </div>

        <DeviceScanner
          scanning={scanning}
          onScan={handleScan}
          deviceCount={mergedDevices.length}
          disabled={!isConnected}
        />

        {mergedDevices.length === 0 ? (
          !isConnected ? (
            <DisconnectedState onReconnect={handleReconnect} />
          ) : (
            <div class="empty-state-centered">
              {scanning ? (
              <>
                <div class="spinner-large"></div>
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
                <button class="btn-with-icon" onClick={handleScan}>
                  <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                    <path d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z"></path>
                  </svg>
                  <span>{content.scanDevices}</span>
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
                    {device.name || content.unnamedDevice}
                    {isLastConnected && <span class="last-connected-badge">{content.lastConnected}</span>}
                  </div>
                  <div class="device-info">
                    <div class="device-serial">{content.serial} {device.serial}</div>
                    {device.nodeId !== undefined && (
                      <div class="device-node">{content.nodeId} {device.nodeId}</div>
                    )}
                    <div class={`device-status ${isDeviceOnline(device.serial) ? 'online' : 'offline'}`}>
                      {isDeviceOnline(device.serial) ? content.online : content.offline}
                    </div>
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
