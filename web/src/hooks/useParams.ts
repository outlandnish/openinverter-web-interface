import { useState, useEffect } from 'preact/hooks'
import { api } from '../api/inverter'
import { ParamStorage, ParameterList, getParameterDisplayName } from '../utils/paramStorage'

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
  downloadProgress: number // 0-100 percentage of download complete
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

  const refresh = async () => {
    setRefreshTrigger(prev => prev + 1)
  }

  const getParamId = (paramName: string): number | null => {
    if (!deviceSerial || !params) return null
    return params[paramName]?.id ?? null
  }

  const getDisplayName = (paramName: string): string => {
    if (!params || !params[paramName]) return paramName
    return getParameterDisplayName(paramName, params[paramName])
  }

  useEffect(() => {
    // Create AbortController for this effect
    const abortController = new AbortController()
    const forceRefresh = refreshTrigger > 0

    const loadParams = async () => {
      if (!deviceSerial) {
        setParams(null)
        setLoading(false)
        return
      }

      try {
        setLoading(true)
        setError(null)

        // Check cache first (unless force refresh is requested)
        // Cache is keyed by deviceSerial, so it's safe to use cached params
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
            }
            return
          }
        }

        // Download from device - nodeId is required
        if (explicitNodeId === undefined) {
          console.error('Cannot download parameters: nodeId is required')
          setError('nodeId is required to fetch parameters')
          return
        }
        
        console.log(`Downloading parameters from nodeId ${explicitNodeId} for serial:`, deviceSerial)
        setDownloadProgress(0)
        
        const rawParams = await api.getParamsJson(explicitNodeId, abortController.signal, (progress) => {
          setDownloadProgress(progress)
        })

        // Check if request was aborted
        if (abortController.signal.aborted) {
          console.log('Parameter fetch aborted for device:', deviceSerial)
          return
        }

        // Process parameters to parse enums from unit strings
        const processedParams = processParameters(rawParams)

        // Save to localStorage
        ParamStorage.saveParams(deviceSerial, processedParams)

        setParams(processedParams)
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
        }
      }
    }

    loadParams()

    // Cleanup: abort the request when component unmounts or deviceSerial/refreshTrigger/nodeId changes
    return () => {
      abortController.abort()
    }
  }, [deviceSerial, refreshTrigger, explicitNodeId])

  return {
    params,
    loading,
    error,
    refresh,
    getParamId,
    getDisplayName,
    downloadProgress,
  }
}
