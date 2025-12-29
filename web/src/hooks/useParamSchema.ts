import { useState, useEffect, useRef } from 'preact/hooks'
import { ParamStorage, ParameterList, getParameterDisplayName } from '../utils/paramStorage'
import { useWebSocketContext } from '../contexts/WebSocketContext'

/**
 * Parses enum definitions from unit strings
 * Format: "0=Rev1, 1=Rev2, 2=Rev3"
 */
function parseEnumsFromUnit(unit: string | undefined): Record<string, string> | null {
  if (!unit || !unit.includes('=')) {
    return null
  }

  const enums: Record<string, string> = {}
  const parts = unit.split(',').map(p => p.trim())

  for (const part of parts) {
    const match = part.match(/^(\d+)=(.+)$/)
    if (match) {
      enums[match[1]] = match[2]
    }
  }

  return Object.keys(enums).length > 0 ? enums : null
}

/**
 * Processes parameters to parse enum definitions from unit strings
 */
function processParameters(params: ParameterList): ParameterList {
  const processed: ParameterList = {}

  for (const [key, param] of Object.entries(params)) {
    const enums = parseEnumsFromUnit(param.unit)

    processed[key] = {
      ...param,
      ...(enums && { enums }),
      ...(enums && { unit: undefined }),
    }
  }

  return processed
}

interface UseParamSchemaResult {
  schema: ParameterList | null
  loading: boolean
  hasSchema: boolean
  getParamId: (paramName: string) => number | null
  getDisplayName: (paramName: string) => string
}

/**
 * Hook for accessing parameter schema (definitions only, no values).
 *
 * This hook:
 * 1. Returns cached schema immediately if available in localStorage
 * 2. If not cached, tries to fetch from ESP32 cache
 * 3. If ESP32 cache is empty, triggers a background download from the OpenInverter device
 * 4. Extracts and caches schema when download completes
 *
 * Unlike useParams, this hook is optimized for non-blocking schema access:
 * - Spot values can start streaming as soon as schema is available
 * - The download happens in the background without blocking the UI
 *
 * @param deviceSerial - Device serial number for cache lookup
 * @param nodeId - Node ID for fetching from device
 */
