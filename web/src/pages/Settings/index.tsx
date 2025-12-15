import { useState, useEffect } from 'preact/hooks'
import { useLocation } from 'wouter'
import { useIntlayer } from 'preact-intlayer'
import { api, DeviceSettings } from '@api/inverter'
import Layout from '@components/Layout'
import DisconnectedState from '@components/DisconnectedState'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { useToast } from '@hooks/useToast'

const CAN_SPEEDS = [
  { value: 0, label: '125 kbit/s' },
  { value: 1, label: '250 kbit/s' },
  { value: 2, label: '500 kbit/s' },
]

export default function Settings() {
  const [, setLocation] = useLocation()
  const content = useIntlayer('settings')
  const [loading, setLoading] = useState(true)
  const [saving, setSaving] = useState(false)
  const [settings, setSettings] = useState<DeviceSettings>({
    canRXPin: 4,
    canTXPin: 5,
    canEnablePin: 0,
    canSpeed: 2,
    scanStartNode: 1,
    scanEndNode: 32,
  })

  // WebSocket connection
  const { isConnected, isConnecting, isRetrying } = useWebSocketContext()

  const handleReconnect = () => {
    window.location.reload()
  }

  // Toast notifications
  const { showSuccess, showError } = useToast()

  useEffect(() => {
    loadSettings()
  }, [])

  const loadSettings = async () => {
    try {
      setLoading(true)
      const loadedSettings = await api.getSettings()
      setSettings(loadedSettings)
    } catch (error) {
      console.error('Failed to load settings:', error)
      // Keep default values if loading fails
    } finally {
      setLoading(false)
    }
  }

  const handleSave = async () => {
    try {
      setSaving(true)
      await api.updateSettings(settings)
      showSuccess(content.savedSuccessfully.value)
    } catch (error) {
      console.error('Failed to save settings:', error)
      showError(content.saveFailed.value)
    } finally {
      setSaving(false)
    }
  }

  if (loading) {
    return (
      <Layout>
        <div class="container">
          <div class="loading">{content.loadingSettings}</div>
        </div>
      </Layout>
    )
  }

  return (
    <Layout>
      <div class="container">
        <div class="page-header">
          <h1 class="page-title">{content.title}</h1>
          <p class="page-subtitle">{content.subtitle}</p>
        </div>

        {!isConnected ? (
          <DisconnectedState onReconnect={handleReconnect} isConnecting={isConnecting} isRetrying={isRetrying} />
        ) : (
          <>
            <section class="card">
              <h2>{content.canBusConfiguration}</h2>
              <p class="info">{content.canBusInfo}</p>

              <div class="form-group">
                <label>{content.canSpeed}</label>
                <select
                  value={settings.canSpeed}
                  onChange={(e) => setSettings({ ...settings, canSpeed: parseInt((e.target as HTMLSelectElement).value) })}
                >
                  {CAN_SPEEDS.map(speed => (
                    <option key={speed.value} value={speed.value}>
                      {speed.label}
                    </option>
                  ))}
                </select>
                <small class="hint">
                  {content.canSpeedHint}
                </small>
              </div>

              <div class="form-group">
                <label>{content.canRxPin}</label>
                <input
                  type="number"
                  value={settings.canRXPin}
                  onInput={(e) => setSettings({ ...settings, canRXPin: parseInt((e.target as HTMLInputElement).value) })}
                  placeholder={content.gpioPin}
                />
                <small class="hint">{content.canRxPinHint}</small>
              </div>

              <div class="form-group">
                <label>{content.canTxPin}</label>
                <input
                  type="number"
                  value={settings.canTXPin}
                  onInput={(e) => setSettings({ ...settings, canTXPin: parseInt((e.target as HTMLInputElement).value) })}
                  placeholder={content.gpioPin}
                />
                <small class="hint">{content.canTxPinHint}</small>
              </div>

              <div class="form-group">
                <label>{content.canEnablePin}</label>
                <input
                  type="number"
                  value={settings.canEnablePin}
                  onInput={(e) => setSettings({ ...settings, canEnablePin: parseInt((e.target as HTMLInputElement).value) })}
                  placeholder={content.gpioPin}
                />
                <small class="hint">{content.canEnablePinHint}</small>
              </div>
            </section>

            <section class="card">
              <h2>{content.scanConfiguration}</h2>
              <p class="info">{content.scanConfigurationInfo}</p>

              <div class="form-group">
                <label>{content.scanStartNode}</label>
                <input
                  type="number"
                  min="0"
                  max="255"
                  value={settings.scanStartNode}
                  onInput={(e) => setSettings({ ...settings, scanStartNode: parseInt((e.target as HTMLInputElement).value) || 1 })}
                  placeholder="1"
                />
                <small class="hint">{content.scanStartNodeHint.value}</small>
              </div>

              <div class="form-group">
                <label>{content.scanEndNode}</label>
                <input
                  type="number"
                  min="0"
                  max="255"
                  value={settings.scanEndNode}
                  onInput={(e) => setSettings({ ...settings, scanEndNode: parseInt((e.target as HTMLInputElement).value) || 32 })}
                  placeholder="32"
                />
                <small class="hint">{content.scanEndNodeHint.value}</small>
              </div>
            </section>

            <button class="btn-primary" onClick={handleSave} disabled={saving || !isConnected}>
              {saving ? content.saving : content.saveSettings}
            </button>
          </>
        )}
      </div>
    </Layout>
  )
}
