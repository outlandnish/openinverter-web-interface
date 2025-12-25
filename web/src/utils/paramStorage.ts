/**
 * Parameter storage utility for managing device parameter definitions in localStorage
 * Parameters are stored per device (keyed by serial number)
 */

import type { Parameter as ApiParameter, ParameterList as ApiParameterList } from '../api/inverter'

// Re-export API types for consistency
export type Parameter = ApiParameter
export type ParameterList = ApiParameterList

const SCHEMA_PREFIX = 'schema_'
const STORAGE_VERSION = 2 // v2: Separated schema from values

interface StoredSchema {
  version: number
  timestamp: number
  schema: ParameterList
}

export const ParamStorage = {

  /**
   * Get parameter schema (definitions without values) for a device
   */
  getSchema(serial: string): ParameterList | null {
    try {
      const key = `${SCHEMA_PREFIX}${serial}`
      const data = localStorage.getItem(key)

      if (!data) return null

      const stored: StoredSchema = JSON.parse(data)

      // Version check
      if (stored.version !== STORAGE_VERSION) {
        console.warn('Schema storage version mismatch, clearing cache')
        this.clearSchema(serial)
        return null
      }

      return stored.schema
    } catch (error) {
      console.error('Failed to load schema from localStorage:', error)
      return null
    }
  },

  /**
   * Save parameter schema (definitions without values) for a device
   */
  saveSchema(serial: string, schema: ParameterList): void {
    try {
      const key = `${SCHEMA_PREFIX}${serial}`
      const stored: StoredSchema = {
        version: STORAGE_VERSION,
        timestamp: Date.now(),
        schema,
      }

      localStorage.setItem(key, JSON.stringify(stored))
      console.log(`Saved schema with ${Object.keys(schema).length} parameters for device ${serial}`)
    } catch (error) {
      console.error('Failed to save schema to localStorage:', error)
      // Handle quota exceeded
      if (error instanceof DOMException && error.name === 'QuotaExceededError') {
        console.warn('localStorage quota exceeded, clearing old schema caches')
        this.clearOldestSchemaCache()
        // Retry once
        try {
          localStorage.setItem(
            `${SCHEMA_PREFIX}${serial}`,
            JSON.stringify({
              version: STORAGE_VERSION,
              timestamp: Date.now(),
              schema,
            })
          )
        } catch (retryError) {
          console.error('Failed to save schema even after cleanup:', retryError)
        }
      }
    }
  },

  /**
   * Check if schema is cached for a device
   */
  hasSchema(serial: string): boolean {
    return this.getSchema(serial) !== null
  },

  /**
   * Clear schema for a specific device
   */
  clearSchema(serial: string): void {
    const key = `${SCHEMA_PREFIX}${serial}`
    localStorage.removeItem(key)
    console.log(`Cleared schema for device ${serial}`)
  },

  /**
   * Clear the oldest schema cache to free up space
   */
  clearOldestSchemaCache(): void {
    const keys: Array<{ key: string; timestamp: number }> = []

    // Find all schema storage keys
    for (let i = 0; i < localStorage.length; i++) {
      const key = localStorage.key(i)
      if (key && key.startsWith(SCHEMA_PREFIX)) {
        try {
          const data = localStorage.getItem(key)
          if (data) {
            const stored: StoredSchema = JSON.parse(data)
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
      console.log(`Removed oldest schema cache: ${keys[0].key}`)
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
