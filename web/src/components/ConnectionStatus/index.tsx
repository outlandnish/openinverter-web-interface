import { useIntlayer } from 'preact-intlayer'

interface ConnectionStatusProps {
  connected: boolean
  connecting?: boolean
  label?: string
  showIcon?: boolean
}

export default function ConnectionStatus({ connected, connecting = false, label, showIcon = true }: ConnectionStatusProps) {
  const content = useIntlayer('connection-status')

  const statusClass = connecting ? 'connecting' : (connected ? 'connected' : 'disconnected')

  return (
    <div class={`connection-status ${statusClass}`}>
      {showIcon && (
        <span class="status-indicator">
          {connecting ? (
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" class="spinner">
              <circle cx="12" cy="12" r="8" stroke-opacity="0.3"></circle>
              <path d="M12 4a8 8 0 0 1 8 8" stroke-linecap="round"></path>
            </svg>
          ) : connected ? (
            <svg viewBox="0 0 24 24" fill="currentColor">
              <circle cx="12" cy="12" r="8"></circle>
            </svg>
          ) : (
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <circle cx="12" cy="12" r="8"></circle>
              <path d="M15 9l-6 6m0-6l6 6"></path>
            </svg>
          )}
        </span>
      )}
      <span class="status-text">
        {label || (connecting ? content.connecting : (connected ? content.connected : content.disconnected))}
      </span>
    </div>
  )
}
