import { useEffect } from 'preact/hooks'
import { useIntlayer } from 'preact-intlayer'
import { useParams } from '@hooks/useParams'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { useDeviceDetailsContext } from '@contexts/DeviceDetailsContext'
import MultiLineChart from '@components/MultiLineChart'
import { LoadingSpinner } from '@components/LoadingSpinner'
import { convertSpotValue } from '@utils/spotValueConversions'
import { formatParameterValue } from '@utils/parameterDisplay'

const MAX_HISTORY_POINTS = 100

interface SpotValuesMonitorProps {
  serial: string
  nodeId: number  // Required: nodeId for explicit device parameter loading (multi-client support)
  showHeader?: boolean
  showBackButton?: boolean
  onBack?: () => void
}

/**
 * Normalize serial number for comparison by removing leading zeros from each segment
 * e.g., "00500040:32315110:34303539:34303539" becomes "500040:32315110:34303539:34303539"
 */
function normalizeSerial(serial: string | null | undefined): string {
  if (!serial) return ''
  return serial.split(':').map(segment => {
    // Remove leading zeros but keep at least one digit
    const normalized = segment.replace(/^0+/, '')
    return normalized || '0'
  }).join(':')
}

export default function SpotValuesMonitor({
  serial,
  nodeId,
  showHeader = true,
  showBackButton = false,
  onBack
}: SpotValuesMonitorProps) {
  const content = useIntlayer('spot-values')

  // Get state from context
  const {
    monitoring,
    setStreaming,
    setInterval,
    setSpotValues,
    setHistoricalData,
    clearHistoricalData,
    setSelectedParams,
  } = useDeviceDetailsContext()

  // Destructure monitoring state for easier access
  const { streaming, interval, spotValues, historicalData, selectedParams, connectedSerial } = monitoring

  // Load device parameters using explicit nodeId for multi-client support
  const { params, loading: paramsLoading, getDisplayName } = useParams(serial, nodeId)

  // WebSocket connection
  const { isConnected, sendMessage, subscribe } = useWebSocketContext()

  // Helper function to get consistent color for a parameter
  const getColorForParam = (key: string): string => {
    // Simple hash function to generate consistent RGB color
    let hash = 0
    for (let i = 0; i < key.length; i++) {
      hash = ((hash << 5) - hash) + key.charCodeAt(i)
      hash = hash & hash // Convert to 32-bit integer
    }

    // Generate RGB values from hash, ensuring vibrant, visible colors
    // Use different bit ranges for each color component
    const r = (hash & 0xFF0000) >>> 16
    const g = (hash & 0x00FF00) >>> 8
    const b = hash & 0x0000FF

    // Map values to range 60-200 to avoid colors that are too light or too dark
    const normalize = (val: number) => 60 + (val / 255) * 140

    // Ensure at least one component is in the darker range for contrast
    const values = [normalize(r), normalize(g), normalize(b)]

    // If all values are too high (too light), darken the lowest one
    if (values.every(v => v > 150)) {
      const minIndex = values.indexOf(Math.min(...values))
      values[minIndex] = Math.max(60, values[minIndex] - 80)
    }

    return `rgb(${Math.round(values[0])}, ${Math.round(values[1])}, ${Math.round(values[2])})`
  }

  // Subscribe to WebSocket messages
  useEffect(() => {
    const unsubscribe = subscribe((message) => {
      switch (message.event) {
        case 'spotValuesStatus':
          setStreaming(message.data.active)
          if (message.data.interval) {
            setInterval(message.data.interval)
          }
          // Clear historical data when stopping
          if (!message.data.active) {
            clearHistoricalData()
          }
          break

        case 'spotValues':
          // CRITICAL: Only process spot values if they're from the device we're monitoring
          // This prevents values from device A being interpreted with device B's parameter definitions
          if (normalizeSerial(connectedSerial) !== normalizeSerial(serial)) {
            console.warn('[SpotValuesMonitor] Ignoring spot values from wrong device:', {
              expected: serial,
              expectedNormalized: normalizeSerial(serial),
              connected: connectedSerial,
              connectedNormalized: normalizeSerial(connectedSerial)
            })
            return
          }

          // Data comes as { timestamp: number, values: { paramId: value, ... } }
          const timestamp = message.data.timestamp
          const values = message.data.values

          setSpotValues(values)

          // Update historical data with conversions applied
          setHistoricalData(prev => {
            const updated = { ...prev }

            Object.entries(values).forEach(([paramId, rawValue]) => {
              if (!updated[paramId]) {
                updated[paramId] = []
              }

              // Find the parameter to get its unit
              // Try both 'id' and 'i' (index) fields for compatibility
              const paramEntry = params && Object.entries(params).find(([_, p]) => 
                p.id?.toString() === paramId || p.i?.toString() === paramId
              )
              const param = paramEntry?.[1]

              // Apply conversion if value is numeric
              let convertedValue = rawValue as number
              if (typeof rawValue === 'number' && param?.unit) {
                const converted = convertSpotValue(rawValue, param.unit)
                convertedValue = converted.value
              }

              // Add new data point with converted value
              updated[paramId].push({ timestamp, value: convertedValue })

              // Limit history to MAX_HISTORY_POINTS
              if (updated[paramId].length > MAX_HISTORY_POINTS) {
                updated[paramId] = updated[paramId].slice(-MAX_HISTORY_POINTS)
              }
            })

            return updated
          })
          break
      }
    })

    return unsubscribe
  }, [subscribe, params, serial, connectedSerial, streaming])

  // Auto-start monitoring ALL spot values when ready
  useEffect(() => {
    const deviceMatches = serial && normalizeSerial(connectedSerial) === normalizeSerial(serial)
    const ready = params && deviceMatches && isConnected && !streaming

    if (ready) {
      // Get all spot value parameter IDs (non-params)
      const spotParamKeys = Object.keys(params).filter(key => !params[key].isparam)
      const paramIds = spotParamKeys
        .map(key => params[key]?.id || params[key]?.i)
        .filter(id => id !== undefined) as number[]

      if (paramIds.length > 0) {
        console.log('[SpotValuesMonitor] Auto-starting monitoring with ALL', paramIds.length, 'spot values')
        sendMessage('startSpotValues', {
          paramIds,
          interval
        })
      }
    }
  }, [params, serial, connectedSerial, isConnected, streaming])

  const handleParamToggle = (paramKey: string) => {
    // Toggle chart visibility only (streaming is always active for all params)
    setSelectedParams(prev => {
      const newSelected = new Set(prev)
      if (newSelected.has(paramKey)) {
        newSelected.delete(paramKey)
      } else {
        newSelected.add(paramKey)
      }
      return newSelected
    })
  }

  const handleClearData = () => {
    clearHistoricalData()
  }

  const handleIntervalChange = (newInterval: number) => {
    setInterval(newInterval)

    // If streaming, restart with new interval
    if (streaming && params) {
      const spotParamKeys = Object.keys(params).filter(key => !params[key].isparam)
      const paramIds = spotParamKeys
        .map(key => params[key]?.id || params[key]?.i)
        .filter(id => id !== undefined) as number[]

      if (paramIds.length > 0) {
        console.log('[SpotValuesMonitor] Restarting monitoring with new interval:', newInterval)
        sendMessage('startSpotValues', {
          paramIds,
          interval: newInterval
        })
      }
    }
  }

  // Only show loading screen on initial load (when params don't exist yet)
  if (paramsLoading && !params) {
    return (
      <div class="loading" style={{
        display: 'flex',
        flexDirection: 'column',
        alignItems: 'center',
        justifyContent: 'center',
        minHeight: '300px'
      }}>
        <LoadingSpinner size="large" label={content.loadingParameters} />
        {serial && connectedSerial && normalizeSerial(connectedSerial) !== normalizeSerial(serial) && (
          <div style={{ marginTop: '1rem', padding: '1rem', backgroundColor: '#fff3cd', border: '1px solid #ffc107', borderRadius: '4px', color: '#856404', maxWidth: '500px' }}>
            {content.loadingWrongDeviceWarningPrefix} <strong>{connectedSerial}</strong>{content.loadingWrongDeviceWarningEnd}
          </div>
        )}
      </div>
    )
  }

  if (!params) {
    return (
      <div class="error-message">{content.noParametersLoaded}</div>
    )
  }

  // Show warning if viewing parameters for wrong device (normalized comparison to handle leading zeros)
  const wrongDeviceConnected = serial && connectedSerial && normalizeSerial(connectedSerial) !== normalizeSerial(serial)

  // Organize parameters by category
  const spotParams = Object.entries(params).filter(([_, param]) => !param.isparam)
  const categories = new Map<string, Array<[string, typeof params[string]]>>()

  spotParams.forEach(([key, param]) => {
    const category = param.category || 'Spot Values'
    if (!categories.has(category)) {
      categories.set(category, [])
    }
    categories.get(category)!.push([key, param])
  })

  return (
    <section class="card">
      {showHeader && (
        <header class="header">
          {showBackButton && onBack && (
            <button class="back-button" onClick={onBack}>
              ← {content.back}
            </button>
          )}
          <h2>{content.title}</h2>
          {serial && (
            <div class="device-serial-header">{content.serialLabel} {serial}</div>
          )}
        </header>
      )}

      {wrongDeviceConnected && (
        <div style={{ margin: '1rem', padding: '1rem', backgroundColor: '#fff3cd', border: '1px solid #ffc107', borderRadius: '4px', color: '#856404' }}>
          {content.streamingWrongDeviceWarningPrefix} <strong>{content.streamingWrongDeviceWarningBold}</strong> {content.streamingWrongDeviceWarningText1} <strong>{serial}</strong>{content.streamingWrongDeviceWarningText2} <strong>{connectedSerial}</strong> {content.streamingWrongDeviceWarningText3}
        </div>
      )}

      <div class="spot-values-controls">
        <div class="form-group">
          <label>{content.updateInterval}</label>
          <input
            type="number"
            value={interval}
            onInput={(e) => handleIntervalChange(parseInt((e.target as HTMLInputElement).value))}
            min="100"
            max="10000"
            step="100"
          />
        </div>

        <div class="button-group">
          <button class="btn-secondary" onClick={handleClearData}>
            Clear Data
          </button>
        </div>
      </div>

      {/* Spot Values Grid */}
      <div class="spot-values-categories">
        {Array.from(categories.entries()).map(([category, categoryParams]) => (
          <div key={category} class="parameter-category">
            <h3 class="category-title">
              {category}
              <span class="param-count">({categoryParams.length})</span>
            </h3>
            <div class="parameters-list">
              {categoryParams.map(([key, param]) => {
                const paramId = (param.id || param.i)?.toString()
                const rawValue = paramId && spotValues[paramId] !== undefined
                  ? spotValues[paramId]
                  : param.value

                // Apply conversions and format for display
                let displayValue: string

                if (typeof rawValue === 'number' && param.unit) {
                  const converted = convertSpotValue(rawValue, param.unit)
                  const convertedParam = { ...param, unit: converted.unit }
                  displayValue = formatParameterValue(convertedParam, converted.value)
                } else {
                  displayValue = formatParameterValue(param, rawValue)
                }

                const isSelected = selectedParams.has(key)
                const hasChartData = isSelected && paramId && historicalData[paramId] && historicalData[paramId].length > 0

                return (
                  <div
                    key={key}
                    class="parameter-item"
                    style={{
                      background: isSelected ? '#f0f8ff' : 'transparent',
                      padding: '0.75rem',
                      borderRadius: '6px',
                      border: `2px solid ${isSelected ? 'var(--oi-blue)' : 'transparent'}`,
                      cursor: 'pointer',
                      transition: 'all 0.2s',
                      display: 'flex',
                      flexDirection: 'column',
                      gap: '0.75rem'
                    }}
                    onClick={() => handleParamToggle(key)}
                  >
                    <div class="parameter-header" style={{ marginBottom: '0.25rem' }}>
                      <label class="parameter-label" style={{ fontSize: '0.9rem', fontWeight: 500 }}>
                        {getDisplayName(key)}
                      </label>
                    </div>
                    <div class="parameter-value" style={{
                      fontSize: '1.1rem',
                      fontWeight: 600,
                      color: 'var(--text-primary)',
                      fontFamily: "'Monaco', 'Courier New', monospace"
                    }}>
                      {displayValue}
                    </div>
                    {hasChartData && (
                      <div style={{
                        width: '100%',
                        marginTop: '0.5rem',
                        overflow: 'hidden'
                      }}>
                        <MultiLineChart
                          series={[{
                            label: getDisplayName(key),
                            unit: convertSpotValue(0, param.unit).unit,
                            color: getColorForParam(key),
                            data: historicalData[paramId]
                          }]}
                          width={350}
                          height={150}
                          showLegend={false}
                        />
                      </div>
                    )}
                  </div>
                )
              })}
            </div>
          </div>
        ))}
      </div>
    </section>
  )
}
