/**
 * Utility for managing the last connected device in localStorage
 */

const LAST_DEVICE_KEY = 'oi_last_device'

export interface LastDevice {
  serial: string
  nodeId: number
  connectedAt: number
}

/**
 * Save the last connected device to localStorage
 */
export function saveLastDevice(serial: string, nodeId: number): void {
  try {
    const lastDevice: LastDevice = {
      serial,
      nodeId,
      connectedAt: Date.now()
    }
    localStorage.setItem(LAST_DEVICE_KEY, JSON.stringify(lastDevice))
  } catch (error) {
    console.error('Failed to save last device to localStorage:', error)
  }
}

/**
 * Get the last connected device from localStorage
 */
export function getLastDevice(): LastDevice | null {
  try {
    const data = localStorage.getItem(LAST_DEVICE_KEY)
    if (!data) return null

    const lastDevice = JSON.parse(data) as LastDevice
    return lastDevice
  } catch (error) {
    console.error('Failed to load last device from localStorage:', error)
    return null
  }
}

/**
 * Clear the last connected device from localStorage
 */
export function clearLastDevice(): void {
  try {
    localStorage.removeItem(LAST_DEVICE_KEY)
  } catch (error) {
    console.error('Failed to clear last device from localStorage:', error)
  }
}
