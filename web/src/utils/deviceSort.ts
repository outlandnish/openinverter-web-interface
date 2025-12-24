/**
 * Device sorting utilities
 */

import type { MergedDevice } from '@contexts/DeviceContext'

/**
 * Sort devices by last seen timestamp in descending order (most recent first)
 */
export function sortDevicesByLastSeen(devices: MergedDevice[]): MergedDevice[] {
  return [...devices].sort((a, b) => {
    const aLastSeen = a.lastSeen || 0
    const bLastSeen = b.lastSeen || 0
    return bLastSeen - aLastSeen
  })
}
