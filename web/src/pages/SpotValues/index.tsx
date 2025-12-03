import { useState, useEffect } from 'preact/hooks'
import { useLocation, useRoute } from 'wouter'
import { useIntlayer } from 'react-intlayer'
import { useParams } from '@hooks/useParams'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { getParameterDisplayName } from '@utils/paramStorage'
import MultiLineChart, { COLORS, type DataSeries, type DataPoint } from '@components/MultiLineChart'

type HistoricalData = Record<string, DataPoint[]>

const MAX_HISTORY_POINTS = 100 // Limit to last 100 data points per parameter

export default function SpotValues() {
  const [, setLocation] = useLocation()
  const [, routeParams] = useRoute('/devices/:serial/spot-values')
  const content = useIntlayer('spot-values')

  const [streaming, setStreaming] = useState(false)
  const [interval, setInterval] = useState(1000)
  const [spotValues, setSpotValues] = useState<Record<string, number>>({})
  const [historicalData, setHistoricalData] = useState<HistoricalData>({})
  const [selectedParams, setSelectedParams] = useState<Set<string>>(new Set())
  const [chartParams, setChartParams] = useState<Set<string>>(new Set())
  const [viewMode, setViewMode] = useState<'table' | 'chart'>('table')

  // Load device parameters
  const { params, loading: paramsLoading, getDisplayName } = useParams(routeParams?.serial)

  // WebSocket connection
  const { isConnected, sendMessage, subscribe } = useWebSocketContext()

  // Subscribe to WebSocket messages
  useEffect(() => {
    const unsubscribe = subscribe((message) => {
      console.log('WebSocket event:', message.event, message.data)

      switch (message.event) {
        case 'spotValuesStatus':
          setStreaming(message.data.active)
          if (message.data.interval) {
            setInterval(message.data.interval)
          }
          // Clear historical data when stopping
          if (!message.data.active) {
            setHistoricalData({})
          }
          break

        case 'spotValues':
          // Data comes as { timestamp: number, values: { paramId: value, ... } }
          const timestamp = message.data.timestamp
          const values = message.data.values

          setSpotValues(values)

          // Update historical data
          setHistoricalData(prev => {
            const updated = { ...prev }

            Object.entries(values).forEach(([paramId, value]) => {
              if (!updated[paramId]) {
                updated[paramId] = []
              }

              // Add new data point
              updated[paramId].push({ timestamp, value: value as number })

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
  }, [subscribe])

  // Auto-select all spot values (non-params) on load
  useEffect(() => {
    if (params && selectedParams.size === 0) {
      const spotParamKeys = Object.keys(params).filter(key => !params[key].isparam)
      setSelectedParams(new Set(spotParamKeys))
    }
  }, [params])

  const handleStartStop = () => {
    if (streaming) {
      // Stop streaming
      sendMessage('stopSpotValues')
    } else {
      // Start streaming
      if (selectedParams.size === 0) {
        alert(content.selectAtLeastOne)
        return
      }

      // Convert param keys to IDs
      const paramIds = Array.from(selectedParams)
        .map(key => params?.[key]?.id)
        .filter(id => id !== undefined) as number[]

      sendMessage('startSpotValues', {
        paramIds,
        interval
      })
    }
  }

  const handleParamToggle = (paramKey: string) => {
    const newSelected = new Set(selectedParams)
    if (newSelected.has(paramKey)) {
      newSelected.delete(paramKey)
    } else {
      newSelected.add(paramKey)
    }
    setSelectedParams(newSelected)
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

  const handleBack = () => {
    if (streaming) {
      sendMessage('stopSpotValues')
    }
    setLocation(`/devices/${routeParams?.serial}`)
  }

  const handleChartParamToggle = (paramKey: string) => {
    const newChartParams = new Set(chartParams)
    if (newChartParams.has(paramKey)) {
      newChartParams.delete(paramKey)
    } else {
      newChartParams.add(paramKey)
    }
    setChartParams(newChartParams)
  }

  if (paramsLoading) {
    return (
      <div class="container">
        <div class="loading">{content.loadingParameters}</div>
      </div>
    )
  }

  if (!params) {
    return (
      <div class="container">
        <div class="error-message">{content.noParametersLoaded}</div>
      </div>
    )
  }

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
    <div class="container">
      <header class="header">
        <button class="back-button" onClick={handleBack}>
          ← {content.back}
        </button>
        <h1>{content.title}</h1>
        {routeParams?.serial && (
          <div class="device-serial-header">{content.serialLabel} {routeParams.serial}</div>
        )}
      </header>

      <section class="card">
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
              disabled={!isConnected}
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
      </section>

      {viewMode === 'table' && (
        <div class="spot-values-grid">
          {Array.from(categories.entries()).map(([category, categoryParams]) => (
            <section key={category} class="card">
              <h2>{category}</h2>
              <table class="spot-values-table">
                <thead>
                  <tr>
                    {!streaming && <th>{content.monitor}</th>}
                    <th>{content.parameter}</th>
                    <th>{content.value}</th>
                    <th>{content.unit}</th>
                  </tr>
                </thead>
                <tbody>
                  {categoryParams.map(([key, param]) => {
                    const paramId = param.id?.toString()
                    const value = paramId && spotValues[paramId] !== undefined
                      ? spotValues[paramId]
                      : param.value

                    return (
                      <tr key={key} class={selectedParams.has(key) ? 'selected' : ''}>
                        {!streaming && (
                          <td>
                            <input
                              type="checkbox"
                              checked={selectedParams.has(key)}
                              onChange={() => handleParamToggle(key)}
                            />
                          </td>
                        )}
                        <td>{getDisplayName(key)}</td>
                        <td class="value-cell">{typeof value === 'number' ? value.toFixed(2) : value}</td>
                        <td class="unit-cell">{param.unit || ''}</td>
                      </tr>
                    )
                  })}
                </tbody>
              </table>
            </section>
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
                const paramId = param?.id?.toString()
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
                  const paramId = param?.id?.toString()
                  return {
                    label: getDisplayName(key),
                    unit: param.unit,
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
    </div>
  )
}
