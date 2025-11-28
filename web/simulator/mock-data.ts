/**
 * Mock data for device simulator
 */

export interface MockParameter {
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

export type MockParameterList = Record<string, MockParameter>

// Mock parameters that simulate a typical inverter
export const mockParameters: MockParameterList = {
  opmode: {
    value: 0,
    name: 'Operating Mode',
    unit: '',
    minimum: 0,
    maximum: 3,
    default: 0,
    isparam: true,
    category: 'System',
    enums: { '0': 'Off', '1': 'Run', '2': 'ManualRun', '3': 'Boost' },
    id: 1,
    i: 0,
  },
  version: {
    value: '5.24.R',
    name: 'Version',
    unit: '',
    isparam: false,
    category: 'System',
    id: 2,
    i: 1,
  },
  udcinv: {
    value: 350.5,
    name: 'DC Link Voltage',
    unit: 'V',
    minimum: 0,
    maximum: 1000,
    isparam: false,
    category: 'Measurements',
    id: 10,
    i: 2,
  },
  idc: {
    value: 12.3,
    name: 'DC Current',
    unit: 'A',
    minimum: 0,
    maximum: 500,
    isparam: false,
    category: 'Measurements',
    id: 11,
    i: 3,
  },
  power: {
    value: 4321.5,
    name: 'Power',
    unit: 'W',
    minimum: 0,
    maximum: 100000,
    isparam: false,
    category: 'Measurements',
    id: 12,
    i: 4,
  },
  speed: {
    value: 3456,
    name: 'Motor Speed',
    unit: 'rpm',
    minimum: 0,
    maximum: 10000,
    isparam: false,
    category: 'Measurements',
    id: 13,
    i: 5,
  },
  tmphs: {
    value: 45.2,
    name: 'Heatsink Temperature',
    unit: '°C',
    minimum: -40,
    maximum: 150,
    isparam: false,
    category: 'Measurements',
    id: 14,
    i: 6,
  },
  tmpm: {
    value: 38.7,
    name: 'Motor Temperature',
    unit: '°C',
    minimum: -40,
    maximum: 150,
    isparam: false,
    category: 'Measurements',
    id: 15,
    i: 7,
  },
  uac: {
    value: 230.2,
    name: 'AC Voltage',
    unit: 'V',
    minimum: 0,
    maximum: 500,
    isparam: false,
    category: 'Measurements',
    id: 16,
    i: 8,
  },
  boost: {
    value: 1700,
    name: 'Boost',
    unit: 'dig',
    minimum: 0,
    maximum: 4095,
    default: 1700,
    isparam: true,
    category: 'Motor',
    id: 20,
    i: 9,
  },
  fweak: {
    value: 80,
    name: 'Field Weakening',
    unit: 'dig',
    minimum: 0,
    maximum: 100,
    default: 80,
    isparam: true,
    category: 'Motor',
    id: 21,
    i: 10,
  },
  udcnom: {
    value: 360,
    name: 'Nominal DC Voltage',
    unit: 'V',
    minimum: 0,
    maximum: 1000,
    default: 360,
    isparam: true,
    category: 'System',
    id: 30,
    i: 11,
  },
  udcmin: {
    value: 300,
    name: 'Minimum DC Voltage',
    unit: 'V',
    minimum: 0,
    maximum: 1000,
    default: 300,
    isparam: true,
    category: 'System',
    id: 31,
    i: 12,
  },
  udcmax: {
    value: 420,
    name: 'Maximum DC Voltage',
    unit: 'V',
    minimum: 0,
    maximum: 1000,
    default: 420,
    isparam: true,
    category: 'System',
    id: 32,
    i: 13,
  },
}

export interface MockDevice {
  nodeId: number
  serial: string
  name?: string
  lastSeen: number
}

// Mock CAN devices that can be discovered
export const mockDevices: MockDevice[] = [
  { nodeId: 1, serial: 'OI-INV-001234', name: 'Main Inverter', lastSeen: Date.now() },
  { nodeId: 2, serial: 'OI-CHG-005678', name: 'Charger', lastSeen: Date.now() },
  { nodeId: 3, serial: 'OI-BMS-009012', lastSeen: Date.now() },
]

export interface MockCanMapping {
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

// Mock CAN mappings
export const mockCanMappings: MockCanMapping[] = [
  {
    isrx: false,
    paramid: 10, // udcinv
    id: 0x521,
    position: 0,
    length: 2,
    gain: 10,
    offset: 0,
    index: 0,
  },
  {
    isrx: false,
    paramid: 13, // speed
    id: 0x521,
    position: 16,
    length: 2,
    gain: 1,
    offset: 0,
    index: 1,
  },
  {
    isrx: true,
    paramid: 20, // boost
    id: 0x601,
    position: 0,
    length: 2,
    gain: 1,
    offset: 0,
    index: 2,
  },
]

// Simulate realistic value changes over time
export function updateMockParameters() {
  // Simulate voltage fluctuation
  const currentVoltage = mockParameters.udcinv.value as number
  mockParameters.udcinv.value = currentVoltage + (Math.random() - 0.5) * 2

  // Simulate current fluctuation
  const currentCurrent = mockParameters.idc.value as number
  mockParameters.idc.value = Math.max(0, currentCurrent + (Math.random() - 0.5) * 1)

  // Update power based on voltage and current
  mockParameters.power.value = (mockParameters.udcinv.value as number) * (mockParameters.idc.value as number)

  // Simulate speed changes
  const currentSpeed = mockParameters.speed.value as number
  mockParameters.speed.value = Math.max(0, currentSpeed + (Math.random() - 0.5) * 50)

  // Simulate temperature drift
  const currentTempHs = mockParameters.tmphs.value as number
  mockParameters.tmphs.value = Math.max(20, Math.min(80, currentTempHs + (Math.random() - 0.5) * 0.5))

  const currentTempM = mockParameters.tmpm.value as number
  mockParameters.tmpm.value = Math.max(20, Math.min(70, currentTempM + (Math.random() - 0.5) * 0.3))
}
