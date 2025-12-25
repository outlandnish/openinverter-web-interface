export interface LoadingSpinnerProps {
  size?: 'small' | 'medium' | 'large'
  label?: string
  className?: string
}

export function LoadingSpinner({ size = 'medium', label, className }: LoadingSpinnerProps) {
  const sizeClass = `spinner-${size}`

  return (
    <div class={`loading-spinner-container ${className || ''}`}>
      <svg
        class={`loading-spinner ${sizeClass}`}
        viewBox="0 0 24 24"
        fill="none"
        stroke="currentColor"
        stroke-width="2"
      >
        <circle cx="12" cy="12" r="8" stroke-opacity="0.3"></circle>
        <path d="M12 4a8 8 0 0 1 8 8" stroke-linecap="round"></path>
      </svg>
      {label && <div class="loading-spinner-label">{label}</div>}
    </div>
  )
}
