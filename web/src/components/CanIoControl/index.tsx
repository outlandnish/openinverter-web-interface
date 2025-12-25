import { useEffect } from 'preact/hooks'
import { useIntlayer } from 'preact-intlayer'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { useDeviceDetailsContext } from '@contexts/DeviceDetailsContext'
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

  const content = useIntlayer('can-io-control')
  const { isConnected, sendMessage, subscribe } = useWebSocketContext()
  const { showError, showSuccess } = useToast()

  // Get state from context
  const {
    canIo,
    setCanIoActive,
    setCanIoCanId,
    setCanIoInterval,
    setCanIoCruise,
    setCanIoStart,
    setCanIoBrake,
    setCanIoForward,
    setCanIoReverse,
    setCanIoBms,
    setCanIoThrottlePercent,
    setCanIoCruisespeed,
    setCanIoRegenpreset,
    setCanIoUseCrc,
  } = useDeviceDetailsContext()

  // Destructure canIo state for easier access
  const {
    active,
    canId,
    interval,
    cruise,
    start,
    brake,
    forward,
    reverse,
    bms,
    throttlePercent,
    cruisespeed,
    regenpreset,
    useCrc,
  } = canIo

  // Subscribe to WebSocket messages
  useEffect(() => {
    const unsubscribe = subscribe((message) => {
      switch (message.event) {
        case 'canIoIntervalStatus':
          setCanIoActive(message.data.active)
          if (message.data.active) {
            showSuccess(content.intervalStartedSuccess({ intervalMs: message.data.intervalMs }))
          } else {
            showSuccess(content.intervalStoppedSuccess)
          }
          break
      }
    })

    return unsubscribe
  }, [subscribe, setCanIoActive, showSuccess, content])

  const handleStart = () => {
    if (!isConnected) {
      showError(content.notConnectedError)
      return
    }

    // Validate CAN ID
    const parsedCanId = parseInt(canId, 16)
    if (isNaN(parsedCanId) || parsedCanId < 0 || parsedCanId > 0x7FF) {
      showError(content.invalidCanIdError)
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
      interval,
      useCrc
    })
  }

  const handleStop = () => {
    if (!isConnected) {
      showError(content.notConnectedError)
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

  const handleUpdateFlags = (overrides?: {
    cruise?: boolean,
    start?: boolean,
    brake?: boolean,
    forward?: boolean,
    reverse?: boolean,
    bms?: boolean,
    throttlePercent?: number,
    cruisespeed?: number,
    regenpreset?: number
  }) => {
    if (!isConnected || !active) return

    // Use override values or current state
    const currentCruise = overrides?.cruise !== undefined ? overrides.cruise : cruise
    const currentStart = overrides?.start !== undefined ? overrides.start : start
    const currentBrake = overrides?.brake !== undefined ? overrides.brake : brake
    const currentForward = overrides?.forward !== undefined ? overrides.forward : forward
    const currentReverse = overrides?.reverse !== undefined ? overrides.reverse : reverse
    const currentBms = overrides?.bms !== undefined ? overrides.bms : bms
    const currentThrottle = overrides?.throttlePercent !== undefined ? overrides.throttlePercent : throttlePercent
    const currentCruisespeed = overrides?.cruisespeed !== undefined ? overrides.cruisespeed : cruisespeed
    const currentRegenpreset = overrides?.regenpreset !== undefined ? overrides.regenpreset : regenpreset

    // Calculate canio flags
    let canio = 0
    if (currentCruise) canio |= CAN_IO_CRUISE
    if (currentStart) canio |= CAN_IO_START
    if (currentBrake) canio |= CAN_IO_BRAKE
    if (currentForward) canio |= CAN_IO_FWD
    if (currentReverse) canio |= CAN_IO_REV
    if (currentBms) canio |= CAN_IO_BMS

    // Scale throttle percent (0-100) to pot values (0-4095)
    const pot = Math.round((currentThrottle / 100) * 4095)

    sendMessage('updateCanIoFlags', {
      pot,
      pot2: pot,
      canio,
      cruisespeed: currentCruisespeed,
      regenpreset: currentRegenpreset
    })
  }

  return (
    <div class="can-io-control">
      <h3>{content.title}</h3>

      <div class="can-io-section">
        <h4>{content.configurationSection}</h4>
        <div class="can-io-row">
          <label>{content.canIdLabel}</label>
          <input
            type="text"
            value={canId}
            onInput={(e) => setCanIoCanId((e.target as HTMLInputElement).value.toUpperCase())}
            disabled={active}
            placeholder={content.canIdPlaceholder}
            maxLength={3}
          />
        </div>
        <div class="can-io-row">
          <label>{content.intervalLabel}</label>
          <input
            type="number"
            value={interval}
            onInput={(e) => setCanIoInterval(parseInt((e.target as HTMLInputElement).value) || 100)}
            disabled={active}
            min={10}
            max={500}
          />
          <span class="hint">{content.intervalHint}</span>
        </div>
        <div class="can-io-row">
          <label>
            <input
              type="checkbox"
              checked={useCrc}
              onChange={(e) => setCanIoUseCrc((e.target as HTMLInputElement).checked)}
              disabled={active}
            />
            <span>{content.useCrcLabel}</span>
          </label>
          <span class="hint">{content.useCrcHint}</span>
        </div>
      </div>

      <div class="can-io-section">
        <h4>{content.controlFlagsSection}</h4>
        <div class="can-io-flags">
          <label class="can-io-checkbox">
            <input
              type="checkbox"
              checked={cruise}
              onChange={(e) => { const val = (e.target as HTMLInputElement).checked; setCanIoCruise(val); handleUpdateFlags({ cruise: val }); }}
              disabled={!active}
            />
            <span>{content.cruiseFlag}</span>
          </label>
          <label class="can-io-checkbox">
            <input
              type="checkbox"
              checked={start}
              onChange={(e) => { const val = (e.target as HTMLInputElement).checked; setCanIoStart(val); handleUpdateFlags({ start: val }); }}
              disabled={!active}
            />
            <span>{content.startFlag}</span>
          </label>
          <label class="can-io-checkbox">
            <input
              type="checkbox"
              checked={brake}
              onChange={(e) => { const val = (e.target as HTMLInputElement).checked; setCanIoBrake(val); handleUpdateFlags({ brake: val }); }}
              disabled={!active}
            />
            <span>{content.brakeFlag}</span>
          </label>
          <label class="can-io-checkbox">
            <input
              type="checkbox"
              checked={forward}
              onChange={(e) => { const val = (e.target as HTMLInputElement).checked; setCanIoForward(val); handleUpdateFlags({ forward: val }); }}
              disabled={!active}
            />
            <span>{content.forwardFlag}</span>
          </label>
          <label class="can-io-checkbox">
            <input
              type="checkbox"
              checked={reverse}
              onChange={(e) => { const val = (e.target as HTMLInputElement).checked; setCanIoReverse(val); handleUpdateFlags({ reverse: val }); }}
              disabled={!active}
            />
            <span>{content.reverseFlag}</span>
          </label>
          <label class="can-io-checkbox">
            <input
              type="checkbox"
              checked={bms}
              onChange={(e) => { const val = (e.target as HTMLInputElement).checked; setCanIoBms(val); handleUpdateFlags({ bms: val }); }}
              disabled={!active}
            />
            <span>{content.bmsFlag}</span>
          </label>
        </div>
      </div>

      <div class="can-io-section">
        <h4>{content.throttleSection}</h4>
        <div class="can-io-row">
          <label>{content.throttleLabel}</label>
          <input
            type="range"
            value={throttlePercent}
            onInput={(e) => { const val = parseInt((e.target as HTMLInputElement).value); setCanIoThrottlePercent(val); handleUpdateFlags({ throttlePercent: val }); }}
            disabled={!active}
            min={0}
            max={100}
          />
          <span class="value">{throttlePercent}%</span>
        </div>
        <div class="can-io-row">
          <label>{content.cruiseSpeedLabel}</label>
          <input
            type="number"
            value={cruisespeed}
            onInput={(e) => { const val = parseInt((e.target as HTMLInputElement).value) || 0; setCanIoCruisespeed(val); handleUpdateFlags({ cruisespeed: val }); }}
            disabled={!active}
            min={0}
            max={16383}
          />
          <span class="hint">{content.cruiseSpeedHint}</span>
        </div>
        <div class="can-io-row">
          <label>{content.regenPresetLabel}</label>
          <input
            type="number"
            value={regenpreset}
            onInput={(e) => { const val = parseInt((e.target as HTMLInputElement).value) || 0; setCanIoRegenpreset(val); handleUpdateFlags({ regenpreset: val }); }}
            disabled={!active}
            min={0}
            max={255}
          />
          <span class="hint">{content.regenPresetHint}</span>
        </div>
      </div>

      <div class="can-io-actions">
        <button
          onClick={handleToggle}
          disabled={!isConnected}
          class={active ? 'stop-btn' : 'start-btn'}
        >
          {active ? content.stopButton : content.startButton}
        </button>
        {active && (
          <div class="can-io-status-indicator active">
            <span class="pulse"></span>
            {content.activeStatus({ interval })}
          </div>
        )}
      </div>
    </div>
  )
}
