import { useState, useCallback } from 'preact/hooks'
import { createContext } from 'preact'
import { ComponentChildren } from 'preact'
import Toast, { ToastType } from '@components/Toast'

interface ToastData {
  id: string
  type: ToastType
  message: string
  duration?: number
}

interface ToastContextValue {
  showToast: (type: ToastType, message: string, duration?: number) => void
  showError: (message: string) => void
  showSuccess: (message: string) => void
  showWarning: (message: string) => void
  showInfo: (message: string) => void
}

export const ToastContext = createContext<ToastContextValue>({
  showToast: () => {},
  showError: () => {},
  showSuccess: () => {},
  showWarning: () => {},
  showInfo: () => {},
})

export function ToastProvider({ children }: { children: ComponentChildren }) {
  const [toasts, setToasts] = useState<ToastData[]>([])

  const showToast = useCallback((type: ToastType, message: string, duration = 5000) => {
    const id = `toast-${Date.now()}-${Math.random()}`
    setToasts(prev => [...prev, { id, type, message, duration }])
  }, [])

  const showError = useCallback((message: string) => showToast('error', message, 7000), [showToast])
  const showSuccess = useCallback((message: string) => showToast('success', message, 3000), [showToast])
  const showWarning = useCallback((message: string) => showToast('warning', message, 5000), [showToast])
  const showInfo = useCallback((message: string) => showToast('info', message, 4000), [showToast])

  const removeToast = useCallback((id: string) => {
    setToasts(prev => prev.filter(toast => toast.id !== id))
  }, [])

  return (
    <ToastContext.Provider value={{ showToast, showError, showSuccess, showWarning, showInfo }}>
      {children}
      <div class="toast-container">
        {toasts.map(toast => (
          <Toast
            key={toast.id}
            id={toast.id}
            type={toast.type}
            message={toast.message}
            duration={toast.duration}
            onClose={removeToast}
          />
        ))}
      </div>
    </ToastContext.Provider>
  )
}
