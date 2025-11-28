import { useState, useEffect } from 'preact/hooks'
import { useLocation } from 'wouter'
import { api, DeviceSettings } from '../api/inverter'
import Layout from '../components/Layout'
import DisconnectedState from '../components/DisconnectedState'
import { useWebSocket } from '../hooks/useWebSocket'

const CAN_SPEEDS = [
  { value: 0, label: '125 kbit/s' },
  { value: 1, label: '250 kbit/s' },
  { value: 2, label: '500 kbit/s' },
]

export default function Settings() {
  const [, setLocation] = useLocation()
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
      alert('Settings saved successfully! Device will reinitialize CAN bus.')
    } catch (error) {
      console.error('Failed to save settings:', error)
      alert('Failed to save settings')
    } finally {
      setSaving(false)
    }
  }

  if (loading) {
    return (
      <Layout>
        <div class="container">
          <div class="loading">Loading settings...</div>
        </div>
      </Layout>
    )
  }

  return (
    <Layout>
      <div class="container">
        <div class="page-header">
          <h1 class="page-title">Global Settings</h1>
          <p class="page-subtitle">Configure CAN bus settings for all devices</p>
        </div>

        {!isConnected ? (
          <DisconnectedState onReconnect={reconnect} />
        ) : (
          <section class="card">
        <h2>CAN Bus Configuration</h2>
        <p class="info">These settings apply to all devices on the CAN bus.</p>

        <div class="form-group">
          <label>CAN Speed</label>
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
            Select the CAN bus speed. This must match the speed configured on your devices.
            Common speeds: 500k (default), 250k, 125k
          </small>
        </div>

        <div class="form-group">
          <label>CAN RX Pin</label>
          <input
            type="number"
            value={settings.canRXPin}
            onInput={(e) => setSettings({ ...settings, canRXPin: parseInt((e.target as HTMLInputElement).value) })}
            placeholder="GPIO Pin"
          />
          <small class="hint">GPIO pin for CAN receive (default: 4)</small>
        </div>

        <div class="form-group">
          <label>CAN TX Pin</label>
          <input
            type="number"
            value={settings.canTXPin}
            onInput={(e) => setSettings({ ...settings, canTXPin: parseInt((e.target as HTMLInputElement).value) })}
            placeholder="GPIO Pin"
          />
          <small class="hint">GPIO pin for CAN transmit (default: 5)</small>
        </div>

        <div class="form-group">
          <label>CAN Enable Pin (Optional)</label>
          <input
            type="number"
            value={settings.canEnablePin}
            onInput={(e) => setSettings({ ...settings, canEnablePin: parseInt((e.target as HTMLInputElement).value) })}
            placeholder="GPIO Pin (0 = disabled)"
          />
          <small class="hint">GPIO pin to enable CAN transceiver (set to 0 if not used)</small>
        </div>

        <button class="btn-primary" onClick={handleSave} disabled={saving || !isConnected}>
          {saving ? 'Saving...' : 'Save Settings'}
        </button>
          </section>
        )}
      </div>
    </Layout>
  )
}
