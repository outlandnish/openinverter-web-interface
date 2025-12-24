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

  // Track pending WebSocket request
  const pendingRequestRef = useRef<{
    nodeId: number
    resolve: (params: ParameterList) => void
    reject: (error: Error) => void
  } | null>(null)

  const refresh = async () => {
    setRefreshTrigger(prev => prev + 1)
  }

  // Request params via WebSocket and return a promise
  const requestParamsViaWebSocket = (nodeId: number): Promise<ParameterList> => {
    return new Promise((resolve, reject) => {
      // Store the pending request
      pendingRequestRef.current = { nodeId, resolve, reject }

      // Send WebSocket message
      sendMessage('getParams', { nodeId })

      // Set a timeout in case we don't get a response
      setTimeout(() => {
        if (pendingRequestRef.current && pendingRequestRef.current.nodeId === nodeId) {
          pendingRequestRef.current.reject(new Error('Request timeout'))
          pendingRequestRef.current = null
        }
      }, 30000) // 30 second timeout
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

      // Handle params data response
      else if (message.event === 'paramsData') {
        const nodeId = message.data.nodeId
        const params = message.data.params

        // Check if this response is for a pending request
        if (pendingRequestRef.current && pendingRequestRef.current.nodeId === nodeId) {
          console.log('Received params data via WebSocket for nodeId:', nodeId)
          pendingRequestRef.current.resolve(params)
          pendingRequestRef.current = null
        }
      }

      // Handle params error response
      else if (message.event === 'paramsError') {
        const nodeId = message.data.nodeId
        const error = message.data.error

        // Check if this error is for a pending request
        if (pendingRequestRef.current && pendingRequestRef.current.nodeId === nodeId) {
          console.error('Params error via WebSocket:', error)
          pendingRequestRef.current.reject(new Error(error))
          pendingRequestRef.current = null
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
      if (!forceRefresh && loadedSerialRef.current === deviceSerial && params !== null) {
        console.log('Params already loaded for device:', deviceSerial, '- skipping redundant load')
        if (!abortController.signal.aborted) {
          setLoading(false)
          setError(null)
        }
        return
      }

      // Check cache first BEFORE setting loading state
      // This prevents stale data from previous device and avoids loading flicker
      if (!forceRefresh) {
        const cached = ParamStorage.getParams(deviceSerial)
        if (cached) {
          console.log('Using cached parameters from localStorage for device:', deviceSerial)
          // Process cached parameters to ensure enums are parsed
          const processedCached = processParameters(cached)

          // Check if aborted before setting state
          if (!abortController.signal.aborted) {
            setParams(processedCached)
            setLoading(false)
            setError(null)
            loadedSerialRef.current = deviceSerial
          }
          return
        }
      }

      // No cache available - need to download
      // Clear stale params from previous device to avoid confusion
      try {
        // Download from device - nodeId is required
        if (explicitNodeId === undefined) {
          // Clear params and show we're waiting for nodeId
          if (!abortController.signal.aborted) {
            setParams(null)
            setLoading(false)
            setError(null)
          }
          console.log('Waiting for nodeId to download parameters for device:', deviceSerial)
          return
        }

        // Start loading - clear stale params
        if (!abortController.signal.aborted) {
          setParams(null)
          setLoading(true)
          setError(null)
        }

        console.log(`Downloading parameters from nodeId ${explicitNodeId} for serial:`, deviceSerial)
        setDownloadProgress(0)
        setDownloadTotal(0) // Reset for new download

        // Request params via WebSocket - progress will be tracked via WebSocket jsonProgress events
        let rawParams: ParameterList
        try {
          rawParams = await requestParamsViaWebSocket(explicitNodeId)

          // Check if request was aborted while waiting
          if (abortController.signal.aborted) {
            console.log('Parameter fetch aborted for device:', deviceSerial)
            return
          }
        } catch (err) {
          // Clear pending request on abort
          if (abortController.signal.aborted) {
            pendingRequestRef.current = null
            console.log('Parameter fetch was aborted')
            return
          }
          throw err // Re-throw if not aborted
        }

        // Process parameters to parse enums from unit strings
        const processedParams = processParameters(rawParams)

        // Save to localStorage
        ParamStorage.saveParams(deviceSerial, processedParams)

        setParams(processedParams)
        loadedSerialRef.current = deviceSerial
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
      // Clear any pending WebSocket request
      if (pendingRequestRef.current) {
        pendingRequestRef.current.reject(new Error('Request cancelled'))
        pendingRequestRef.current = null
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
