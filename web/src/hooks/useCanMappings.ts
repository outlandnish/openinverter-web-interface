import { useState, useEffect, useRef } from 'preact/hooks'
import { useWebSocketContext } from '../contexts/WebSocketContext'

export interface CanMapping {
  isrx: boolean
  id: number
  paramid: number
  position: number
  length: number
  gain: number
  offset: number
  index: number
  subindex: number
}

interface UseCanMappingsResult {
  mappings: CanMapping[]
  loading: boolean
  error: string | null
  refresh: () => Promise<void>
  addMapping: (mapping: Omit<CanMapping, 'index' | 'subindex'>) => Promise<void>
  removeMapping: (index: number, subindex: number) => Promise<void>
}

/**
 * Hook for managing CAN mappings via WebSocket
 */
export function useCanMappings(): UseCanMappingsResult {
  const [mappings, setMappings] = useState<CanMapping[]>([])
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const { subscribe, sendMessage } = useWebSocketContext()

  // Track pending WebSocket requests
  const pendingRequestRef = useRef<{
    type: 'get' | 'add' | 'remove'
    resolve: (value: any) => void
    reject: (error: Error) => void
  } | null>(null)

  const refresh = async () => {
    return new Promise<void>((resolve, reject) => {
      pendingRequestRef.current = { type: 'get', resolve, reject }
      setLoading(true)
      setError(null)
      sendMessage('getCanMappings', {})

      // Set a timeout
      setTimeout(() => {
        if (pendingRequestRef.current && pendingRequestRef.current.type === 'get') {
          pendingRequestRef.current.reject(new Error('Request timeout'))
          pendingRequestRef.current = null
          setLoading(false)
        }
      }, 10000)
    })
  }

  const addMapping = async (mapping: Omit<CanMapping, 'index' | 'subindex'>) => {
    return new Promise<void>((resolve, reject) => {
      pendingRequestRef.current = { type: 'add', resolve, reject }
      setError(null)
      sendMessage('addCanMapping', mapping)

      // Set a timeout
      setTimeout(() => {
        if (pendingRequestRef.current && pendingRequestRef.current.type === 'add') {
          pendingRequestRef.current.reject(new Error('Request timeout'))
          pendingRequestRef.current = null
        }
      }, 10000)
    })
  }

  const removeMapping = async (index: number, subindex: number) => {
    return new Promise<void>((resolve, reject) => {
      pendingRequestRef.current = { type: 'remove', resolve, reject }
      setError(null)
      sendMessage('removeCanMapping', { index, subindex })

      // Set a timeout
      setTimeout(() => {
        if (pendingRequestRef.current && pendingRequestRef.current.type === 'remove') {
          pendingRequestRef.current.reject(new Error('Request timeout'))
          pendingRequestRef.current = null
        }
      }, 10000)
    })
  }

  // Subscribe to WebSocket events
  useEffect(() => {
    const unsubscribe = subscribe((message) => {
      // Handle mappings data response
      if (message.event === 'canMappingsData') {
        const mappingsData = message.data.mappings || []
        console.log('Received CAN mappings data:', mappingsData)

        if (pendingRequestRef.current && pendingRequestRef.current.type === 'get') {
          setMappings(mappingsData)
          setLoading(false)
          pendingRequestRef.current.resolve(undefined)
          pendingRequestRef.current = null
        }
      }

      // Handle mappings error response
      else if (message.event === 'canMappingsError') {
        const errorMsg = message.data.error
        console.error('CAN mappings error:', errorMsg)

        if (pendingRequestRef.current && pendingRequestRef.current.type === 'get') {
          setError(errorMsg)
          setLoading(false)
          pendingRequestRef.current.reject(new Error(errorMsg))
          pendingRequestRef.current = null
        }
      }

      // Handle mapping added response
      else if (message.event === 'canMappingAdded') {
        console.log('CAN mapping added successfully')

        if (pendingRequestRef.current && pendingRequestRef.current.type === 'add') {
          pendingRequestRef.current.resolve(undefined)
          pendingRequestRef.current = null
          // Refresh mappings after adding
          refresh()
        }
      }

      // Handle mapping removed response
      else if (message.event === 'canMappingRemoved') {
        console.log('CAN mapping removed successfully')

        if (pendingRequestRef.current && pendingRequestRef.current.type === 'remove') {
          pendingRequestRef.current.resolve(undefined)
          pendingRequestRef.current = null
          // Refresh mappings after removing
          refresh()
        }
      }

      // Handle mapping error (add/remove)
      else if (message.event === 'canMappingError') {
        const errorMsg = message.data.error
        console.error('CAN mapping operation error:', errorMsg)

        if (pendingRequestRef.current && (pendingRequestRef.current.type === 'add' || pendingRequestRef.current.type === 'remove')) {
          setError(errorMsg)
          pendingRequestRef.current.reject(new Error(errorMsg))
          pendingRequestRef.current = null
        }
      }
    })

    return unsubscribe
  }, [subscribe])

  // Load mappings on mount
  useEffect(() => {
    refresh()
  }, [])

  return {
    mappings,
    loading,
    error,
    refresh,
    addMapping,
    removeMapping,
  }
}
