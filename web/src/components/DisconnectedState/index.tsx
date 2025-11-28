import { useIntlayer } from 'preact-intlayer'

interface DisconnectedStateProps {
  onReconnect: () => void
}

export default function DisconnectedState({ onReconnect }: DisconnectedStateProps) {
  const content = useIntlayer('disconnected-state')
  return (
    <div class="disconnected-state-wrapper">
      <div class="disconnected-state-content">
        <div class="connection-error-icon-large">
          <svg viewBox="0 0 24 24" fill="none" stroke-linecap="round">
            <circle cx="12" cy="12" r="10"></circle>
            <line x1="12" y1="8" x2="12" y2="12"></line>
            <circle cx="12" cy="16" r="0.5" fill="var(--oi-orange)"></circle>
          </svg>
        </div>
        <p class="empty-state-text">{content.title}</p>
        <p class="empty-state-hint">
          {content.message}
        </p>
        <button class="btn-with-icon" onClick={onReconnect}>
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M1 4v6h6M23 20v-6h-6"></path>
            <path d="M20.49 9A9 9 0 0 0 5.64 5.64L1 10m22 4l-4.64 4.36A9 9 0 0 1 3.51 15"></path>
          </svg>
          <span>{content.reconnect}</span>
        </button>
      </div>
    </div>
  )
}
