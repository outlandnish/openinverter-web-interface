import { useIntlayer } from 'preact-intlayer'

interface ConnectionStatusProps {
  connected: boolean
  label?: string
  showIcon?: boolean
}

export default function ConnectionStatus({ connected, label, showIcon = true }: ConnectionStatusProps) {
  const content = useIntlayer('connection-status')
  return (
    <div class={`connection-status ${connected ? 'connected' : 'disconnected'}`}>
      {showIcon && (
        <span class="status-indicator">
          {connected ? (
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
        {label || (connected ? content.connected : content.disconnected)}
      </span>
    </div>
  )
}
