import { useState } from 'preact/hooks'
import { useIntlayer } from 'react-intlayer'

interface DeviceNamingProps {
  serial: string
  onSave: (name: string) => void
  onCancel: () => void
}

export default function DeviceNaming({ serial, onSave, onCancel }: DeviceNamingProps) {
  const content = useIntlayer('device-naming')
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
        <h2>{content.title}</h2>

        <div class="modal-body">
          <p class="device-serial-info">{content.serial} {serial}</p>

          <form onSubmit={handleSubmit}>
            <div class="form-group">
              <label>{content.deviceName}</label>
              <input
                type="text"
                value={name}
                onInput={(e) => setName((e.target as HTMLInputElement).value)}
                placeholder={content.placeholder}
                autoFocus
                required
              />
            </div>

            <div class="modal-actions">
              <button type="button" class="btn-secondary" onClick={onCancel}>
                {content.cancel}
              </button>
              <button type="submit" class="btn-primary" disabled={!name.trim()}>
                {content.saveAndConnect}
              </button>
            </div>
          </form>
        </div>
      </div>
    </div>
  )
}
