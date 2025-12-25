import { useEffect } from 'preact/hooks'
import { useIntlayer } from 'preact-intlayer'
import { useParams } from '@hooks/useParams'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { useDeviceDetailsContext } from '@contexts/DeviceDetailsContext'
import MultiLineChart, { COLORS } from '@components/MultiLineChart'
import { convertSpotValue } from '@utils/spotValueConversions'
import { formatParameterValue } from '@utils/parameterDisplay'
import { ProgressBar } from '@components/ProgressBar'

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
    setChartParams,
    setConnectedSerial,
  } = useDeviceDetailsContext()

  // Destructure monitoring state for easier access
  const { streaming, interval, spotValues, historicalData, selectedParams, chartParams, connectedSerial } = monitoring

  // Load device parameters using explicit nodeId for multi-client support
  const { params, loading: paramsLoading, getDisplayName, downloadProgress } = useParams(serial, nodeId)

  // WebSocket connection
  const { isConnected, sendMessage, subscribe } = useWebSocketContext()

  // Subscribe to WebSocket messages
  useEffect(() => {
    const unsubscribe = subscribe((message) => {
      console.log('WebSocket event:', message.event, message.data)

      switch (message.event) {
        case 'connected':
          // Track which device is currently connected
          setConnectedSerial(message.data.serial)
          console.log('[SpotValuesMonitor] Device connected:', message.data.serial)
          break

        case 'disconnected':
          setConnectedSerial(null)
          // Stop streaming when disconnected
          if (streaming) {
            setStreaming(false)
            clearHistoricalData()
          }
          console.log('[SpotValuesMonitor] Device disconnected')
          break

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

  // Auto-select all spot values (non-params) on load
  useEffect(() => {
    if (params && selectedParams.size === 0) {
      const spotParamKeys = Object.keys(params).filter(key => !params[key].isparam)
      setSelectedParams(new Set(spotParamKeys))
    }
  }, [params])

  // Note: Removed cleanup that stops streaming on unmount
  // Streaming state now persists in context across tab switches

  const handleStartStop = () => {
    if (streaming) {
      // Stop streaming
      sendMessage('stopSpotValues')
    } else {
      // CRITICAL: Verify correct device is connected before starting streaming
      if (serial && normalizeSerial(connectedSerial) !== normalizeSerial(serial)) {
        alert(`Wrong device connected! Please connect to ${serial} first.\nCurrently connected to: ${connectedSerial || 'none'}`)
        return
      }

      // Start streaming
      if (selectedParams.size === 0) {
        alert(content.selectAtLeastOne)
        return
      }

      // Convert param keys to IDs (use id or i field)
      const paramIds = Array.from(selectedParams)
        .map(key => params?.[key]?.id || params?.[key]?.i)
        .filter(id => id !== undefined) as number[]

      sendMessage('startSpotValues', {
        paramIds,
        interval
      })
    }
  }

  const handleParamToggle = (paramKey: string) => {
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

  const handleSelectAll = () => {
    if (params) {
      const allKeys = Object.keys(params).filter(key => !params[key].isparam)
      setSelectedParams(new Set(allKeys))
    }
  }

  const handleSelectNone = () => {
    setSelectedParams(new Set())
  }

  const handleChartParamToggle = (paramKey: string) => {
    setChartParams(prev => {
      const newChartParams = new Set(prev)
      if (newChartParams.has(paramKey)) {
        newChartParams.delete(paramKey)
      } else {
        newChartParams.add(paramKey)
      }
      return newChartParams
    })
  }

  if (paramsLoading) {
    return (
      <div class="loading">
        <div style={{ width: '100%', maxWidth: '500px' }}>
          <ProgressBar
            progress={downloadProgress}
            label={downloadProgress > 0 ? "Downloading parameter definitions..." : content.loadingParameters}
          />
          {serial && connectedSerial && normalizeSerial(connectedSerial) !== normalizeSerial(serial) && (
            <div style={{ marginTop: '1rem', padding: '1rem', backgroundColor: '#fff3cd', border: '1px solid #ffc107', borderRadius: '4px', color: '#856404' }}>
              ⚠️ Warning: Currently connected to device <strong>{connectedSerial}</strong>, but loading parameters for <strong>{serial}</strong>.
              The parameter file may be incorrect. Please ensure the correct device is connected.
            </div>
          )}
        </div>
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
    const category = param.category || 'Other'
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
          ⚠️ <strong>Warning:</strong> You are viewing parameters for device <strong>{serial}</strong>, 
          but device <strong>{connectedSerial}</strong> is currently connected. 
          Values shown may be incorrect. Please connect to the correct device before streaming.
        </div>
      )}

      <div class="spot-values-controls">
        <div class="form-group">
          <label>{content.updateInterval}</label>
          <input
            type="number"
            value={interval}
            onInput={(e) => setInterval(parseInt((e.target as HTMLInputElement).value))}
            min="100"
            max="10000"
            step="100"
            disabled={streaming}
          />
        </div>

        <div class="button-group">
          <button
            class={streaming ? "btn-secondary" : "btn-primary"}
            onClick={handleStartStop}
            disabled={!isConnected || !!wrongDeviceConnected}
            title={wrongDeviceConnected ? 'Cannot stream - wrong device connected' : ''}
          >
            {streaming ? <>⏸ {content.stopMonitoring}</> : <>▶ {content.startMonitoring}</>}
          </button>
          {!streaming && (
            <>
              <button class="btn-secondary" onClick={handleSelectAll}>
                {content.selectAll}
              </button>
              <button class="btn-secondary" onClick={handleSelectNone}>
                {content.selectNone}
              </button>
            </>
          )}
        </div>
      </div>

      {streaming && (
        <div class="streaming-indicator">
          <span class="pulse-dot"></span>
          {content.streamingStatus({ count: selectedParams.size, interval })}
        </div>
      )}

      {/* Chart Section - Show when there's data and parameters are selected for charting */}
      {chartParams.size > 0 && Object.keys(historicalData).length > 0 && (
        <div class="chart-section" style={{ marginBottom: '2rem' }}>
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'space-between',
            marginBottom: '1rem',
            paddingBottom: '0.75rem',
            borderBottom: '2px solid var(--oi-beige)'
          }}>
            <h3 style={{ margin: 0, color: 'var(--oi-blue)', fontSize: '1.1rem', fontWeight: 600 }}>
              {content.timeSeriesChart}
            </h3>
            <div class="chart-param-selector" style={{ display: 'flex', flexWrap: 'wrap', gap: '1rem', flex: 1, justifyContent: 'flex-end' }}>
              {Array.from(selectedParams).map(key => {
                const param = params[key]
                const paramId = (param?.id || param?.i)?.toString()
                const hasData = paramId && historicalData[paramId] && historicalData[paramId].length > 0

                return (
                  <label key={key} class="chart-param-option" style={{ fontSize: '0.85rem' }}>
                    <input
                      type="checkbox"
                      checked={chartParams.has(key)}
                      onChange={() => handleChartParamToggle(key)}
                      disabled={!hasData}
                    />
                    <span style={{ color: hasData ? COLORS[Array.from(chartParams).indexOf(key) % COLORS.length] : '#999' }}>
                      {getDisplayName(key)}
                      {!hasData && <> ({content.noData})</>}
                    </span>
                  </label>
                )
              })}
            </div>
          </div>
          <MultiLineChart
            series={Array.from(chartParams).map((key, index) => {
              const param = params[key]
              const paramId = (param?.id || param?.i)?.toString()
              const converted = convertSpotValue(0, param.unit)
              const displayUnit = converted.unit

              return {
                label: getDisplayName(key),
                unit: displayUnit,
                color: COLORS[index % COLORS.length],
                data: (paramId && historicalData[paramId]) || []
              }
            })}
            width={Math.min(window.innerWidth - 100, 1000)}
            height={400}
          />
        </div>
      )}

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
                const isCharted = chartParams.has(key)

                return (
                  <div
                    key={key}
                    class="parameter-item"
                    style={{
                      background: isSelected ? '#f0f8ff' : 'transparent',
                      padding: '0.75rem',
                      borderRadius: '6px',
                      border: `2px solid ${isSelected ? 'var(--oi-blue)' : 'transparent'}`,
                      cursor: streaming ? 'default' : 'pointer',
                      transition: 'all 0.2s'
                    }}
                    onClick={() => !streaming && handleParamToggle(key)}
                  >
                    <div class="parameter-header" style={{ marginBottom: '0.5rem' }}>
                      <label class="parameter-label" style={{ fontSize: '0.9rem', fontWeight: 500, display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
                        {getDisplayName(key)}
                        {isCharted && (
                          <span style={{
                            width: '8px',
                            height: '8px',
                            borderRadius: '50%',
                            background: COLORS[Array.from(chartParams).indexOf(key) % COLORS.length],
                            display: 'inline-block'
                          }} />
                        )}
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
