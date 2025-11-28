import { useState, useEffect } from 'preact/hooks'
import { useLocation } from 'wouter'
import { useIntlayer } from 'preact-intlayer'
import { api, DeviceSettings } from '@api/inverter'
import Layout from '@components/Layout'
import DisconnectedState from '@components/DisconnectedState'
import { useWebSocket } from '@hooks/useWebSocket'

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
  })

  // WebSocket connection
  const { isConnected, reconnect } = useWebSocket('/ws')

  useEffect(() => {
    loadSettings()
  }, [])

  const loadSettings = async () => {
    try {
      setLoading(true)
      // Settings are loaded with defaults for now
      // In the future, we can fetch from device if needed
    } catch (error) {
      console.error('Failed to load settings:', error)
    } finally {
      setLoading(false)
    }
  }

  const handleSave = async () => {
    try {
      setSaving(true)
      await api.updateSettings(settings)
      alert(content.savedSuccessfully)
    } catch (error) {
      console.error('Failed to save settings:', error)
      alert(content.saveFailed)
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
          <DisconnectedState onReconnect={reconnect} />
        ) : (
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

        <button class="btn-primary" onClick={handleSave} disabled={saving || !isConnected}>
          {saving ? content.saving : content.saveSettings}
        </button>
          </section>
        )}
      </div>
    </Layout>
  )
}
