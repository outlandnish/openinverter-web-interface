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
}

/**
 * Hook for managing device parameters with localStorage caching
 */
export function useParams(deviceSerial: string | undefined): UseParamsResult {
  const [params, setParams] = useState<ParameterList | null>(null)
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)

  const loadParams = async (forceRefresh = false) => {
    if (!deviceSerial) {
      setParams(null)
      setLoading(false)
      return
    }

    try {
      setLoading(true)
      setError(null)

      // Check localStorage first
      if (!forceRefresh) {
        const cached = ParamStorage.getParams(deviceSerial)
        if (cached) {
          console.log('Using cached parameters from localStorage')
          // Process cached parameters to ensure enums are parsed
          const processedCached = processParameters(cached)
          setParams(processedCached)
          setLoading(false)
          return
        }
      }

      // Download from device
      console.log('Downloading parameters from device...')
      const rawParams = await api.getParamsJson()

      // Process parameters to parse enums from unit strings
      const processedParams = processParameters(rawParams)

      // Save to localStorage
      ParamStorage.saveParams(deviceSerial, processedParams)

      setParams(processedParams)
    } catch (err) {
      console.error('Failed to load parameters:', err)
      setError(err instanceof Error ? err.message : 'Failed to load parameters')
    } finally {
      setLoading(false)
    }
  }

  const refresh = async () => {
    await loadParams(true)
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
    loadParams()
  }, [deviceSerial])

  return {
    params,
    loading,
    error,
    refresh,
    getParamId,
    getDisplayName,
  }
}
