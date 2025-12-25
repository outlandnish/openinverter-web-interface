import { useEffect, useState } from 'preact/hooks'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { useToast } from '@hooks/useToast'
import './styles.css'

interface CanIoControlProps {
  serial: string
  nodeId: number
}

// CAN IO bit flags
const CAN_IO_CRUISE = 0x01
const CAN_IO_START = 0x02
const CAN_IO_BRAKE = 0x04
const CAN_IO_FWD = 0x08
const CAN_IO_REV = 0x10
const CAN_IO_BMS = 0x20

export default function CanIoControl({ serial, nodeId }: CanIoControlProps) {
  // Props available for future use
  void serial; void nodeId;

  const { isConnected, sendMessage, subscribe } = useWebSocketContext()
  const { showError, showSuccess } = useToast()

  // State
  const [active, setActive] = useState(false)
  const [canId, setCanId] = useState('3F')
  const [interval, setInterval] = useState(100)

  // CAN IO flags
  const [cruise, setCruise] = useState(false)
  const [start, setStart] = useState(false)
  const [brake, setBrake] = useState(false)
  const [forward, setForward] = useState(false)
  const [reverse, setReverse] = useState(false)
  const [bms, setBms] = useState(false)

  // Throttle and other parameters
  const [throttlePercent, setThrottlePercent] = useState(0)
  const [cruisespeed, setCruisespeed] = useState(0)
  const [regenpreset, setRegenpreset] = useState(0)

  // Subscribe to WebSocket messages
  useEffect(() => {
    const unsubscribe = subscribe((message) => {
      switch (message.event) {
        case 'canIoIntervalStatus':
          setActive(message.data.active)
          if (message.data.active) {
            showSuccess(`CAN IO interval started (${message.data.intervalMs}ms)`)
          } else {
            showSuccess('CAN IO interval stopped')
          }
          break
      }
    })

    return unsubscribe
  }, [subscribe])

  const handleStart = () => {
    if (!isConnected) {
      showError('Not connected to device')
      return
    }

    // Validate CAN ID
    const parsedCanId = parseInt(canId, 16)
    if (isNaN(parsedCanId) || parsedCanId < 0 || parsedCanId > 0x7FF) {
      showError('Invalid CAN ID (must be 0x000 to 0x7FF)')
      return
    }

    // Calculate canio flags
    let canio = 0
    if (cruise) canio |= CAN_IO_CRUISE
    if (start) canio |= CAN_IO_START
    if (brake) canio |= CAN_IO_BRAKE
    if (forward) canio |= CAN_IO_FWD
    if (reverse) canio |= CAN_IO_REV
    if (bms) canio |= CAN_IO_BMS

    // Scale throttle percent (0-100) to pot values (0-4095)
    const pot = Math.round((throttlePercent / 100) * 4095)

    sendMessage('startCanIoInterval', {
      canId: parsedCanId,
      pot,
      pot2: pot, // pot2 same as pot
      canio,
      cruisespeed,
      regenpreset,
      interval
    })
  }

  const handleStop = () => {
    if (!isConnected) {
      showError('Not connected to device')
      return
    }

    sendMessage('stopCanIoInterval', {})
  }

  const handleToggle = () => {
    if (active) {
      handleStop()
    } else {
      handleStart()
    }
  }

  return (
    <div class="can-io-control">
      <h3>CAN IO Control (Inverter)</h3>

      <div class="can-io-section">
        <h4>Configuration</h4>
        <div class="can-io-row">
          <label>CAN ID (hex):</label>
          <input
            type="text"
            value={canId}
            onInput={(e) => setCanId((e.target as HTMLInputElement).value.toUpperCase())}
            disabled={active}
            placeholder="3F"
            maxLength={3}
          />
        </div>
        <div class="can-io-row">
          <label>Interval (ms):</label>
          <input
            type="number"
            value={interval}
            onInput={(e) => setInterval(parseInt((e.target as HTMLInputElement).value) || 100)}
            disabled={active}
            min={10}
            max={500}
          />
          <span class="hint">10-500ms (recommended: 50-100ms)</span>
        </div>
      </div>

      <div class="can-io-section">
        <h4>Control Flags</h4>
        <div class="can-io-flags">
          <label class="can-io-checkbox">
            <input
              type="checkbox"
              checked={cruise}
              onChange={(e) => setCruise((e.target as HTMLInputElement).checked)}
              disabled={!active}
            />
            <span>Cruise (0x01)</span>
          </label>
          <label class="can-io-checkbox">
            <input
              type="checkbox"
              checked={start}
              onChange={(e) => setStart((e.target as HTMLInputElement).checked)}
              disabled={!active}
            />
            <span>Start (0x02)</span>
          </label>
          <label class="can-io-checkbox">
            <input
              type="checkbox"
              checked={brake}
              onChange={(e) => setBrake((e.target as HTMLInputElement).checked)}
              disabled={!active}
            />
            <span>Brake (0x04)</span>
          </label>
          <label class="can-io-checkbox">
            <input
              type="checkbox"
              checked={forward}
              onChange={(e) => setForward((e.target as HTMLInputElement).checked)}
              disabled={!active}
            />
            <span>Forward (0x08)</span>
          </label>
          <label class="can-io-checkbox">
            <input
              type="checkbox"
              checked={reverse}
              onChange={(e) => setReverse((e.target as HTMLInputElement).checked)}
              disabled={!active}
            />
            <span>Reverse (0x10)</span>
          </label>
          <label class="can-io-checkbox">
            <input
              type="checkbox"
              checked={bms}
              onChange={(e) => setBms((e.target as HTMLInputElement).checked)}
              disabled={!active}
            />
            <span>BMS (0x20)</span>
          </label>
        </div>
      </div>

      <div class="can-io-section">
        <h4>Throttle & Parameters</h4>
        <div class="can-io-row">
          <label>Throttle (%):</label>
          <input
            type="range"
            value={throttlePercent}
            onInput={(e) => setThrottlePercent(parseInt((e.target as HTMLInputElement).value))}
            disabled={!active}
            min={0}
            max={100}
          />
          <span class="value">{throttlePercent}%</span>
        </div>
        <div class="can-io-row">
          <label>Cruise Speed:</label>
          <input
            type="number"
            value={cruisespeed}
            onInput={(e) => setCruisespeed(parseInt((e.target as HTMLInputElement).value) || 0)}
            disabled={!active}
            min={0}
            max={16383}
          />
          <span class="hint">0-16383</span>
        </div>
        <div class="can-io-row">
          <label>Regen Preset:</label>
          <input
            type="number"
            value={regenpreset}
            onInput={(e) => setRegenpreset(parseInt((e.target as HTMLInputElement).value) || 0)}
            disabled={!active}
            min={0}
            max={255}
          />
          <span class="hint">0-255</span>
        </div>
      </div>

      <div class="can-io-actions">
        <button
          onClick={handleToggle}
          disabled={!isConnected}
          class={active ? 'stop-btn' : 'start-btn'}
        >
          {active ? 'Stop CAN IO' : 'Start CAN IO'}
        </button>
        {active && (
          <div class="status-indicator active">
            <span class="pulse"></span>
            Active (sending every {interval}ms)
          </div>
        )}
      </div>
    </div>
  )
}
