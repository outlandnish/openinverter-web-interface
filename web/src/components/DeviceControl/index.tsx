import { useEffect, useState } from 'preact/hooks'
import { useIntlayer } from 'preact-intlayer'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { LoadingSpinner } from '@components/LoadingSpinner'
import './styles.css'

interface DeviceControlProps {
  serial: string
  nodeId: number
}

interface ErrorEntry {
  errorNum: number
  errorTime: number
  description?: string
}

export default function DeviceControl({ serial, nodeId }: DeviceControlProps) {
  const content = useIntlayer('device-control')
  const { isConnected, sendMessage, subscribe } = useWebSocketContext()

  const [errors, setErrors] = useState<ErrorEntry[]>([])
  const [loadingErrors, setLoadingErrors] = useState(false)
  const [loadingAction, setLoadingAction] = useState<string | null>(null)

  // Subscribe to WebSocket events
  useEffect(() => {
    const unsubscribe = subscribe((message) => {
      console.log('DeviceControl WebSocket event:', message.event, message.data)

      switch (message.event) {
        case 'listErrorsSuccess':
          setErrors(message.data.errors || [])
          setLoadingErrors(false)
          break

        case 'listErrorsError':
          console.error('List errors error:', message.data.error)
          setLoadingErrors(false)
          break

        case 'startDeviceSuccess':
        case 'stopDeviceSuccess':
        case 'loadFromFlashSuccess':
        case 'loadDefaultsSuccess':
        case 'resetDeviceSuccess':
          setLoadingAction(null)
          // Reload errors after device state change
          setTimeout(() => loadErrors(), 500)
          break

        case 'startDeviceError':
        case 'stopDeviceError':
        case 'loadFromFlashError':
        case 'loadDefaultsError':
        case 'resetDeviceError':
          console.error('Device action error:', message.data.error)
          setLoadingAction(null)
          break
      }
    })

    return unsubscribe
  }, [subscribe])

  // Load errors on mount and when device changes
  useEffect(() => {
    if (isConnected && nodeId) {
      loadErrors()
    }
  }, [isConnected, nodeId])

  const loadErrors = () => {
    if (!isConnected) return

    setLoadingErrors(true)
    sendMessage('listErrors', {})
  }

  const handleStartDevice = () => {
    if (!isConnected) return

    setLoadingAction('start')
    sendMessage('startDevice', {})
  }

  const handleStopDevice = () => {
    if (!isConnected) return

    setLoadingAction('stop')
    sendMessage('stopDevice', {})
  }

  const handleLoadFromFlash = () => {
    if (!isConnected) return

    setLoadingAction('loadFromFlash')
    sendMessage('loadFromFlash', {})
  }

  const handleLoadDefaults = () => {
    if (!isConnected) return

    setLoadingAction('loadDefaults')
    sendMessage('loadDefaults', {})
  }

  const handleReset = () => {
    if (!isConnected) return

    setLoadingAction('reset')
    sendMessage('resetDevice', {})
  }

  const formatTimestamp = (timestamp: number): string => {
    if (timestamp === 0) return content.noTimestamp || 'N/A'

    // Convert to seconds, minutes, hours as appropriate
    if (timestamp < 60) {
      return `${timestamp}s`
    } else if (timestamp < 3600) {
      const minutes = Math.floor(timestamp / 60)
      const seconds = timestamp % 60
      return `${minutes}m ${seconds}s`
    } else {
      const hours = Math.floor(timestamp / 3600)
      const minutes = Math.floor((timestamp % 3600) / 60)
      return `${hours}h ${minutes}m`
    }
  }

  return (
    <section id="device-control" class="card">
      <div class="device-control-header">
        <h2>{content.title}</h2>
        <button
          class="btn btn-secondary btn-sm"
          onClick={loadErrors}
          disabled={!isConnected || loadingErrors}
        >
          {loadingErrors ? <LoadingSpinner size="small" /> : content.refreshErrors}
        </button>
      </div>

      {/* Error Display */}
      <div class="errors-section">
        <h3>{content.currentErrors}</h3>
        {loadingErrors ? (
          <div class="loading-container">
            <LoadingSpinner />
          </div>
        ) : errors.length > 0 ? (
          <div class="errors-list">
            {errors.map((error, index) => (
              <div key={index} class="error-item">
                <div class="error-number">
                  {content.errorCode}: {error.errorNum}
                </div>
                {error.description && (
                  <div class="error-description">{error.description}</div>
                )}
                <div class="error-time">
                  {content.errorTime}: {formatTimestamp(error.errorTime)}
                </div>
              </div>
            ))}
          </div>
        ) : (
          <div class="no-errors">{content.noErrors}</div>
        )}
      </div>

      {/* Action Buttons */}
      <div class="actions-section">
        <h3>{content.deviceActions}</h3>
        <div class="action-buttons">
          <button
            class="btn btn-primary"
            onClick={handleStartDevice}
            disabled={!isConnected || loadingAction === 'start'}
          >
            {loadingAction === 'start' ? <LoadingSpinner size="small" /> : content.startDevice}
          </button>

          <button
            class="btn btn-secondary"
            onClick={handleStopDevice}
            disabled={!isConnected || loadingAction === 'stop'}
          >
            {loadingAction === 'stop' ? <LoadingSpinner size="small" /> : content.stopDevice}
          </button>

          <button
            class="btn btn-secondary"
            onClick={handleLoadFromFlash}
            disabled={!isConnected || loadingAction === 'loadFromFlash'}
          >
            {loadingAction === 'loadFromFlash' ? <LoadingSpinner size="small" /> : content.loadFromFlash}
          </button>

          <button
            class="btn btn-warning"
            onClick={handleLoadDefaults}
            disabled={!isConnected || loadingAction === 'loadDefaults'}
          >
            {loadingAction === 'loadDefaults' ? <LoadingSpinner size="small" /> : content.loadDefaults}
          </button>

          <button
            class="btn btn-danger"
            onClick={handleReset}
            disabled={!isConnected || loadingAction === 'reset'}
          >
            {loadingAction === 'reset' ? <LoadingSpinner size="small" /> : content.resetDevice}
          </button>
        </div>
      </div>
    </section>
  )
}
