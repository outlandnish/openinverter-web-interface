import { useIntlayer } from 'preact-intlayer'
import { useEffect, useState } from 'preact/hooks'
import { api } from '@api/inverter'
import { useToast } from '@hooks/useToast'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { normalizeEnumValue } from '@utils/parameterDisplay'

interface ParameterInputProps {
  paramKey: string
  param: any
  displayName: string
  isConnected: boolean
  onUpdate: (paramId: number, newValue: number) => void
}

export default function ParameterInput({
  paramKey,
  param,
  displayName,
  isConnected,
  onUpdate
}: ParameterInputProps) {
  const content = useIntlayer('device-details')
  const { showError, showSuccess } = useToast()
  const { subscribe } = useWebSocketContext()
  const [pendingUpdate, setPendingUpdate] = useState<number | null>(null)

  const paramId = param.id
  const hasEnum = param.enums && Object.keys(param.enums).length > 0

  // Subscribe to WebSocket events for parameter update responses
  useEffect(() => {
    const unsubscribe = subscribe((message) => {
      if (message.event === 'paramUpdateSuccess') {
        if (message.data.paramId === paramId && pendingUpdate !== null) {
          showSuccess(content.parameterUpdatedSuccess({ paramName: displayName }))
          // Update local parameter value without full reload
          onUpdate(paramId, pendingUpdate)
          setPendingUpdate(null)
        }
      } else if (message.event === 'paramUpdateError') {
        if (message.data.paramId === paramId && pendingUpdate !== null) {
          showError(content.failedToUpdateParam({ paramName: displayName }) + ': ' + message.data.error)
          setPendingUpdate(null)
        }
      }
    })

    return unsubscribe
  }, [subscribe, paramId, pendingUpdate, displayName, showSuccess, showError, onUpdate])

  const handleEnumChange = async (e: Event) => {
    const newValue = parseFloat((e.target as HTMLSelectElement).value)
    try {
      setPendingUpdate(newValue)
      await api.setParamById(paramId, newValue)
      // Response will be handled by WebSocket event listener
    } catch (error) {
      showError(content.failedToUpdateParam({ paramName: displayName }))
      setPendingUpdate(null)
    }
  }

  const handleNumberBlur = async (e: Event) => {
    const target = e.target as HTMLInputElement
    const newValue = parseFloat(target.value)

    if (isNaN(newValue)) return

    // Validate range
    if (param.minimum !== undefined && newValue < param.minimum) {
      showError(content.valueMustBeAtLeast({ min: param.minimum }))
      target.value = param.value.toString()
      return
    }
    if (param.maximum !== undefined && newValue > param.maximum) {
      showError(content.valueMustBeAtMost({ max: param.maximum }))
      target.value = param.value.toString()
      return
    }

    if (newValue !== param.value) {
      try {
        setPendingUpdate(newValue)
        await api.setParamById(paramId, newValue)
        // Response will be handled by WebSocket event listener
      } catch (error) {
        showError(content.failedToUpdateParam({ paramName: displayName }))
        target.value = param.value.toString()
        setPendingUpdate(null)
      }
    }
  }

  return (
    <div key={paramKey} class="parameter-item">
      <div class="parameter-header">
        <label class="parameter-label">
          {displayName}
          {param.unit && <span class="parameter-unit"> ({param.unit})</span>}
        </label>
      </div>

      <div class="parameter-input-group">
        {hasEnum ? (
          <select
            value={normalizeEnumValue(param.value)}
            onChange={handleEnumChange}
            disabled={!isConnected}
          >
            {Object.entries(param.enums).map(([value, label]) => (
              <option key={value} value={value}>
                {String(label)}
              </option>
            ))}
          </select>
        ) : (
          <input
            type="number"
            value={param.value}
            min={param.minimum}
            max={param.maximum}
            step={param.unit === 'Hz' || param.unit === 'A' ? '0.1' : '1'}
            onBlur={handleNumberBlur}
            disabled={!isConnected}
          />
        )}

        {param.minimum !== undefined && param.maximum !== undefined && !hasEnum && (
          <small class="parameter-hint">
            {content.range} {param.minimum} - {param.maximum}
            {param.default !== undefined && <> ({content.default} {param.default})</>}
          </small>
        )}
      </div>
    </div>
  )
}
