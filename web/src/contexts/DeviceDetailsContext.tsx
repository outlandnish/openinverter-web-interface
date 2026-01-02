import { createContext, ComponentChildren } from 'preact'
import { useContext, useState, useCallback } from 'preact/hooks'
import { ParameterList } from '../utils/paramStorage'

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

// Types for CAN IO Control state
export interface CanIoState {
  active: boolean
  canId: string
  interval: number
  cruise: boolean
  start: boolean
  brake: boolean
  forward: boolean
  reverse: boolean
  bms: boolean
  throttlePercent: number
  cruisespeed: number
  regenpreset: number
  useCrc: boolean
}

// Context value interface
export interface DeviceDetailsContextValue {
  // Parameter cache (persists between tab changes during a session)
  parameters: {
    cached: ParameterList | null
    lastFetchTime: number | null
  }

  // Monitoring state
  monitoring: {
    streaming: boolean
    interval: number
    spotValues: Record<string, number>
    historicalData: HistoricalData
    selectedParams: Set<string>
    chartParams: Set<string>
    viewMode: 'table' | 'chart'
    connectedSerial: string | null
  }

  // CAN message state
  canMessages: {
    canId: string
    dataBytes: string
    periodicMessages: PeriodicMessage[]
    showAddPeriodicForm: boolean
    periodicFormData: PeriodicFormData
  }

  // CAN IO state
  canIo: CanIoState

  // Monitoring actions
  setStreaming: (streaming: boolean) => void
  setInterval: (interval: number) => void
  setSpotValues: (values: Record<string, number>) => void
  mergeSpotValues: (values: Record<string, number>) => void
  updateSpotValue: (paramId: string, value: number) => void
  setHistoricalData: (data: HistoricalData | ((prev: HistoricalData) => HistoricalData)) => void
  addHistoricalDataPoint: (paramId: string, value: number) => void
  clearHistoricalData: () => void
  setSelectedParams: (params: Set<string> | ((prev: Set<string>) => Set<string>)) => void
  toggleSelectedParam: (paramId: string) => void
  setChartParams: (params: Set<string> | ((prev: Set<string>) => Set<string>)) => void
  toggleChartParam: (paramId: string) => void
  setViewMode: (mode: 'table' | 'chart') => void
  setConnectedSerial: (serial: string | null) => void

  // Parameter actions
  setCachedParameters: (params: ParameterList | null) => void
  updateParameterValue: (paramName: string, value: number) => void
  clearParameterCache: () => void

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

  // CAN IO actions
  setCanIoActive: (active: boolean) => void
  setCanIoCanId: (canId: string) => void
  setCanIoInterval: (interval: number) => void
  setCanIoCruise: (cruise: boolean) => void
  setCanIoStart: (start: boolean) => void
  setCanIoBrake: (brake: boolean) => void
  setCanIoForward: (forward: boolean) => void
  setCanIoReverse: (reverse: boolean) => void
  setCanIoBms: (bms: boolean) => void
  setCanIoThrottlePercent: (throttlePercent: number) => void
  setCanIoCruisespeed: (cruisespeed: number) => void
  setCanIoRegenpreset: (regenpreset: number) => void
  setCanIoUseCrc: (useCrc: boolean) => void
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
  // Parameter cache state (persists during session, cleared on disconnect)
  const [cachedParameters, setCachedParameters] = useState<ParameterList | null>(null)
  const [lastFetchTime, setLastFetchTime] = useState<number | null>(null)

  // Monitoring state
  const [streaming, setStreaming] = useState(false)
  const [interval, setInterval] = useState(1000)
  const [spotValues, setSpotValues] = useState<Record<string, number>>({})
  const [historicalData, setHistoricalData] = useState<HistoricalData>({})
  const [selectedParams, setSelectedParams] = useState<Set<string>>(new Set())
  const [chartParams, setChartParams] = useState<Set<string>>(new Set())
  const [viewMode, setViewMode] = useState<'table' | 'chart'>('table')
  const [connectedSerial, setConnectedSerial] = useState<string | null>(null)

