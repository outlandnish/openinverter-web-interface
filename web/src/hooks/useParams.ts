import { useState, useEffect, useRef } from 'preact/hooks'
import { ParamStorage, ParameterList, getParameterDisplayName } from '../utils/paramStorage'
import { useWebSocketContext } from '../contexts/WebSocketContext'
import { useDeviceDetailsContext } from '../contexts/DeviceDetailsContext'

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
 * Hook for managing device parameters with session caching in DeviceDetailsContext
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
  const { parameters, setCachedParameters, updateParameterValue } = useDeviceDetailsContext()

  // Track which serial we currently have loaded to prevent redundant loads
  const loadedSerialRef = useRef<string | null>(null)

  // Track pending WebSocket requests
  const pendingSchemaRequestRef = useRef<{
    nodeId: number
    resolve: (schema: ParameterList) => void
    reject: (error: Error) => void
    timeoutId?: ReturnType<typeof setTimeout>
  } | null>(null)

  const pendingValuesRequestRef = useRef<{
    nodeId: number
    resolve: (rawParams: ParameterList) => void
    reject: (error: Error) => void
    timeoutId?: ReturnType<typeof setTimeout>
  } | null>(null)

  const refresh = async () => {
    setRefreshTrigger(prev => prev + 1)
  }

  // Request values via WebSocket and return a promise
  // If we have a cached schema, use the optimized values-only endpoint
  // Returns the full raw params (which contains both schema info and values)
  const requestValuesViaWebSocket = (nodeId: number, hasSchema: boolean): Promise<ParameterList> => {
    return new Promise((resolve, reject) => {
      // Set a timeout in case we don't get a response
      const timeoutId = setTimeout(() => {
        if (pendingValuesRequestRef.current && pendingValuesRequestRef.current.nodeId === nodeId) {
          pendingValuesRequestRef.current.reject(new Error('Values request timeout'))
          pendingValuesRequestRef.current = null
        }
      }, 60000) // 60 second timeout (downloads can take a while for large param sets)

      // Store the pending request with timeout reference
      pendingValuesRequestRef.current = { nodeId, resolve, reject, timeoutId }

      // Use optimized endpoint if we have schema cached
      if (hasSchema) {
        sendMessage('getParamValuesOnly', { nodeId })
      } else {
        sendMessage('getParamValues', { nodeId })
      }
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

          // Clear timeout
          if (pendingSchemaRequestRef.current.timeoutId) {
            clearTimeout(pendingSchemaRequestRef.current.timeoutId)
          }

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

          // Clear timeout
          if (pendingSchemaRequestRef.current.timeoutId) {
            clearTimeout(pendingSchemaRequestRef.current.timeoutId)
          }

          pendingSchemaRequestRef.current.reject(new Error(error))
          pendingSchemaRequestRef.current = null
        }
      }

      // Handle values pending response (download in progress)
      else if (message.event === 'paramValuesPending') {
        const nodeId = message.data.nodeId
        console.log('Param values download pending for nodeId:', nodeId, message.data.message)

        // Reset timeout when we receive pending - download is in progress
        if (pendingValuesRequestRef.current && pendingValuesRequestRef.current.nodeId === nodeId) {
          // Clear existing timeout and set a new one
          if (pendingValuesRequestRef.current.timeoutId) {
            clearTimeout(pendingValuesRequestRef.current.timeoutId)
          }
          pendingValuesRequestRef.current.timeoutId = setTimeout(() => {
            if (pendingValuesRequestRef.current && pendingValuesRequestRef.current.nodeId === nodeId) {
              pendingValuesRequestRef.current.reject(new Error('Values download timeout'))
              pendingValuesRequestRef.current = null
            }
          }, 120000) // 120 second timeout for active downloads
        }
      }

      // Handle values data response
      else if (message.event === 'paramValuesData') {
        const nodeId = message.data.nodeId
        const rawParams = message.data.rawParams as ParameterList

        console.log('[useParams] Received paramValuesData event:', {
          nodeId,
          hasPendingRequest: !!pendingValuesRequestRef.current,
          pendingNodeId: pendingValuesRequestRef.current?.nodeId,
          rawParamsKeys: rawParams ? Object.keys(rawParams).length : 0
        })

        // Check if this response is for a pending request
        if (pendingValuesRequestRef.current && pendingValuesRequestRef.current.nodeId === nodeId) {
          console.log('[useParams] Received param values via WebSocket for nodeId:', nodeId, 'with', Object.keys(rawParams).length, 'params')

          // Clear timeout
          if (pendingValuesRequestRef.current.timeoutId) {
            clearTimeout(pendingValuesRequestRef.current.timeoutId)
          }

          // Return the full raw params (contains both schema and values)
          pendingValuesRequestRef.current.resolve(rawParams)
          pendingValuesRequestRef.current = null
        }
      }

      // Handle values-only data response (optimized, no schema)
      else if (message.event === 'paramValuesOnly') {
        const nodeId = message.data.nodeId
        const values = message.data.values as Record<string, number>

        console.log('[useParams] Received paramValuesOnly event:', {
          nodeId,
          hasPendingRequest: !!pendingValuesRequestRef.current,
          pendingNodeId: pendingValuesRequestRef.current?.nodeId,
          valueCount: values ? Object.keys(values).length : 0
        })

        // Check if this response is for a pending request
        if (pendingValuesRequestRef.current && pendingValuesRequestRef.current.nodeId === nodeId) {
          console.log('[useParams] Received values-only via WebSocket for nodeId:', nodeId, 'with', Object.keys(values).length, 'values')

          // Clear timeout
          if (pendingValuesRequestRef.current.timeoutId) {
            clearTimeout(pendingValuesRequestRef.current.timeoutId)
          }

          // Convert id->value map back to full parameter list format
          // Note: This will be merged with cached schema by the caller
          const rawParams: ParameterList = {}
          for (const [idStr, value] of Object.entries(values)) {
            // We'll create minimal param objects - caller will merge with schema
            rawParams[idStr] = { id: parseInt(idStr), value } as any
          }

          pendingValuesRequestRef.current.resolve(rawParams)
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

          // Clear timeout
          if (pendingValuesRequestRef.current.timeoutId) {
            clearTimeout(pendingValuesRequestRef.current.timeoutId)
          }

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

      // Check context cache first (persists between tab changes)
      if (!forceRefresh && parameters.cached && parameters.lastFetchTime) {
        if (!abortController.signal.aborted) {
          const processed = processParameters(parameters.cached)
          setParams(processed)
          setLoading(false)
          setError(null)
          loadedSerialRef.current = deviceSerial
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

        // Check for cached schema in localStorage
        let schema: ParameterList | null = !forceRefresh ? ParamStorage.getSchema(deviceSerial) : null

        if (schema) {
          console.log('Using cached schema from localStorage for device:', deviceSerial)
        }

        // Always fetch full parameters from device
        let rawParams: ParameterList
        try {
          rawParams = await requestValuesViaWebSocket(explicitNodeId, false)

          // Check if request was aborted while waiting
          if (abortController.signal.aborted) {
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

        // If we got values-only response (param ids as keys), we need to map them to schema keys
        if (schema && Object.keys(rawParams).some(key => !isNaN(Number(key)))) {
          console.log('Received values-only response, mapping to schema')
          const valueById: Record<number, number> = {}
          for (const [idStr, param] of Object.entries(rawParams)) {
            valueById[param.id] = param.value
          }
          
          // Create new rawParams with proper keys from schema
          const mappedParams: ParameterList = {}
          for (const [key, paramDef] of Object.entries(schema)) {
            const value = valueById[paramDef.id]
            mappedParams[key] = {
              ...paramDef,
              value: value !== undefined ? value : paramDef.value
            }
          }
          rawParams = mappedParams
        }

        // If no cached schema, extract from rawParams and cache it
        if (!schema) {
          console.log('Extracting schema from raw params for device:', deviceSerial)
          schema = {}
          for (const [key, param] of Object.entries(rawParams)) {
            // Strip 'value' field for schema
            const { value, ...schemaFields } = param
            schema[key] = schemaFields as any
          }
          // Process schema to parse enums from unit strings
          schema = processParameters(schema)
          // Save schema to localStorage for future use
          ParamStorage.saveSchema(deviceSerial, schema)
        }

        // Merge cached schema with fresh values from rawParams
        const mergedParams: ParameterList = {}
        for (const [key, paramDef] of Object.entries(schema)) {
          mergedParams[key] = {
            ...paramDef,
            // Use value from rawParams (fresh from device)
            value: rawParams[key]?.value ?? paramDef.value
          }
        }

        // Check if aborted before setting state
        if (!abortController.signal.aborted) {
          setParams(mergedParams)
          setLoading(false)
          setError(null)
          loadedSerialRef.current = deviceSerial
          
          // Cache parameters in context for persistence between tab changes
          setCachedParameters(mergedParams)
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
      // Clear any pending WebSocket requests and their timeouts
      if (pendingSchemaRequestRef.current) {
        if (pendingSchemaRequestRef.current.timeoutId) {
          clearTimeout(pendingSchemaRequestRef.current.timeoutId)
        }
        pendingSchemaRequestRef.current.reject(new Error('Request cancelled'))
        pendingSchemaRequestRef.current = null
      }
      if (pendingValuesRequestRef.current) {
        if (pendingValuesRequestRef.current.timeoutId) {
          clearTimeout(pendingValuesRequestRef.current.timeoutId)
        }
        pendingValuesRequestRef.current.reject(new Error('Request cancelled'))
        pendingValuesRequestRef.current = null
      }
    }
  }, [deviceSerial, refreshTrigger, explicitNodeId, sendMessage])

  // Sync params state with context cache when it changes (for real-time updates)
  useEffect(() => {
    if (parameters.cached && !loading) {
      const processed = processParameters(parameters.cached)
      setParams(processed)
    }
  }, [parameters.cached, loading])

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
