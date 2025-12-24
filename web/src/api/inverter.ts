/**
 * API client for ESP32 Inverter
 * All endpoints proxy to http://inverter.local during development
 */

// Type definitions
export interface CanMapping {
  isrx: boolean
  paramid: number
  id: number
  position: number
  length: number
  gain: number
  offset: number
  index?: number
  subindex?: number
}

export interface FileInfo {
  type: 'file' | 'dir'
  name: string
}

/**
 * Parameter definition and value
 *
 * @property value - Current parameter value
 * @property name - Display-friendly name for the parameter (optional, falls back to key)
 * @property unit - Unit of measurement (e.g., 'V', 'A', 'rpm')
 * @property minimum - Minimum allowed value
 * @property maximum - Maximum allowed value
 * @property default - Default value
 * @property isparam - Whether this is a user-configurable parameter
 * @property category - Category for grouping parameters
 * @property enums - Enumeration values for discrete parameters
 * @property id - Unique parameter ID
 * @property i - Parameter index
 */
export interface Parameter {
  value: number | string
  name?: string
  unit?: string
  minimum?: number
  maximum?: number
  default?: number
  isparam?: boolean
  category?: string
  enums?: Record<string, string>
  id?: number
  i?: number
}

export type ParameterList = Record<string, Parameter>

class InverterAPI {
  // Store WebSocket sendMessage function reference
  private wsSendMessage: ((action: string, data?: any) => void) | null = null

  /**
   * Register WebSocket sendMessage function for parameter updates
   */
  setWebSocketSender(sendMessage: (action: string, data?: any) => void) {
    this.wsSendMessage = sendMessage
  }

  /**
   * Send a command to the inverter
   */
  async sendCommand(cmd: string): Promise<string> {
    const response = await fetch(`/cmd?cmd=${encodeURIComponent(cmd)}`)
    return response.text()
  }

  /**
   * Get JSON parameter list with current values from inverter
   */
  async getParamList(): Promise<ParameterList> {
    const response = await fetch('/cmd?cmd=json')
    return response.json()
  }

  /**
   * Set a parameter value by ID (not name) via WebSocket
   */
  async setParamById(paramId: number, value: number | string): Promise<string> {
    if (!this.wsSendMessage) {
      throw new Error('WebSocket not initialized. Call setWebSocketSender() first.')
    }

    // Send via WebSocket - response will be handled by WebSocket event listeners
    this.wsSendMessage('updateParam', { paramId, value })

    // Return a success message (actual result will come through WebSocket events)
    return 'Parameter update requested'
  }

  /**
   * Get a parameter value by ID
   */
  async getParamById(paramId: number): Promise<number> {
    // Use the existing getParamList and extract the value
    // This is a fallback - in practice you might implement a dedicated endpoint
    const params = await this.getParamList()
    for (const key in params) {
      if (params[key].id === paramId) {
        return params[key].value as number
      }
    }
    return 0
  }

  /**
   * Save parameters to flash via WebSocket
   */
  async saveParams(): Promise<string> {
    if (!this.wsSendMessage) {
      throw new Error('WebSocket not initialized. Call setWebSocketSender() first.')
    }

    // Send via WebSocket - response will be handled by WebSocket event listeners
    this.wsSendMessage('saveToFlash', {})

    // Return a success message (actual result will come through WebSocket events)
    return 'Save to flash requested'
  }

  /**
   * Get CAN mappings
   */
  async getCanMapping(): Promise<CanMapping[]> {
    const response = await fetch('/canmap')
    return response.json()
  }

  /**
   * Add CAN mapping
   */
  async addCanMapping(mapping: Omit<CanMapping, 'index' | 'subindex'>): Promise<CanMapping[]> {
    const params = new URLSearchParams({ add: JSON.stringify(mapping) })
    const response = await fetch(`/canmap?${params}`)
    return response.json()
  }

  /**
   * Remove CAN mapping
   */
  async removeCanMapping(mapping: Partial<CanMapping>): Promise<CanMapping[]> {
    const params = new URLSearchParams({ remove: JSON.stringify(mapping) })
    const response = await fetch(`/canmap?${params}`)
    return response.json()
  }

  /**
   * Get file list from SPIFFS
   */
  async getFileList(): Promise<FileInfo[]> {
    const response = await fetch('/list')
    return response.json()
  }

  /**
   * Get WiFi settings
   */
  async getWifiSettings(): Promise<string> {
    const response = await fetch('/wifi')
    return response.text()
  }

  /**
   * Get device settings (returns HTML, needs parsing)
   */
  async getSettingsHtml(): Promise<string> {
    const response = await fetch('/settings')
    return response.text()
  }

  /**
   * Get device settings as JSON
   */
  async getSettings(): Promise<DeviceSettings> {
    const response = await fetch('/settings')
    return response.json()
  }

  /**
   * Update device settings
   */
  async updateSettings(settings: DeviceSettings): Promise<string> {
    const params = new URLSearchParams({
      canRXPin: settings.canRXPin.toString(),
      canTXPin: settings.canTXPin.toString(),
      canEnablePin: settings.canEnablePin.toString(),
      canSpeed: settings.canSpeed.toString(),
      scanStartNode: settings.scanStartNode.toString(),
      scanEndNode: settings.scanEndNode.toString(),
    })
    const response = await fetch(`/settings?${params}`)
    return response.text()
  }

  /**
   * Reload parameter JSON from device
   */
  async reloadJson(): Promise<string> {
    const response = await fetch('/reloadjson')
    return response.text()
  }

  /**
   * Reset the remote device
   */
  async resetDevice(): Promise<string> {
    const response = await fetch('/resetdevice')
    return response.text()
  }

  /**
   * Get firmware version
   */
  async getVersion(): Promise<string> {
    const response = await fetch('/version')
    return response.text()
  }

  // Device management methods

  /**
   * Scan for devices on the CAN bus
   */
  async scanDevices(start: number = 1, end: number = 32): Promise<ScannedDevice[]> {
    const params = new URLSearchParams({
      start: start.toString(),
      end: end.toString(),
    })
    const response = await fetch(`/scan?${params}`)
    return response.json()
  }

  /**
   * Get saved devices list
   */
  async getSavedDevices(): Promise<SavedDevicesResponse> {
    const response = await fetch('/devices')
    return response.json()
  }

}

// Device management type definitions
export interface ScannedDevice {
  nodeId: number
  serial: string
  lastSeen: number
}

export interface SavedDevice {
  name?: string
  nodeId?: number
  lastSeen?: number
}

export interface SavedDevicesResponse {
  devices: Record<string, SavedDevice> // Serial -> Device mapping
}

export interface DeviceSettings {
  canRXPin: number
  canTXPin: number
  canEnablePin: number
  canSpeed: number
  scanStartNode: number
  scanEndNode: number
}

export const api = new InverterAPI()