  // CAN message state
  const [canId, setCanId] = useState<string>('0x180')
  const [dataBytes, setDataBytes] = useState<string>('00 00 00 00 00 00 00 00')
  const [periodicMessages, setPeriodicMessages] = useState<PeriodicMessage[]>([])
  const [showAddPeriodicForm, setShowAddPeriodicForm] = useState(false)
  const [periodicFormData, setPeriodicFormData] = useState<PeriodicFormData>(DEFAULT_PERIODIC_FORM_DATA)

  // CAN IO state
  const [canIoActive, setCanIoActive] = useState(false)
  const [canIoCanId, setCanIoCanId] = useState('3F')
  const [canIoInterval, setCanIoInterval] = useState(100)
  const [canIoCruise, setCanIoCruise] = useState(false)
  const [canIoStart, setCanIoStart] = useState(false)
  const [canIoBrake, setCanIoBrake] = useState(false)
  const [canIoForward, setCanIoForward] = useState(false)
  const [canIoReverse, setCanIoReverse] = useState(false)
  const [canIoBms, setCanIoBms] = useState(false)
  const [canIoThrottlePercent, setCanIoThrottlePercent] = useState(0)
  const [canIoCruisespeed, setCanIoCruisespeed] = useState(0)
  const [canIoRegenpreset, setCanIoRegenpreset] = useState(0)
  const [canIoUseCrc, setCanIoUseCrc] = useState(false)

  // Monitoring actions - merge new values with existing ones to preserve values not in current batch
  const mergeSpotValues = useCallback((values: Record<string, number>) => {
    setSpotValues(prev => ({
      ...prev,
      ...values
    }))
  }, [])

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

  // Parameter actions
  const handleSetCachedParameters = useCallback((params: ParameterList | null) => {
    setCachedParameters(params)
    setLastFetchTime(params ? Date.now() : null)
  }, [])

  const updateParameterValue = useCallback((paramName: string, value: number) => {
    setCachedParameters(prev => {
      if (!prev || !prev[paramName]) return prev
      return {
        ...prev,
        [paramName]: {
          ...prev[paramName],
          value
        }
      }
    })
  }, [])

  const clearParameterCache = useCallback(() => {
    setCachedParameters(null)
    setLastFetchTime(null)
  }, [])

  const value: DeviceDetailsContextValue = {
    parameters: {
      cached: cachedParameters,
      lastFetchTime,
    },
    monitoring: {
      streaming,
      interval,
      spotValues,
      historicalData,
      selectedParams,
      chartParams,
      viewMode,
      connectedSerial,
    },
    canMessages: {
      canId,
      dataBytes,
      periodicMessages,
      showAddPeriodicForm,
      periodicFormData,
    },
    canIo: {
      active: canIoActive,
      canId: canIoCanId,
      interval: canIoInterval,
      cruise: canIoCruise,
      start: canIoStart,
      brake: canIoBrake,
      forward: canIoForward,
      reverse: canIoReverse,
      bms: canIoBms,
      throttlePercent: canIoThrottlePercent,
      cruisespeed: canIoCruisespeed,
      regenpreset: canIoRegenpreset,
      useCrc: canIoUseCrc,
    },
    setStreaming,
    setInterval,
    setSpotValues,
    mergeSpotValues,
    updateSpotValue,
    setHistoricalData,
    addHistoricalDataPoint,
    clearHistoricalData,
    setSelectedParams,
    toggleSelectedParam,
    setChartParams,
    toggleChartParam,
    setViewMode,
    setConnectedSerial,
    setCachedParameters: handleSetCachedParameters,
    updateParameterValue,
    clearParameterCache,
    setCanId,
    setDataBytes,
    addPeriodicMessage,
    removePeriodicMessage,
    togglePeriodicMessage,
    updatePeriodicMessage,
    setShowAddPeriodicForm,
    setPeriodicFormData,
    resetPeriodicFormData,
    setCanIoActive,
    setCanIoCanId,
    setCanIoInterval,
    setCanIoCruise,
    setCanIoStart,
    setCanIoBrake,
    setCanIoForward,
    setCanIoReverse,
    setCanIoBms,
    setCanIoThrottlePercent,
    setCanIoCruisespeed,
    setCanIoRegenpreset,
    setCanIoUseCrc,
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
