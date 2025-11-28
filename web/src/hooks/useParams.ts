import { useState, useEffect } from 'preact/hooks'
import { api } from '../api/inverter'
import { ParamStorage, ParameterList, getParameterDisplayName } from '../utils/paramStorage'

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
          setParams(cached)
          setLoading(false)
          return
        }
      }

      // Download from device
      console.log('Downloading parameters from device...')
      const deviceParams = await api.getParamsJson()

      // Save to localStorage
      ParamStorage.saveParams(deviceSerial, deviceParams)

      setParams(deviceParams)
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
