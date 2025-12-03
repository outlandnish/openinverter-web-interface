import { createContext, ComponentChildren } from 'preact'
import { useContext, useEffect, useRef, useState, useCallback } from 'preact/hooks'

export interface WebSocketMessage {
  event: string
  data: any
}

export interface WebSocketContextValue {
  isConnected: boolean
  isConnecting: boolean
  isRetrying: boolean
  sendMessage: (action: string, data?: any) => void
  subscribe: (handler: (message: WebSocketMessage) => void) => () => void
}

const WebSocketContext = createContext<WebSocketContextValue | null>(null)

interface WebSocketProviderProps {
  children: ComponentChildren
  url: string
}

export function WebSocketProvider({ children, url }: WebSocketProviderProps) {
  const [isConnected, setIsConnected] = useState(false)
  const [isConnecting, setIsConnecting] = useState(true) // Start as connecting on mount
  const [isRetrying, setIsRetrying] = useState(false)
  const wsRef = useRef<WebSocket | null>(null)
  const reconnectTimeoutRef = useRef<number | null>(null)
  const subscribersRef = useRef<Set<(message: WebSocketMessage) => void>>(new Set())
  const reconnectInterval = 3000

  const connect = () => {
    try {
      // Build WebSocket URL
      const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
      const host = window.location.hostname
      const port = window.location.port || (window.location.protocol === 'https:' ? '443' : '80')
      const wsUrl = `${protocol}//${host}:${port}${url}`

      console.log('Connecting to WebSocket:', wsUrl)
      setIsConnecting(true)
      setIsConnected(false)

      const ws = new WebSocket(wsUrl)

      ws.onopen = () => {
        console.log('WebSocket connected')
        setIsConnecting(false)
        setIsConnected(true)
        setIsRetrying(false) // Reset retry flag on successful connection
      }

      ws.onclose = (event) => {
        console.log('WebSocket disconnected', event.code, event.reason)

        // Only set to disconnected state after a small delay to ensure UI updates
        setTimeout(() => {
          setIsConnecting(false)
          setIsConnected(false)

          // Mark as retrying for subsequent connection attempts
          setIsRetrying(true)

          // Attempt to reconnect after showing error state
          reconnectTimeoutRef.current = window.setTimeout(() => {
            console.log('Attempting to reconnect...')
            connect()
          }, reconnectInterval)
        }, 100)
      }

      ws.onerror = (error) => {
        console.error('WebSocket error:', error)
        // Don't set states here - let onclose handle it
      }

      ws.onmessage = (event) => {
        try {
          const message: WebSocketMessage = JSON.parse(event.data)
          console.log('WebSocket message:', message)

          // Notify all subscribers
          subscribersRef.current.forEach(handler => {
            try {
              handler(message)
            } catch (error) {
              console.error('Error in WebSocket message handler:', error)
            }
          })
        } catch (error) {
          console.error('Failed to parse WebSocket message:', error)
        }
      }

      wsRef.current = ws
    } catch (error) {
      console.error('Failed to create WebSocket:', error)
    }
  }

  const sendMessage = useCallback((action: string, data: any = {}) => {
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      const message = { action, ...data }
      console.log('Sending WebSocket message:', message)
      wsRef.current.send(JSON.stringify(message))
    } else {
      console.warn('WebSocket is not connected')
    }
  }, [])

  const subscribe = useCallback((handler: (message: WebSocketMessage) => void) => {
    subscribersRef.current.add(handler)
    return () => {
      subscribersRef.current.delete(handler)
    }
  }, [])

  const disconnect = () => {
    if (reconnectTimeoutRef.current) {
      clearTimeout(reconnectTimeoutRef.current)
      reconnectTimeoutRef.current = null
    }

    if (wsRef.current) {
      wsRef.current.close()
      wsRef.current = null
    }
  }

  useEffect(() => {
    connect()

    return () => {
      disconnect()
    }
  }, [url])

  const value: WebSocketContextValue = {
    isConnected,
    isConnecting,
    isRetrying,
    sendMessage,
    subscribe,
  }

  return (
    <WebSocketContext.Provider value={value}>
      {children}
    </WebSocketContext.Provider>
  )
}

export function useWebSocketContext() {
  const context = useContext(WebSocketContext)
  if (!context) {
    throw new Error('useWebSocketContext must be used within a WebSocketProvider')
  }
  return context
}
