import { useEffect } from 'preact/hooks'

export type ToastType = 'success' | 'error' | 'warning' | 'info'

export interface ToastProps {
  id: string
  type: ToastType
  message: string
  duration?: number
  onClose: (id: string) => void
}

export default function Toast({ id, type, message, duration = 5000, onClose }: ToastProps) {
  useEffect(() => {
    if (duration > 0) {
      const timer = setTimeout(() => {
        onClose(id)
      }, duration)
      return () => clearTimeout(timer)
    }
  }, [id, duration, onClose])

  const getIcon = () => {
    switch (type) {
      case 'success':
        return (
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="12" cy="12" r="10"></circle>
            <path d="M9 12l2 2 4-4"></path>
          </svg>
        )
      case 'error':
        return (
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="12" cy="12" r="10"></circle>
            <path d="M15 9l-6 6m0-6l6 6"></path>
          </svg>
        )
      case 'warning':
        return (
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <path d="M12 2L2 20h20L12 2z"></path>
            <path d="M12 9v4m0 4h.01"></path>
          </svg>
        )
      case 'info':
        return (
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="12" cy="12" r="10"></circle>
            <path d="M12 16v-4m0-4h.01"></path>
          </svg>
        )
    }
  }

  return (
    <div class={`toast toast-${type}`}>
      <div class="toast-icon">{getIcon()}</div>
      <div class="toast-message">{message}</div>
      <button class="toast-close" onClick={() => onClose(id)}>
        <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <path d="M18 6L6 18M6 6l12 12"></path>
        </svg>
      </button>
    </div>
  )
}
