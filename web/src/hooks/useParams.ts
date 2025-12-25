import { useState, useEffect, useRef } from 'preact/hooks'
import { ParamStorage, ParameterList, getParameterDisplayName } from '../utils/paramStorage'
import { useWebSocketContext } from '../contexts/WebSocketContext'

/**
 * Parses enum definitions from unit strings
 * Format: "0=Rev1, 1=Rev2, 2=Rev3"
 * Returns: { "0": "Rev1", "1": "Rev2", "2": "Rev3" }
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
      // Add parsed enums if found
      ...(enums && { enums }),
      // Remove enum definition from unit field if enums were found
      ...(enums && { unit: undefined }),
    }
  }

  return processed
}

interface UseParamsResult {
  params: ParameterList | null
  loading: boolean
  error: string | null
  refresh: () => Promise<void>
  getParamId: (paramName: string) => number | null
  getDisplayName: (paramName: string) => string
  downloadProgress: number // 0-100 percentage of download complete (0 if indeterminate)
  downloadTotal: number // Total bytes to download (0 if unknown/indeterminate)
}

/**
 * Hook for managing device parameters with localStorage caching
 * @param deviceSerial - Device serial number for cache key
 * @param nodeId - Node ID to fetch parameters from (required for multi-client support)
 */
export function useParams(deviceSerial: string | undefined, nodeId: number | undefined): UseParamsResult {
  const explicitNodeId = nodeId
  const [params, setParams] = useState<ParameterList | null>(null)
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)
  const [refreshTrigger, setRefreshTrigger] = useState(0)
  const [downloadProgress, setDownloadProgress] = useState(0)
  const [downloadTotal, setDownloadTotal] = useState(0)
  const { subscribe, sendMessage } = useWebSocketContext()

  // Track which serial we currently have loaded to prevent redundant loads
  const loadedSerialRef = useRef<string | null>(null)

  // Track pending WebSocket requests
  const pendingSchemaRequestRef = useRef<{
    nodeId: number
    resolve: (schema: ParameterList) => void
    reject: (error: Error) => void
  } | null>(null)

  const pendingValuesRequestRef = useRef<{
    nodeId: number
    resolve: (values: Record<string, number | string>) => void
    reject: (error: Error) => void
  } | null>(null)

  const refresh = async () => {
    setRefreshTrigger(prev => prev + 1)
  }

  // Request schema via WebSocket and return a promise
  const requestSchemaViaWebSocket = (nodeId: number): Promise<ParameterList> => {
    return new Promise((resolve, reject) => {
      // Store the pending request
      pendingSchemaRequestRef.current = { nodeId, resolve, reject }

      // Send WebSocket message
      sendMessage('getParamSchema', { nodeId })

      // Set a timeout in case we don't get a response
      setTimeout(() => {
        if (pendingSchemaRequestRef.current && pendingSchemaRequestRef.current.nodeId === nodeId) {
          pendingSchemaRequestRef.current.reject(new Error('Schema request timeout'))
          pendingSchemaRequestRef.current = null
        }
      }, 30000) // 30 second timeout
    })
  }

  // Request values via WebSocket and return a promise
  const requestValuesViaWebSocket = (nodeId: number): Promise<Record<string, number | string>> => {
    return new Promise((resolve, reject) => {
      // Store the pending request
      pendingValuesRequestRef.current = { nodeId, resolve, reject }

      // Send WebSocket message
      sendMessage('getParamValues', { nodeId })

      // Set a timeout in case we don't get a response
      setTimeout(() => {
        if (pendingValuesRequestRef.current && pendingValuesRequestRef.current.nodeId === nodeId) {
          pendingValuesRequestRef.current.reject(new Error('Values request timeout'))
          pendingValuesRequestRef.current = null
        }
      }, 10000) // 10 second timeout (values are faster than full schema)
    })
  }

  const getParamId = (paramName: string): number | null => {
    if (!deviceSerial || !params) return null
    return params[paramName]?.id ?? null
  }

  const getDisplayName = (paramName: string): string => {
    if (!params || !params[paramName]) return paramName
    return getParameterDisplayName(paramName, params[paramName])
  }

  // Subscribe to WebSocket events for progress and params data
  useEffect(() => {
    const unsubscribe = subscribe((message) => {
      // Handle download progress events
      if (message.event === 'jsonProgress') {
        const bytesReceived = message.data.bytesReceived
        const totalBytes = message.data.totalBytes || 0
        const complete = message.data.complete

        // Update total size if known
        if (totalBytes > 0) {
          setDownloadTotal(totalBytes)
        }

        if (complete) {
          // Download complete
          setDownloadProgress(100)
          setDownloadTotal(0) // Reset for next download
        } else if (bytesReceived > 0 && totalBytes > 0) {
          // Calculate actual progress percentage
          const progress = Math.min(99, Math.round((bytesReceived / totalBytes) * 100))
          setDownloadProgress(progress)
        } else if (bytesReceived > 0) {
          // Total unknown - just update that we're downloading (indeterminate)
          setDownloadProgress(0) // 0 indicates indeterminate progress
        }
      }

      // Handle schema data response
      else if (message.event === 'paramSchemaData') {
        const nodeId = message.data.nodeId
        const rawSchema = message.data.schema

        // Check if this response is for a pending request
        if (pendingSchemaRequestRef.current && pendingSchemaRequestRef.current.nodeId === nodeId) {
          console.log('Received param schema via WebSocket for nodeId:', nodeId)

          // Strip 'value' fields from schema on frontend
          const schema: ParameterList = {}
          for (const [key, param] of Object.entries(rawSchema as ParameterList)) {
            const { value, ...schemaFields } = param
            schema[key] = schemaFields as any
          }

          pendingSchemaRequestRef.current.resolve(schema)
          pendingSchemaRequestRef.current = null
        }
      }

      // Handle schema error response
      else if (message.event === 'paramSchemaError') {
        const nodeId = message.data.nodeId
        const error = message.data.error

        // Check if this error is for a pending request
        if (pendingSchemaRequestRef.current && pendingSchemaRequestRef.current.nodeId === nodeId) {
          console.error('Schema error via WebSocket:', error)
          pendingSchemaRequestRef.current.reject(new Error(error))
          pendingSchemaRequestRef.current = null
        }
      }

      // Handle values data response
      else if (message.event === 'paramValuesData') {
        const nodeId = message.data.nodeId
        const rawParams = message.data.rawParams

        // Check if this response is for a pending request
        if (pendingValuesRequestRef.current && pendingValuesRequestRef.current.nodeId === nodeId) {
          console.log('Received param values via WebSocket for nodeId:', nodeId)

          // Extract id -> value mapping from raw params on frontend
          const values: Record<string, number | string> = {}
          for (const param of Object.values(rawParams as ParameterList)) {
            if (param.id !== undefined && param.value !== undefined) {
              values[param.id.toString()] = param.value
            }
          }

          pendingValuesRequestRef.current.resolve(values)
          pendingValuesRequestRef.current = null
        }
      }

      // Handle values error response
      else if (message.event === 'paramValuesError') {
        const nodeId = message.data.nodeId
        const error = message.data.error

        // Check if this error is for a pending request
        if (pendingValuesRequestRef.current && pendingValuesRequestRef.current.nodeId === nodeId) {
          console.error('Values error via WebSocket:', error)
          pendingValuesRequestRef.current.reject(new Error(error))
          pendingValuesRequestRef.current = null
        }
      }
    })

    return unsubscribe
  }, [subscribe])

  useEffect(() => {
    // Create AbortController for this effect
    const abortController = new AbortController()
    const forceRefresh = refreshTrigger > 0

    const loadParams = async () => {
      if (!deviceSerial) {
        setParams(null)
        setLoading(false)
        loadedSerialRef.current = null
        return
      }

      // Check if we already have the right params loaded (prevents redundant effect runs)
      // On refresh, we still want to reload values
      if (!forceRefresh && loadedSerialRef.current === deviceSerial && params !== null) {
        console.log('Params already loaded for device:', deviceSerial, '- skipping redundant load')
        if (!abortController.signal.aborted) {
          setLoading(false)
          setError(null)
        }
        return
      }

      // NodeId is required for fetching from device
      if (explicitNodeId === undefined) {
        // Clear params and show we're waiting for nodeId
        if (!abortController.signal.aborted) {
          setParams(null)
          setLoading(false)
          setError(null)
        }
        console.log('Waiting for nodeId to fetch parameters for device:', deviceSerial)
        return
      }

      try {
        // Start loading
        if (!abortController.signal.aborted) {
          setLoading(true)
          setError(null)
        }

        console.log(`Fetching parameters from nodeId ${explicitNodeId} for serial:`, deviceSerial)
        setDownloadProgress(0)
        setDownloadTotal(0) // Reset for new download

        // STEP 1: Get schema (from cache or fetch from device)
        let schema: ParameterList
        const cachedSchema = !forceRefresh ? ParamStorage.getSchema(deviceSerial) : null

        if (cachedSchema) {
          console.log('Using cached schema from localStorage for device:', deviceSerial)
          schema = cachedSchema
        } else {
          console.log('Fetching fresh schema from device for nodeId:', explicitNodeId)
          try {
            schema = await requestSchemaViaWebSocket(explicitNodeId)

            // Check if request was aborted while waiting
            if (abortController.signal.aborted) {
              console.log('Schema fetch aborted for device:', deviceSerial)
              return
            }

            // Process schema to parse enums from unit strings
            schema = processParameters(schema)

            // Save schema to localStorage
            ParamStorage.saveSchema(deviceSerial, schema)
          } catch (err) {
            // Clear pending request on abort
            if (abortController.signal.aborted) {
              pendingSchemaRequestRef.current = null
              console.log('Schema fetch was aborted')
              return
            }
            throw err // Re-throw if not aborted
          }
        }

        // STEP 2: Always fetch fresh values from device
        console.log('Fetching fresh values from device for nodeId:', explicitNodeId)
        let values: Record<string, number | string>
        try {
          values = await requestValuesViaWebSocket(explicitNodeId)

          // Check if request was aborted while waiting
          if (abortController.signal.aborted) {
            console.log('Values fetch aborted for device:', deviceSerial)
            return
          }
        } catch (err) {
          // Clear pending request on abort
          if (abortController.signal.aborted) {
            pendingValuesRequestRef.current = null
            console.log('Values fetch was aborted')
            return
          }
          throw err // Re-throw if not aborted
        }

        // STEP 3: Merge schema + values
        const mergedParams: ParameterList = {}
        for (const [key, paramDef] of Object.entries(schema)) {
          const paramId = paramDef.id
          mergedParams[key] = {
            ...paramDef,
            // Update value from fresh values if available
            value: paramId !== undefined && values[paramId] !== undefined
              ? values[paramId]
              : paramDef.value // Fallback to schema value if not found
          }
        }

        // Check if aborted before setting state
        if (!abortController.signal.aborted) {
          setParams(mergedParams)
          setLoading(false)
          setError(null)
          loadedSerialRef.current = deviceSerial
        }

        console.log('Successfully loaded parameters for device:', deviceSerial)
      } catch (err) {
        // Ignore abort errors
        if (err instanceof Error && err.name === 'AbortError') {
          console.log('Parameter fetch was aborted')
          return
        }

        if (!abortController.signal.aborted) {
          console.error('Failed to load parameters:', err)
          setError(err instanceof Error ? err.message : 'Failed to load parameters')
        }
      } finally {
        if (!abortController.signal.aborted) {
          setLoading(false)
          setDownloadProgress(0)
          setDownloadTotal(0)
        }
      }
    }

    loadParams()

    // Cleanup: abort the request when component unmounts or deviceSerial/refreshTrigger/nodeId changes
    return () => {
      abortController.abort()
      // Clear any pending WebSocket requests
      if (pendingSchemaRequestRef.current) {
        pendingSchemaRequestRef.current.reject(new Error('Request cancelled'))
        pendingSchemaRequestRef.current = null
      }
      if (pendingValuesRequestRef.current) {
        pendingValuesRequestRef.current.reject(new Error('Request cancelled'))
        pendingValuesRequestRef.current = null
      }
    }
  }, [deviceSerial, refreshTrigger, explicitNodeId, sendMessage])

  return {
    params,
    loading,
    error,
    refresh,
    getParamId,
    getDisplayName,
    downloadProgress,
    downloadTotal,
  }
}
