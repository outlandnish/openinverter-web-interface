import { useEffect } from 'preact/hooks'
import { useIntlayer } from 'preact-intlayer'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { useDeviceDetailsContext } from '@contexts/DeviceDetailsContext'
import { useToast } from '@hooks/useToast'
import './styles.css'

interface CanMessageSenderProps {
  serial: string
  nodeId: number
}

export default function CanMessageSender({ serial, nodeId }: CanMessageSenderProps) {
  // Props available for future use (validation, filtering, etc.)
  void serial; void nodeId;

  const content = useIntlayer('can-message-sender')
  const { isConnected, sendMessage, subscribe } = useWebSocketContext()
  const { showError, showSuccess } = useToast()

  // Get state from context
  const {
    canMessages,
    setCanId,
    setDataBytes,
    addPeriodicMessage,
    removePeriodicMessage,
    togglePeriodicMessage,
    setShowAddPeriodicForm,
    setPeriodicFormData,
    resetPeriodicFormData,
  } = useDeviceDetailsContext()

  // Destructure CAN message state for easier access
  const { canId, dataBytes, periodicMessages, showAddPeriodicForm, periodicFormData } = canMessages

  // Subscribe to WebSocket messages
  useEffect(() => {
    const unsubscribe = subscribe((message) => {
      switch (message.event) {
        case 'canMessageSent':
          if (message.data.success) {
            showSuccess(content.messageSentSuccess({ canId: `0x${message.data.canId.toString(16).toUpperCase()}` }))
          } else {
            showError(content.messageSentError({ error: message.data.error || 'Unknown error' }))
          }
          break
      }
    })

    return unsubscribe
  }, [subscribe, content, showSuccess, showError])

  // Parse hex string to number
  const parseHex = (hex: string): number => {
    const cleaned = hex.trim().replace(/^0x/i, '')
    return parseInt(cleaned, 16)
  }

  // Parse data bytes string to array
  const parseDataBytes = (data: string): number[] => {
    const bytes = data
      .trim()
      .split(/\s+/)
      .map(b => {
        const cleaned = b.replace(/^0x/i, '')
        return parseInt(cleaned, 16)
      })
      .filter(b => !isNaN(b))

    // Pad or truncate to 8 bytes
    while (bytes.length < 8) bytes.push(0)
    return bytes.slice(0, 8)
  }

  // Format input to hex byte mask (XX XX XX XX XX XX XX XX)
  const formatHexBytes = (input: string): string => {
    // Remove all non-hex characters
    const cleaned = input.replace(/[^0-9A-Fa-f]/g, '').toUpperCase()

    // Split into pairs and join with spaces
    const pairs: string[] = []
    for (let i = 0; i < cleaned.length && i < 16; i += 2) {
      if (i + 1 < cleaned.length) {
        pairs.push(cleaned.substring(i, i + 2))
      } else {
        pairs.push(cleaned.substring(i, i + 1))
      }
    }

    return pairs.join(' ')
  }

  // Handle masked input change
  const handleDataBytesChange = (value: string, setter: (val: string) => void) => {
    const formatted = formatHexBytes(value)
    setter(formatted)
  }

  // Format number to hex string
  const toHex = (num: number): string => {
    return num.toString(16).toUpperCase().padStart(2, '0')
  }

  // Validate CAN ID
  const validateCanId = (id: string): boolean => {
    const parsed = parseHex(id)
    return !isNaN(parsed) && parsed >= 0 && parsed <= 0x7FF
  }

  // Validate data bytes
  const validateDataBytes = (data: string): boolean => {
    const bytes = parseDataBytes(data)
    return bytes.every(b => b >= 0 && b <= 0xFF)
  }

  // Send one-shot CAN message
  const handleSendOneShot = () => {
    if (!isConnected) {
      showError(content.notConnectedError)
      return
    }

    if (!validateCanId(canId)) {
      showError(content.invalidCanIdError)
      return
    }

    if (!validateDataBytes(dataBytes)) {
      showError(content.invalidDataBytesError)
      return
    }

    const parsedCanId = parseHex(canId)
    const parsedData = parseDataBytes(dataBytes)

    sendMessage('sendCanMessage', {
      canId: parsedCanId,
      data: parsedData
    })
  }

  // Add periodic message
  const handleAddPeriodic = () => {
    if (!validateCanId(periodicFormData.canId)) {
      showError(content.invalidCanIdError)
      return
    }

    if (!validateDataBytes(periodicFormData.data)) {
      showError(content.invalidDataBytesError)
      return
    }

    if (periodicFormData.interval < 10 || periodicFormData.interval > 10000) {
      showError(content.invalidIntervalError)
      return
    }

    const newMessage = {
      id: `msg_${Date.now()}`,
      canId: parseHex(periodicFormData.canId),
      data: parseDataBytes(periodicFormData.data),
      interval: periodicFormData.interval,
      active: false
    }

    addPeriodicMessage(newMessage)
    setShowAddPeriodicForm(false)
    resetPeriodicFormData()
    showSuccess(content.periodicAddedSuccess)
  }

  // Start/stop periodic message
  const handleTogglePeriodic = (message: { id: string; canId: number; data: number[]; interval: number; active: boolean }) => {
    if (!isConnected) {
      showError(content.notConnectedError)
      return
    }

    const newActive = !message.active

    if (newActive) {
      // Start interval - matches backend handleStartCanInterval
      sendMessage('startCanInterval', {
        intervalId: message.id,
        canId: message.canId,
        data: message.data,
        interval: message.interval
      })
    } else {
      // Stop interval - matches backend handleStopCanInterval
      sendMessage('stopCanInterval', {
        intervalId: message.id
      })
    }

    // Optimistically update UI
    togglePeriodicMessage(message.id, newActive)
  }

  // Remove periodic message
  const handleRemovePeriodic = (messageId: string) => {
    const message = periodicMessages.find(m => m.id === messageId)
    if (message?.active) {
      // Stop it first - matches backend handleStopCanInterval
      sendMessage('stopCanInterval', {
        intervalId: messageId
      })
    }
    removePeriodicMessage(messageId)
    showSuccess(content.periodicRemovedSuccess)
  }

  return (
    <section id="can-message-sender" class="card">
      <h2 class="section-header">{content.title}</h2>

      {/* One-Shot Messages */}
      <div class="message-section">
        <h3>{content.oneShotSection}</h3>
        <p class="section-description">{content.oneShotDescription}</p>

        <div class="message-form">
          <div class="form-row">
            <label>
              {content.canIdLabel}
              <input
                type="text"
                placeholder={content.canIdPlaceholder}
                value={canId}
                onChange={(e) => setCanId((e.currentTarget as HTMLInputElement).value)}
                class="input-hex"
              />
            </label>
          </div>

          <div class="form-row">
            <label>
              {content.dataBytesLabel}
              <input
                type="text"
                placeholder={content.dataBytesPlaceholder}
                value={dataBytes}
                onChange={(e) => handleDataBytesChange((e.currentTarget as HTMLInputElement).value, setDataBytes)}
                class="input-data"
                maxLength={23}
              />
            </label>
          </div>

          <div class="form-actions">
            <button
              class="btn-send"
              onClick={handleSendOneShot}
              disabled={!isConnected}
            >
              {content.sendButton}
            </button>
          </div>
        </div>
      </div>

      {/* Periodic Messages */}
      <div class="message-section">
        <h3>{content.periodicSection}</h3>
        <p class="section-description">{content.periodicDescription}</p>

        {periodicMessages.length === 0 ? (
          <p class="no-messages">{content.noPeriodicMessages}</p>
        ) : (
          <table class="messages-table">
            <thead>
              <tr>
                <th>{content.tableHeaderCanId}</th>
                <th>{content.tableHeaderData}</th>
                <th>{content.tableHeaderInterval}</th>
                <th>{content.tableHeaderStatus}</th>
                <th>{content.tableHeaderActions}</th>
              </tr>
            </thead>
            <tbody>
              {periodicMessages.map((message) => (
                <tr key={message.id}>
                  <td>0x{message.canId.toString(16).toUpperCase().padStart(3, '0')}</td>
                  <td class="data-cell">{message.data.map(toHex).join(' ')}</td>
                  <td>{message.interval}</td>
                  <td>
                    <span class={`status-badge ${message.active ? 'status-active' : 'status-inactive'}`}>
                      {message.active ? content.statusActive : content.statusStopped}
                    </span>
                  </td>
                  <td>
                    <div class="action-buttons">
                      <button
                        class={message.active ? 'btn-stop' : 'btn-start'}
                        onClick={() => handleTogglePeriodic(message)}
                        disabled={!isConnected}
                      >
                        {message.active ? content.stopButton : content.startButton}
                      </button>
                      <button
                        class="btn-remove"
                        onClick={() => handleRemovePeriodic(message.id)}
                        disabled={message.active}
                      >
                        {content.removeButton}
                      </button>
                    </div>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        )}

        {/* Add Periodic Message Form */}
        <div class="add-message-section">
          {!showAddPeriodicForm ? (
            <button class="btn-add" onClick={() => setShowAddPeriodicForm(true)}>
              {content.addPeriodicButton}
            </button>
          ) : (
            <div class="add-message-form">
              <h4>{content.addPeriodicFormTitle}</h4>

              <div class="form-row">
                <label>
                  {content.canIdLabel}
                  <input
                    type="text"
                    placeholder={content.canIdPlaceholder}
                    value={periodicFormData.canId}
                    onChange={(e) => setPeriodicFormData({
                      ...periodicFormData,
                      canId: (e.currentTarget as HTMLInputElement).value
                    })}
                    class="input-hex"
                  />
                </label>
              </div>

              <div class="form-row">
                <label>
                  {content.dataBytesLabel}
                  <input
                    type="text"
                    placeholder={content.dataBytesPlaceholder}
                    value={periodicFormData.data}
                    onChange={(e) => handleDataBytesChange(
                      (e.currentTarget as HTMLInputElement).value,
                      (val) => setPeriodicFormData({ ...periodicFormData, data: val })
                    )}
                    class="input-data"
                    maxLength={23}
                  />
                </label>
              </div>

              <div class="form-row">
                <label>
                  {content.intervalLabel}
                  <input
                    type="number"
                    min="10"
                    max="10000"
                    step="10"
                    value={periodicFormData.interval}
                    onChange={(e) => setPeriodicFormData({
                      ...periodicFormData,
                      interval: parseInt((e.currentTarget as HTMLInputElement).value) || 100
                    })}
                  />
                </label>
              </div>

              <div class="form-actions">
                <button class="btn-cancel" onClick={() => setShowAddPeriodicForm(false)}>
                  {content.cancelButton}
                </button>
                <button class="btn-save" onClick={handleAddPeriodic}>
                  {content.addMessageButton}
                </button>
              </div>
            </div>
          )}
        </div>
      </div>
    </section>
  )
}
