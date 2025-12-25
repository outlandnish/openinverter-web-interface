import { useEffect } from 'preact/hooks'
import { useIntlayer } from 'preact-intlayer'
import { useParams } from '@hooks/useParams'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { useDeviceDetailsContext } from '@contexts/DeviceDetailsContext'
import MultiLineChart, { COLORS, type DataPoint } from '@components/MultiLineChart'
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
    setViewMode,
    setConnectedSerial,
  } = useDeviceDetailsContext()

  // Destructure monitoring state for easier access
  const { streaming, interval, spotValues, historicalData, selectedParams, chartParams, viewMode, connectedSerial } = monitoring

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

  // Cleanup on unmount
  useEffect(() => {
    return () => {
      if (streaming) {
        sendMessage('stopSpotValues')
      }
    }
  }, [streaming])

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

      <div class="view-mode-tabs">
        <button
          class={viewMode === 'table' ? 'tab-active' : 'tab'}
          onClick={() => setViewMode('table')}
        >
          {content.tableView}
        </button>
        <button
          class={viewMode === 'chart' ? 'tab-active' : 'tab'}
          onClick={() => setViewMode('chart')}
          disabled={!streaming && Object.keys(historicalData).length === 0}
        >
          {content.chartView}
        </button>
      </div>

      {viewMode === 'table' && (
        <div class="spot-values-grid">
          {Array.from(categories.entries()).map(([category, categoryParams]) => (
            <div key={category} class="category-section">
              <h3>{category}</h3>
              <table class="spot-values-table">
                <thead>
                  <tr>
                    <th>{content.parameter}</th>
                    <th>{content.value}</th>
                  </tr>
                </thead>
                <tbody>
                  {categoryParams.map(([key, param]) => {
                    // Use either id or i (index) field
                    const paramId = (param.id || param.i)?.toString()
                    const rawValue = paramId && spotValues[paramId] !== undefined
                      ? spotValues[paramId]
                      : param.value

                    // Apply conversions and format for display
                    let displayValue: string

                    if (typeof rawValue === 'number' && param.unit) {
                      // Apply unit conversion
                      const converted = convertSpotValue(rawValue, param.unit)

                      // Create a modified param with converted unit for formatting
                      const convertedParam = { ...param, unit: converted.unit }
                      displayValue = formatParameterValue(convertedParam, converted.value)
                    } else {
                      // Use formatParameterValue for enum handling and standard formatting
                      displayValue = formatParameterValue(param, rawValue)
                    }

                    return (
                      <tr
                        key={key}
                        class={selectedParams.has(key) ? 'selected' : ''}
                        onClick={() => !streaming && handleParamToggle(key)}
                        style={{ cursor: streaming ? 'default' : 'pointer' }}
                      >
                        <td>{getDisplayName(key)}</td>
                        <td class="value-cell">{displayValue}</td>
                      </tr>
                    )
                  })}
                </tbody>
              </table>
            </div>
          ))}
        </div>
      )}

      {viewMode === 'chart' && (
        <div class="chart-view">
          <section class="card">
            <h2>{content.selectParametersToChart}</h2>
            <div class="chart-param-selector">
              {Array.from(selectedParams).map(key => {
                const param = params[key]
                // Use either id or i (index) field
                const paramId = (param?.id || param?.i)?.toString()
                const hasData = paramId && historicalData[paramId] && historicalData[paramId].length > 0

                return (
                  <label key={key} class="chart-param-option">
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
          </section>

          <section class="card">
            <h2>{content.timeSeriesChart}</h2>
            {chartParams.size > 0 ? (
              <MultiLineChart
                series={Array.from(chartParams).map((key, index) => {
                  const param = params[key]
                  // Use either id or i (index) field
                  const paramId = (param?.id || param?.i)?.toString()

                  // Get the converted unit for display
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
                height={500}
              />
            ) : (
              <div class="chart-placeholder">
                <p>{content.chartPlaceholder}</p>
              </div>
            )}
          </section>
        </div>
      )}
    </section>
  )
}