export function useParamSchema(
  deviceSerial: string | undefined,
  nodeId?: number
): UseParamSchemaResult {
  const [schema, setSchema] = useState<ParameterList | null>(null)
  const [loading, setLoading] = useState(false)
  const { subscribe, sendMessage } = useWebSocketContext()

  // Track request state
  const requestStateRef = useRef<{
    phase: 'idle' | 'checking-esp32' | 'downloading'
    nodeId?: number
    timeoutId?: ReturnType<typeof setTimeout>
  }>({ phase: 'idle' })

  // Subscribe to WebSocket events
  useEffect(() => {
    const unsubscribe = subscribe((message) => {
      // Handle schema data from ESP32 cache (getParamSchema response)
      if (message.event === 'paramSchemaData') {
        const msgNodeId = message.data.nodeId
        const rawSchema = message.data.schema as ParameterList

        if (requestStateRef.current.phase === 'checking-esp32' &&
            requestStateRef.current.nodeId === msgNodeId) {
          console.log('[useParamSchema] Received schema from ESP32 cache for nodeId:', msgNodeId)

          // Clear timeout
          if (requestStateRef.current.timeoutId) {
            clearTimeout(requestStateRef.current.timeoutId)
          }
          requestStateRef.current = { phase: 'idle' }

          // Process and cache schema
          const processedSchema = extractAndProcessSchema(rawSchema)
          if (deviceSerial) {
            ParamStorage.saveSchema(deviceSerial, processedSchema)
          }

          setSchema(processedSchema)
          setLoading(false)
        }
      }

      // Handle schema error - ESP32 doesn't have it cached
      else if (message.event === 'paramSchemaError') {
        const msgNodeId = message.data.nodeId

        if (requestStateRef.current.phase === 'checking-esp32' &&
            requestStateRef.current.nodeId === msgNodeId) {
          console.log('[useParamSchema] ESP32 cache empty, triggering background download')

          // Clear timeout
          if (requestStateRef.current.timeoutId) {
            clearTimeout(requestStateRef.current.timeoutId)
          }

          // Trigger download via getParamValues
          requestStateRef.current = {
            phase: 'downloading',
            nodeId: msgNodeId,
            timeoutId: setTimeout(() => {
              console.log('[useParamSchema] Download timed out')
              requestStateRef.current = { phase: 'idle' }
              setLoading(false)
            }, 120000) // 2 minute timeout for download
          }

          sendMessage('getParamValues', { nodeId: msgNodeId })
        }
      }

      // Handle param values data (from background download)
      else if (message.event === 'paramValuesData') {
        const msgNodeId = message.data.nodeId
        const rawParams = message.data.rawParams as ParameterList

        if (requestStateRef.current.phase === 'downloading' &&
            requestStateRef.current.nodeId === msgNodeId) {
          console.log('[useParamSchema] Received param data from download for nodeId:', msgNodeId)

          // Clear timeout
          if (requestStateRef.current.timeoutId) {
            clearTimeout(requestStateRef.current.timeoutId)
          }
          requestStateRef.current = { phase: 'idle' }

          // Extract schema from the full params data
          const processedSchema = extractAndProcessSchema(rawParams)
          if (deviceSerial) {
            ParamStorage.saveSchema(deviceSerial, processedSchema)
          }

          setSchema(processedSchema)
          setLoading(false)
        }
      }

      // Handle download pending (download in progress)
      else if (message.event === 'paramValuesPending') {
        const msgNodeId = message.data.nodeId

        if (requestStateRef.current.phase === 'downloading' &&
            requestStateRef.current.nodeId === msgNodeId) {
          console.log('[useParamSchema] Download in progress:', message.data.message)

          // Reset timeout since download is progressing
          if (requestStateRef.current.timeoutId) {
            clearTimeout(requestStateRef.current.timeoutId)
          }
          requestStateRef.current.timeoutId = setTimeout(() => {
            console.log('[useParamSchema] Download timed out')
            requestStateRef.current = { phase: 'idle' }
            setLoading(false)
          }, 120000)
        }
      }

      // Handle download error
      else if (message.event === 'paramValuesError') {
        const msgNodeId = message.data.nodeId

        if (requestStateRef.current.phase === 'downloading' &&
            requestStateRef.current.nodeId === msgNodeId) {
          console.error('[useParamSchema] Download failed:', message.data.error)

          if (requestStateRef.current.timeoutId) {
            clearTimeout(requestStateRef.current.timeoutId)
          }
          requestStateRef.current = { phase: 'idle' }
          setLoading(false)
        }
      }
    })

    return unsubscribe
  }, [subscribe, deviceSerial, sendMessage])

  // Load schema from cache or trigger download
  useEffect(() => {
    if (!deviceSerial) {
      setSchema(null)
      setLoading(false)
      return
    }

    // First check localStorage cache
    const cached = ParamStorage.getSchema(deviceSerial)
    if (cached) {
      console.log('[useParamSchema] Using cached schema from localStorage for device:', deviceSerial)
      setSchema(cached)
      setLoading(false)
      return
    }

    // Not in localStorage - need to fetch
    if (nodeId !== undefined) {
      console.log('[useParamSchema] Checking ESP32 cache for nodeId:', nodeId)
      setLoading(true)

      // First try getParamSchema (checks ESP32 cache, doesn't trigger download)
      requestStateRef.current = {
        phase: 'checking-esp32',
        nodeId,
        timeoutId: setTimeout(() => {
          // If no response, assume ESP32 doesn't have it, trigger download
          if (requestStateRef.current.phase === 'checking-esp32') {
            console.log('[useParamSchema] ESP32 cache check timed out, triggering download')
            requestStateRef.current = {
              phase: 'downloading',
              nodeId,
              timeoutId: setTimeout(() => {
                requestStateRef.current = { phase: 'idle' }
                setLoading(false)
              }, 120000)
            }
            sendMessage('getParamValues', { nodeId })
          }
        }, 3000) // 3 second timeout to check ESP32 cache
      }

      sendMessage('getParamSchema', { nodeId })
    } else {
      setSchema(null)
      setLoading(false)
    }

    return () => {
      // Cleanup
      if (requestStateRef.current.timeoutId) {
        clearTimeout(requestStateRef.current.timeoutId)
      }
      requestStateRef.current = { phase: 'idle' }
    }
  }, [deviceSerial, nodeId, sendMessage])

  const getParamId = (paramName: string): number | null => {
    if (!schema) return null
    return schema[paramName]?.id ?? null
  }

  const getDisplayName = (paramName: string): string => {
    if (!schema || !schema[paramName]) return paramName
    return getParameterDisplayName(paramName, schema[paramName])
  }

  return {
    schema,
    loading,
    hasSchema: schema !== null,
    getParamId,
    getDisplayName,
  }
}

/**
 * Extract schema from raw params and process it
 */
function extractAndProcessSchema(rawParams: ParameterList): ParameterList {
  const schema: ParameterList = {}
  for (const [key, param] of Object.entries(rawParams)) {
    // Strip 'value' field for schema
    const { value, ...schemaFields } = param
    schema[key] = schemaFields as any
  }
  return processParameters(schema)
}
