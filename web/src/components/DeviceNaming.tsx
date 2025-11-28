import { useState } from 'preact/hooks'

interface DeviceNamingProps {
  serial: string
  onSave: (name: string) => void
  onCancel: () => void
}

export default function DeviceNaming({ serial, onSave, onCancel }: DeviceNamingProps) {
  const [name, setName] = useState('')

  const handleSubmit = (e: Event) => {
    e.preventDefault()
    if (name.trim()) {
      onSave(name.trim())
    }
  }

  return (
    <div class="modal-overlay" onClick={onCancel}>
      <div class="modal-content" onClick={(e) => e.stopPropagation()}>
        <h2>Name Your Device</h2>

        <div class="modal-body">
          <p class="device-serial-info">Serial: {serial}</p>

          <form onSubmit={handleSubmit}>
            <div class="form-group">
              <label>Device Name</label>
              <input
                type="text"
                value={name}
                onInput={(e) => setName((e.target as HTMLInputElement).value)}
                placeholder="e.g., Main Inverter"
                autoFocus
                required
              />
            </div>

            <div class="modal-actions">
              <button type="button" class="btn-secondary" onClick={onCancel}>
                Cancel
              </button>
              <button type="submit" class="btn-primary" disabled={!name.trim()}>
                Save & Connect
              </button>
            </div>
          </form>
        </div>
      </div>
    </div>
  )
}
