/**
 * Parameter storage utility for managing device parameter definitions in localStorage
 * Parameters are stored per device (keyed by serial number)
 */

import type { Parameter as ApiParameter, ParameterList as ApiParameterList } from '../api/inverter'

// Re-export API types for consistency
export type Parameter = ApiParameter
export type ParameterList = ApiParameterList

const STORAGE_PREFIX = 'params_'
const STORAGE_VERSION = 1

interface StoredParams {
  version: number
  timestamp: number
  params: ParameterList
}

export const ParamStorage = {
  /**
   * Get parameter definitions for a device
   */
  getParams(serial: string): ParameterList | null {
    try {
      const key = `${STORAGE_PREFIX}${serial}`
      const data = localStorage.getItem(key)

      if (!data) return null

      const stored: StoredParams = JSON.parse(data)

      // Version check
      if (stored.version !== STORAGE_VERSION) {
        console.warn('Parameter storage version mismatch, clearing cache')
        this.clearParams(serial)
        return null
      }

      return stored.params
    } catch (error) {
      console.error('Failed to load parameters from localStorage:', error)
      return null
    }
  },

  /**
   * Save parameter definitions for a device
   */
  saveParams(serial: string, params: ParameterList): void {
    try {
      const key = `${STORAGE_PREFIX}${serial}`
      const stored: StoredParams = {
        version: STORAGE_VERSION,
        timestamp: Date.now(),
        params,
      }

      localStorage.setItem(key, JSON.stringify(stored))
      console.log(`Saved ${Object.keys(params).length} parameters for device ${serial}`)
    } catch (error) {
      console.error('Failed to save parameters to localStorage:', error)
      // Handle quota exceeded
      if (error instanceof DOMException && error.name === 'QuotaExceededError') {
        console.warn('localStorage quota exceeded, clearing old parameter caches')
        this.clearOldestCache()
        // Retry once
        try {
          localStorage.setItem(
            `${STORAGE_PREFIX}${serial}`,
            JSON.stringify({
              version: STORAGE_VERSION,
              timestamp: Date.now(),
              params,
            })
          )
        } catch (retryError) {
          console.error('Failed to save parameters even after cleanup:', retryError)
        }
      }
    }
  },

  /**
   * Get parameter ID by name
   */
  getParamId(serial: string, paramName: string): number | null {
    const params = this.getParams(serial)
    return params?.[paramName]?.id ?? null
  },

  /**
   * Get parameter definition by name
   */
  getParam(serial: string, paramName: string): Parameter | null {
    const params = this.getParams(serial)
    return params?.[paramName] ?? null
  },

  /**
   * Check if parameters are cached for a device
   */
  hasParams(serial: string): boolean {
    return this.getParams(serial) !== null
  },

  /**
   * Clear parameters for a specific device
   */
  clearParams(serial: string): void {
    const key = `${STORAGE_PREFIX}${serial}`
    localStorage.removeItem(key)
    console.log(`Cleared parameters for device ${serial}`)
  },

  /**
   * Clear the oldest parameter cache to free up space
   */
  clearOldestCache(): void {
    const keys: Array<{ key: string; timestamp: number }> = []

    // Find all parameter storage keys
    for (let i = 0; i < localStorage.length; i++) {
      const key = localStorage.key(i)
      if (key && key.startsWith(STORAGE_PREFIX)) {
        try {
          const data = localStorage.getItem(key)
          if (data) {
            const stored: StoredParams = JSON.parse(data)
            keys.push({ key, timestamp: stored.timestamp })
          }
        } catch (error) {
          // Invalid data, mark for removal
          keys.push({ key, timestamp: 0 })
        }
      }
    }

    // Sort by timestamp (oldest first)
    keys.sort((a, b) => a.timestamp - b.timestamp)

    // Remove oldest
    if (keys.length > 0) {
      localStorage.removeItem(keys[0].key)
      console.log(`Removed oldest parameter cache: ${keys[0].key}`)
    }
  },

  /**
   * Get all cached device serials
   */
  getCachedDevices(): string[] {
    const serials: string[] = []

    for (let i = 0; i < localStorage.length; i++) {
      const key = localStorage.key(i)
      if (key && key.startsWith(STORAGE_PREFIX)) {
        serials.push(key.replace(STORAGE_PREFIX, ''))
      }
    }

    return serials
  },

  /**
   * Get cache info (size, timestamp) for a device
   */
  getCacheInfo(serial: string): { size: number; timestamp: number; paramCount: number } | null {
    const key = `${STORAGE_PREFIX}${serial}`
    const data = localStorage.getItem(key)

    if (!data) return null

    try {
      const stored: StoredParams = JSON.parse(data)
      return {
        size: new Blob([data]).size,
        timestamp: stored.timestamp,
        paramCount: Object.keys(stored.params).length,
      }
    } catch (error) {
      return null
    }
  },
}

/**
 * Get the display-friendly name for a parameter
 * Falls back to the parameter key if name is not provided
 *
 * @param key - The parameter key
 * @param param - The parameter object
 * @returns The display name
 *
 * @example
 * // With name field
 * getParameterDisplayName('udc', { name: 'DC Voltage', value: 350 })
 * // Returns: 'DC Voltage'
 *
 * @example
 * // Without name field
 * getParameterDisplayName('udc', { value: 350 })
 * // Returns: 'udc'
 */
export function getParameterDisplayName(key: string, param: Parameter): string {
  return param.name || key
}
