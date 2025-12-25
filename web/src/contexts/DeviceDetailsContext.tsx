import { createContext, ComponentChildren } from 'preact'
import { useContext, useState, useCallback } from 'preact/hooks'

// Types for monitoring state
export interface HistoricalDataPoint {
  timestamp: number
  value: number
}

export interface HistoricalData {
  [paramId: string]: HistoricalDataPoint[]
}

// Types for CAN message state
export interface PeriodicMessage {
  id: string
  canId: number
  data: number[]
  interval: number
  active: boolean
}

export interface PeriodicFormData {
  canId: string
  data: string
  interval: number
}

// Context value interface
export interface DeviceDetailsContextValue {
  // Monitoring state
  monitoring: {
    streaming: boolean
    interval: number
    spotValues: Record<string, number>
    historicalData: HistoricalData
    selectedParams: Set<string>
    chartParams: Set<string>
    viewMode: 'table' | 'chart'
  }

  // CAN message state
  canMessages: {
    canId: string
    dataBytes: string
    periodicMessages: PeriodicMessage[]
    showAddPeriodicForm: boolean
    periodicFormData: PeriodicFormData
  }

  // Monitoring actions
  setStreaming: (streaming: boolean) => void
  setInterval: (interval: number) => void
  setSpotValues: (values: Record<string, number>) => void
  updateSpotValue: (paramId: string, value: number) => void
  setHistoricalData: (data: HistoricalData | ((prev: HistoricalData) => HistoricalData)) => void
  addHistoricalDataPoint: (paramId: string, value: number) => void
  clearHistoricalData: () => void
  setSelectedParams: (params: Set<string> | ((prev: Set<string>) => Set<string>)) => void
  toggleSelectedParam: (paramId: string) => void
  setChartParams: (params: Set<string> | ((prev: Set<string>) => Set<string>)) => void
  toggleChartParam: (paramId: string) => void
  setViewMode: (mode: 'table' | 'chart') => void

  // CAN message actions
  setCanId: (canId: string) => void
  setDataBytes: (dataBytes: string) => void
  addPeriodicMessage: (message: PeriodicMessage) => void
  removePeriodicMessage: (id: string) => void
  togglePeriodicMessage: (id: string, active: boolean) => void
  updatePeriodicMessage: (id: string, updates: Partial<PeriodicMessage>) => void
  setShowAddPeriodicForm: (show: boolean) => void
  setPeriodicFormData: (data: PeriodicFormData) => void
  resetPeriodicFormData: () => void
}

const DeviceDetailsContext = createContext<DeviceDetailsContextValue | null>(null)

interface DeviceDetailsProviderProps {
  children: ComponentChildren
}

const DEFAULT_PERIODIC_FORM_DATA: PeriodicFormData = {
  canId: '0x180',
  data: '00 00 00 00 00 00 00 00',
  interval: 100,
}

export function DeviceDetailsProvider({ children }: DeviceDetailsProviderProps) {
  // Monitoring state
  const [streaming, setStreaming] = useState(false)
  const [interval, setInterval] = useState(1000)
  const [spotValues, setSpotValues] = useState<Record<string, number>>({})
  const [historicalData, setHistoricalData] = useState<HistoricalData>({})
  const [selectedParams, setSelectedParams] = useState<Set<string>>(new Set())
  const [chartParams, setChartParams] = useState<Set<string>>(new Set())
  const [viewMode, setViewMode] = useState<'table' | 'chart'>('table')

  // CAN message state
  const [canId, setCanId] = useState<string>('0x180')
  const [dataBytes, setDataBytes] = useState<string>('00 00 00 00 00 00 00 00')
  const [periodicMessages, setPeriodicMessages] = useState<PeriodicMessage[]>([])
  const [showAddPeriodicForm, setShowAddPeriodicForm] = useState(false)
  const [periodicFormData, setPeriodicFormData] = useState<PeriodicFormData>(DEFAULT_PERIODIC_FORM_DATA)

  // Monitoring actions
  const updateSpotValue = useCallback((paramId: string, value: number) => {
    setSpotValues(prev => ({
      ...prev,
      [paramId]: value
    }))
  }, [])

  const addHistoricalDataPoint = useCallback((paramId: string, value: number) => {
    const timestamp = Date.now()
    setHistoricalData(prev => ({
      ...prev,
      [paramId]: [
        ...(prev[paramId] || []),
        { timestamp, value }
      ]
    }))
  }, [])

  const clearHistoricalData = useCallback(() => {
    setHistoricalData({})
  }, [])

  const toggleSelectedParam = useCallback((paramId: string) => {
    setSelectedParams(prev => {
      const newSet = new Set(prev)
      if (newSet.has(paramId)) {
        newSet.delete(paramId)
      } else {
        newSet.add(paramId)
      }
      return newSet
    })
  }, [])

  const toggleChartParam = useCallback((paramId: string) => {
    setChartParams(prev => {
      const newSet = new Set(prev)
      if (newSet.has(paramId)) {
        newSet.delete(paramId)
      } else {
        newSet.add(paramId)
      }
      return newSet
    })
  }, [])

  // CAN message actions
  const addPeriodicMessage = useCallback((message: PeriodicMessage) => {
    setPeriodicMessages(prev => [...prev, message])
  }, [])

  const removePeriodicMessage = useCallback((id: string) => {
    setPeriodicMessages(prev => prev.filter(msg => msg.id !== id))
  }, [])

  const togglePeriodicMessage = useCallback((id: string, active: boolean) => {
    setPeriodicMessages(prev =>
      prev.map(msg => msg.id === id ? { ...msg, active } : msg)
    )
  }, [])

  const updatePeriodicMessage = useCallback((id: string, updates: Partial<PeriodicMessage>) => {
    setPeriodicMessages(prev =>
      prev.map(msg => msg.id === id ? { ...msg, ...updates } : msg)
    )
  }, [])

  const resetPeriodicFormData = useCallback(() => {
    setPeriodicFormData(DEFAULT_PERIODIC_FORM_DATA)
  }, [])

  const value: DeviceDetailsContextValue = {
    monitoring: {
      streaming,
      interval,
      spotValues,
      historicalData,
      selectedParams,
      chartParams,
      viewMode,
    },
    canMessages: {
      canId,
      dataBytes,
      periodicMessages,
      showAddPeriodicForm,
      periodicFormData,
    },
    setStreaming,
    setInterval,
    setSpotValues,
    updateSpotValue,
    setHistoricalData,
    addHistoricalDataPoint,
    clearHistoricalData,
    setSelectedParams,
    toggleSelectedParam,
    setChartParams,
    toggleChartParam,
    setViewMode,
    setCanId,
    setDataBytes,
    addPeriodicMessage,
    removePeriodicMessage,
    togglePeriodicMessage,
    updatePeriodicMessage,
    setShowAddPeriodicForm,
    setPeriodicFormData,
    resetPeriodicFormData,
  }

  return (
    <DeviceDetailsContext.Provider value={value}>
      {children}
    </DeviceDetailsContext.Provider>
  )
}

export function useDeviceDetailsContext() {
  const context = useContext(DeviceDetailsContext)
  if (!context) {
    throw new Error('useDeviceDetailsContext must be used within a DeviceDetailsProvider')
  }
  return context
}
