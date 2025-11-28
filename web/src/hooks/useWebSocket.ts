import { useEffect, useRef, useState } from 'preact/hooks'

export interface WebSocketMessage {
  event: string
  data: any
}

export interface WebSocketOptions {
  onMessage?: (message: WebSocketMessage) => void
  onOpen?: () => void
  onClose?: () => void
  onError?: (error: Event) => void
  reconnectInterval?: number
}

export function useWebSocket(url: string, options: WebSocketOptions = {}) {
  const [isConnected, setIsConnected] = useState(false)
  const [lastMessage, setLastMessage] = useState<WebSocketMessage | null>(null)
  const wsRef = useRef<WebSocket | null>(null)
  const reconnectTimeoutRef = useRef<number | null>(null)
  const reconnectInterval = options.reconnectInterval || 3000

  const connect = () => {
    try {
      // Build WebSocket URL
      const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
      const host = window.location.hostname
      const port = window.location.port || (window.location.protocol === 'https:' ? '443' : '80')
      const wsUrl = `${protocol}//${host}:${port}${url}`

      console.log('Connecting to WebSocket:', wsUrl)
      const ws = new WebSocket(wsUrl)

      ws.onopen = () => {
        console.log('WebSocket connected')
        setIsConnected(true)
        if (options.onOpen) options.onOpen()
      }

      ws.onclose = () => {
        console.log('WebSocket disconnected')
        setIsConnected(false)
        if (options.onClose) options.onClose()

        // Attempt to reconnect
        reconnectTimeoutRef.current = window.setTimeout(() => {
          console.log('Attempting to reconnect...')
          connect()
        }, reconnectInterval)
      }

      ws.onerror = (error) => {
        console.error('WebSocket error:', error)
        if (options.onError) options.onError(error)
      }

      ws.onmessage = (event) => {
        try {
          const message: WebSocketMessage = JSON.parse(event.data)
          console.log('WebSocket message:', message)
          setLastMessage(message)
          if (options.onMessage) options.onMessage(message)
        } catch (error) {
          console.error('Failed to parse WebSocket message:', error)
        }
      }

      wsRef.current = ws
    } catch (error) {
      console.error('Failed to create WebSocket:', error)
    }
  }

  const sendMessage = (action: string, data: any = {}) => {
    if (wsRef.current && wsRef.current.readyState === WebSocket.OPEN) {
      const message = { action, ...data }
      console.log('Sending WebSocket message:', message)
      wsRef.current.send(JSON.stringify(message))
    } else {
      console.warn('WebSocket is not connected')
    }
  }

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

  return {
    isConnected,
    lastMessage,
    sendMessage,
    disconnect,
    reconnect: connect,
  }
}
