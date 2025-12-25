import { useState, useEffect } from 'preact/hooks'
import { useLocation, useRoute } from 'wouter'
import { useIntlayer } from 'preact-intlayer'
import { useParams } from '@hooks/useParams'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import Layout from '@components/Layout'
import ConnectionStatus from '@components/ConnectionStatus'
import SpotValuesMonitor from '@components/SpotValuesMonitor'
import OTAUpdate from '@components/OTAUpdate'
import DeviceParameters from '@components/DeviceParameters'
import CanMappingEditor from '@components/CanMappingEditor'
import CanMessageSender from '@components/CanMessageSender'
import Tabs from '@components/Tabs'
import { useToast } from '@hooks/useToast'
import { formatParameterValue } from '@/utils/parameterDisplay'
import { api } from '@/api/inverter'

export default function DeviceDetails() {
  const [, setLocation] = useLocation()
  const [, routeParams] = useRoute('/devices/:serial')
  const content = useIntlayer('device-details')
  const { showError, showSuccess, showWarning } = useToast()

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

  // Disconnect from device when component unmounts (navigating away)
  useEffect(() => {
    return () => {
      // Send disconnect message when leaving the page
      sendMessage('disconnect')
      console.log('[DeviceDetails] Sent disconnect message on unmount')
    }
  }, [sendMessage])

  // Subscribe to WebSocket messages and load settings when ready
  useEffect(() => {
    const unsubscribe = subscribe((message: any) => {
      console.log('WebSocket event:', message.event, message.data)

      switch (message.event) {
        case 'connected':
          if (message.data.serial === routeParams?.serial) {
            setDeviceConnected(true)
          }
          break

        case 'disconnected':
          setDeviceConnected(false)
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

  return (
    <Layout currentSerial={routeParams?.serial}>
      <div class="container">
        {loading ? (
          <div class="loading-container" style={{
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            justifyContent: 'center',
            minHeight: '400px',
            gap: '1rem'
          }}>
            <div class="spinner" style={{
              border: '4px solid rgba(0, 0, 0, 0.1)',
              borderTop: '4px solid #007bff',
              borderRadius: '50%',
              width: '48px',
              height: '48px',
              animation: 'spin 1s linear infinite'
            }} />
            <div class="loading-text" style={{ fontSize: '1.2rem', color: '#666' }}>
              {content.connectingToDevice}
            </div>
          </div>
        ) : (
          <>
            <div class="page-header">
              <div class="page-title-row">
                <h1 class="page-title">{deviceName || routeParams?.serial || content.deviceMonitor}</h1>
                <ConnectionStatus
                  connected={deviceConnected}
                  label={deviceConnected ? content.connected : content.disconnected}
                />
              </div>
              <div class="device-info-inline">
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
              <CanMessageSender serial={routeParams.serial} nodeId={savedNodeId} />
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
