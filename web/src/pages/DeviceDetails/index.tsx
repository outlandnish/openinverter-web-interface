import { useState, useEffect } from 'preact/hooks'
import { useRoute } from 'wouter'
import { useIntlayer } from 'preact-intlayer'
import { useParams } from '@hooks/useParams'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { DeviceDetailsProvider, useDeviceDetailsContext } from '@contexts/DeviceDetailsContext'
import Layout from '@components/Layout'
import ConnectionStatus from '@components/ConnectionStatus'
import SpotValuesMonitor from '@components/SpotValuesMonitor'
import OTAUpdate from '@components/OTAUpdate'
import DeviceParameters from '@components/DeviceParameters'
import CanMappingEditor from '@components/CanMappingEditor'
import CanMessageSender from '@components/CanMessageSender'
import CanIoControl from '@components/CanIoControl'
import Tabs from '@components/Tabs'
import { LoadingSpinner } from '@components/LoadingSpinner'
import { useToast } from '@hooks/useToast'
import { formatParameterValue } from '@/utils/parameterDisplay'
import { api } from '@/api/inverter'

export default function DeviceDetails() {
  return (
    <DeviceDetailsProvider>
      <DeviceDetailsContent />
    </DeviceDetailsProvider>
  )
}

function DeviceDetailsContent() {
  const [, routeParams] = useRoute('/devices/:serial')
  const content = useIntlayer('device-details')
  const { showSuccess, showWarning } = useToast()

  const [loading, setLoading] = useState(true)
  const [nodeId, setNodeId] = useState('')
  const [savedNodeId, setSavedNodeId] = useState<number>(0)
  const [firmwareVersion, setFirmwareVersion] = useState('')
  const [deviceConnected, setDeviceConnected] = useState(false)
  const [deviceName, setDeviceName] = useState<string>('')

  // Load device parameters (only when savedNodeId is available)
  const { params } = useParams(
    routeParams?.serial, 
    savedNodeId > 0 ? savedNodeId : undefined
  )

  // Use shared WebSocket connection
  const { isConnected, sendMessage, subscribe } = useWebSocketContext()

  // Use shared device details context
  const { monitoring, canIo, setStreaming, clearHistoricalData, setCanIoActive, setConnectedSerial } = useDeviceDetailsContext()

  // Disconnect from device and stop all activities when component unmounts (navigating away)
  useEffect(() => {
    return () => {
      // Stop streaming if active (check current state at cleanup time)
      sendMessage('stopSpotValues')
      setStreaming(false)
      clearHistoricalData()

      // Stop CAN IO if active
      sendMessage('stopCanIoInterval', {})
      setCanIoActive(false)

      // Send disconnect message when leaving the page
      sendMessage('disconnect')
      console.log('[DeviceDetails] Sent disconnect and cleanup on unmount')
    }
  }, [])

  // Subscribe to WebSocket messages and load settings when ready
  useEffect(() => {
    const unsubscribe = subscribe((message: any) => {
      console.log('WebSocket event:', message.event, message.data)

      switch (message.event) {
        case 'connected':
          // Always track which device is connected, regardless of which one we expected
          setConnectedSerial(message.data.serial)
          // Only set deviceConnected if it matches what we're viewing
          if (message.data.serial === routeParams?.serial) {
            setDeviceConnected(true)
          }
          break

        case 'disconnected':
          setDeviceConnected(false)
          setConnectedSerial(null)
          // Stop streaming when disconnected
          if (monitoring.streaming) {
            setStreaming(false)
            clearHistoricalData()
          }
          showWarning(content.deviceDisconnected)
          break

        case 'nodeIdInfo':
          setNodeId(message.data.id.toString())
          setLoading(false)
          setDeviceConnected(true)
          break

        case 'nodeIdSet':
          setNodeId(message.data.id.toString())
          showSuccess(content.nodeIdSaved)
          break
      }
    })

    return unsubscribe
  }, [isConnected])

  // Load settings when WebSocket is connected AND savedNodeId is available
  useEffect(() => {
    if (isConnected && savedNodeId > 0) {
      console.log('WebSocket connected and nodeId loaded, loading settings...')
      loadSettings()
    }
  }, [isConnected, savedNodeId])

  // Load device name and nodeId from saved devices
  useEffect(() => {
    const loadDeviceInfo = async () => {
      try {
        const response = await api.getSavedDevices()
        const serial = routeParams?.serial
        if (serial && response.devices && response.devices[serial]) {
          setDeviceName(response.devices[serial].name || '')
          setSavedNodeId(response.devices[serial].nodeId || 0)
        }
      } catch (error) {
        console.error('Failed to load device info:', error)
      }
    }

    if (routeParams?.serial) {
      loadDeviceInfo()
    }
  }, [routeParams?.serial])

  // Update firmware version from params
  useEffect(() => {
    if (params && params['version']) {
      setFirmwareVersion(formatParameterValue(params['version'], params['version'].value))
    }
  }, [params])

  const loadSettings = () => {
    setLoading(true)

    // First connect to the device, then get node ID
    if (routeParams?.serial) {
      // Connect to the device with the serial and saved nodeId
      sendMessage('connect', {
        serial: routeParams.serial,
        nodeId: savedNodeId
      })

      // Also request the node ID to verify/update it
      sendMessage('getNodeId')
    }
  }

  const handleSaveNodeId = () => {
    const id = parseInt(nodeId)
    if (isNaN(id)) {
      alert('Please enter a valid node ID')
      return
    }
    sendMessage('setNodeId', { id })
  }

  const handleResetDevice = () => {
    if (confirm(content.resetDeviceConfirm.value)) {
      try {
        // Stop all ongoing activities
        sendMessage('stopSpotValues')
        setStreaming(false)
        clearHistoricalData()

        // Stop CAN IO if active
        sendMessage('stopCanIoInterval', {})
        setCanIoActive(false)

        // Send reset command
        sendMessage('resetDevice', {})
        showSuccess(content.deviceResetSuccess.value)

        // Set loading state while device resets
        setLoading(true)
        setDeviceConnected(false)

        // After a timeout, reload settings
        setTimeout(() => {
          loadSettings()
        }, 3000)
      } catch (error) {
        showWarning(content.deviceResetFailed.value)
      }
    }
  }

  return (
    <Layout currentSerial={routeParams?.serial}>
      <div class="container">
      {loading ? (
        <div class="loading-container" style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            minHeight: '400px'
          }}>
            <LoadingSpinner size="large" label={content.connectingToDevice} />
          </div>
        ) : (
          <>
            <div class="page-header">
              <div class="page-title-row" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <h1 class="page-title">{deviceName || routeParams?.serial || content.deviceMonitor}</h1>
                <ConnectionStatus
                  connected={deviceConnected}
                  label={deviceConnected ? content.connected : content.disconnected}
                />
              </div>
              <div class="device-info-inline" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
                <div style={{ display: 'flex', gap: '1rem', alignItems: 'center' }}>
                  <div class="info-badge">
                    <span class="info-label">{content.serial}</span>
                    <span class="info-value">{routeParams?.serial || content.unknown}</span>
                  </div>
                  <div class="info-badge">
                    <span class="info-label">{content.nodeId}</span>
                    <span class="info-value">{nodeId || 'N/A'}</span>
                  </div>
                  <div class="info-badge">
                    <span class="info-label">{content.firmware}</span>
                    <span class="info-value">{firmwareVersion || content.unknown}</span>
                  </div>
                </div>
                <button
                  class="icon-button reset-button"
                  onClick={handleResetDevice}
                  disabled={!deviceConnected}
                  title={content.resetDevice.value}
                  style={{
                    width: '2.5rem',
                    height: '2.5rem',
                    padding: '0.5rem',
                    border: 'none',
                    borderRadius: '0.5rem',
                    background: 'var(--oi-beige)',
                    color: 'var(--text-primary)',
                    cursor: deviceConnected ? 'pointer' : 'not-allowed',
                    opacity: deviceConnected ? 1 : 0.5,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'center',
                    transition: 'all 0.2s ease'
                  }}
                  onMouseEnter={(e) => {
                    if (deviceConnected) {
                      e.currentTarget.style.background = '#f5e5d3'
                      e.currentTarget.style.transform = 'scale(1.05)'
                    }
                  }}
                  onMouseLeave={(e) => {
                    e.currentTarget.style.background = 'var(--oi-beige)'
                    e.currentTarget.style.transform = 'scale(1)'
                  }}
                >
                  <svg
                    viewBox="0 0 24 24"
                    fill="none"
                    stroke="currentColor"
                    stroke-width="2"
                    stroke-linecap="round"
                    stroke-linejoin="round"
                    style={{ width: '1.25rem', height: '1.25rem' }}
                  >
                    <path d="M21.5 2v6h-6M2.5 22v-6h6M2 11.5a10 10 0 0 1 18.8-4.3M22 12.5a10 10 0 0 1-18.8 4.2" />
                  </svg>
                </button>
              </div>
            </div>

      {/* Tabbed Interface */}
      <Tabs
        tabs={[
          {
            id: 'overview',
            label: 'Overview',
            content: savedNodeId > 0 && routeParams?.serial ? (
              <SpotValuesMonitor serial={routeParams.serial} nodeId={savedNodeId} showHeader={false} />
            ) : (
              <div style={{ padding: '2rem', textAlign: 'center', color: 'var(--text-muted)' }}>
                {content.noDataAvailable || 'No data available'}
              </div>
            ),
            disabled: !savedNodeId || savedNodeId === 0
          },
          {
            id: 'parameters',
            label: 'Parameters',
            content: routeParams?.serial && nodeId ? (
              <DeviceParameters
                serial={routeParams.serial}
                nodeId={nodeId}
                onNodeIdChange={setNodeId}
                onSaveNodeId={handleSaveNodeId}
              />
            ) : (
              <div style={{ padding: '2rem', textAlign: 'center', color: 'var(--text-muted)' }}>
                {content.noDataAvailable || 'No data available'}
              </div>
            ),
            disabled: !nodeId
          },
          {
            id: 'can-mappings',
            label: 'CAN Mappings',
            content: routeParams?.serial && savedNodeId > 0 ? (
              <CanMappingEditor serial={routeParams.serial} nodeId={savedNodeId} />
            ) : (
              <div style={{ padding: '2rem', textAlign: 'center', color: 'var(--text-muted)' }}>
                {content.noDataAvailable || 'No data available'}
              </div>
            ),
            disabled: !savedNodeId || savedNodeId === 0
          },
          {
            id: 'can-messages',
            label: 'CAN Messages',
            content: routeParams?.serial && savedNodeId > 0 ? (
              <>
                <CanIoControl serial={routeParams.serial} nodeId={savedNodeId} />
                <CanMessageSender serial={routeParams.serial} nodeId={savedNodeId} />
              </>
            ) : (
              <div style={{ padding: '2rem', textAlign: 'center', color: 'var(--text-muted)' }}>
                {content.noDataAvailable || 'No data available'}
              </div>
            ),
            disabled: !savedNodeId || savedNodeId === 0
          },
          {
            id: 'ota-update',
            label: 'OTA Update',
            content: <OTAUpdate />
          }
        ]}
        defaultTab="overview"
      />
          </>
        )}
        </div>
      </Layout>
  )
}
