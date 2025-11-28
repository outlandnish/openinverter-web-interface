import { useState, useEffect } from 'preact/hooks'
import { useLocation, useRoute } from 'wouter'
import { useIntlayer } from 'preact-intlayer'
import { useParams } from '@hooks/useParams'
import { useWebSocket } from '@hooks/useWebSocket'
import MultiLineChart, { COLORS, type DataPoint } from '@components/MultiLineChart'
import Layout from '@components/Layout'
import ConnectionStatus from '@components/ConnectionStatus'
import { useToast } from '@hooks/useToast'
import { api } from '@api/inverter'

type HistoricalData = Record<string, DataPoint[]>
const MAX_HISTORY_POINTS = 100

export default function DeviceDetails() {
  const [, setLocation] = useLocation()
  const [, routeParams] = useRoute('/devices/:serial')
  const appContent = useIntlayer('app')
  const content = useIntlayer('device-details')
  const { showError, showSuccess, showWarning } = useToast()

  const [loading, setLoading] = useState(true)
  const [nodeId, setNodeId] = useState('')
  const [firmwareVersion] = useState('')
  const [deviceConnected, setDeviceConnected] = useState(false)
  const [deviceName, setDeviceName] = useState<string>('')

  // Spot values state
  const [streaming, setStreaming] = useState(false)
  const [updateInterval, setUpdateInterval] = useState(1000)
  const [spotValues, setSpotValues] = useState<Record<string, number>>({})
  const [historicalData, setHistoricalData] = useState<HistoricalData>({})
  const [selectedParams, setSelectedParams] = useState<Set<string>>(new Set())
  const [chartParams, setChartParams] = useState<Set<string>>(new Set())
  const [viewMode, setViewMode] = useState<'table' | 'chart'>('table')

  // OTA update state
  const [otaFile, setOtaFile] = useState<File | null>(null)
  const [otaProgress, setOtaProgress] = useState<number>(0)
  const [otaStatus, setOtaStatus] = useState<'idle' | 'uploading' | 'updating' | 'success' | 'error'>('idle')
  const [otaError, setOtaError] = useState<string>('')

  // Collapsed sections state for parameters
  const [collapsedSections, setCollapsedSections] = useState<Set<string>>(new Set())

  // Load device parameters
  const { params, loading: paramsLoading, error: paramsError, refresh: refreshParams, getDisplayName } = useParams(routeParams?.serial)

  // WebSocket connection
  const { isConnected, sendMessage } = useWebSocket('/ws', {
    onMessage: (message) => {
      console.log('WebSocket event:', message.event, message.data)

      switch (message.event) {
        case 'connected':
          if (message.data.serial === routeParams?.serial) {
            setDeviceConnected(true)
          }
          break

        case 'disconnected':
          setDeviceConnected(false)
          showWarning(content.deviceDisconnected)
          break

        case 'nodeIdInfo':
          setNodeId(message.data.id.toString())
          setLoading(false)
          setDeviceConnected(true)
          break

        case 'nodeIdSet':
          setNodeId(message.data.id.toString())
          showSuccess(content.nodeIdSaved)
          break

        case 'spotValuesStatus':
          setStreaming(message.data.active)
          if (message.data.interval) {
            setUpdateInterval(message.data.interval)
          }
          if (!message.data.active) {
            setHistoricalData({})
          }
          break

        case 'spotValues':
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

              updated[paramId].push({ timestamp, value: value as number })

              if (updated[paramId].length > MAX_HISTORY_POINTS) {
                updated[paramId] = updated[paramId].slice(-MAX_HISTORY_POINTS)
              }
            })

            return updated
          })
          break

        case 'otaProgress':
          setOtaProgress(message.data.progress)
          setOtaStatus('updating')
          break

        case 'otaSuccess':
          setOtaProgress(100)
          setOtaStatus('success')
          showSuccess(content.updateSuccessful)
          break

        case 'otaError':
          const otaErrorMsg = message.data.error || 'Unknown error occurred'
          setOtaStatus('error')
          setOtaError(otaErrorMsg)
          showError(`${content.otaUpdateFailed} ${otaErrorMsg}`)
          break
      }
    },
    onOpen: () => {
      console.log('WebSocket connected, loading settings...')
      loadSettings()
    }
  })

  useEffect(() => {
    if (isConnected) {
      loadSettings()
    }
  }, [isConnected])

  // Load device name from saved devices
  useEffect(() => {
    const loadDeviceName = async () => {
      try {
        const response = await api.getSavedDevices()
        const serial = routeParams?.serial
        if (serial && response.devices && response.devices[serial]) {
          setDeviceName(response.devices[serial].name || '')
        }
      } catch (error) {
        console.error('Failed to load device name:', error)
      }
    }
    
    if (routeParams?.serial) {
      loadDeviceName()
    }
  }, [routeParams?.serial])

  // Auto-select all spot values on load
  useEffect(() => {
    if (params && selectedParams.size === 0) {
      const spotParamKeys = Object.keys(params).filter(key => !params[key].isparam)
      setSelectedParams(new Set(spotParamKeys))
    }
  }, [params])

  const loadSettings = () => {
    setLoading(true)
    sendMessage('getNodeId')
  }

  const handleSaveNodeId = () => {
    const id = parseInt(nodeId)
    if (isNaN(id)) {
      alert('Please enter a valid node ID')
      return
    }
    sendMessage('setNodeId', { id })
  }

  // Cleanup when component unmounts
  useEffect(() => {
    return () => {
      // Only stop if currently streaming when component unmounts
      sendMessage('stopSpotValues')
    }
  }, [])

  const handleRefreshParams = async () => {
    await refreshParams()
    alert('Parameters refreshed from device')
  }

  // Spot values handlers
  const handleStartStop = () => {
    if (streaming) {
      sendMessage('stopSpotValues')
    } else {
      if (selectedParams.size === 0) {
        alert('Please select at least one parameter to monitor')
        return
      }

      const paramIds = Array.from(selectedParams)
        .map(key => params?.[key]?.id)
        .filter(id => id !== undefined) as number[]

      sendMessage('startSpotValues', {
        paramIds,
        interval: updateInterval
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

  const handleChartParamToggle = (paramKey: string) => {
    const newChartParams = new Set(chartParams)
    if (newChartParams.has(paramKey)) {
      newChartParams.delete(paramKey)
    } else {
      newChartParams.add(paramKey)
    }
    setChartParams(newChartParams)
  }

  // OTA update handlers
  const handleOtaFileSelect = (e: Event) => {
    const target = e.target as HTMLInputElement
    const file = target.files?.[0]
    if (file && file.name.endsWith('.bin')) {
      setOtaFile(file)
      setOtaError('')
    } else if (file) {
      setOtaError('Please select a .bin file')
      setOtaFile(null)
    }
  }

  const handleOtaUpload = async () => {
    if (!otaFile) {
      setOtaError('Please select a firmware file first')
      return
    }

    // Stop streaming if active
    if (streaming) {
      sendMessage('stopSpotValues')
    }

    try {
      setOtaStatus('uploading')
      setOtaProgress(0)
      setOtaError('')

      // Create form data for file upload
      const formData = new FormData()
      formData.append('firmware', otaFile)

      // Upload firmware file
      const response = await fetch('/ota/upload', {
        method: 'POST',
        body: formData
      })

      if (!response.ok) {
        throw new Error('Upload failed')
      }

      // The backend will send progress updates via WebSocket
      setOtaStatus('updating')
    } catch (error) {
      console.error('OTA upload error:', error)
      const errorMessage = error instanceof Error ? error.message : 'Upload failed'
      setOtaStatus('error')
      setOtaError(errorMessage)
      showError(`${content.otaUpdateFailed} ${errorMessage}`)
    }
  }

  const handleOtaReset = () => {
    setOtaFile(null)
    setOtaProgress(0)
    setOtaStatus('idle')
    setOtaError('')
  }

  const toggleSection = (category: string) => {
    const newCollapsed = new Set(collapsedSections)
    if (newCollapsed.has(category)) {
      newCollapsed.delete(category)
    } else {
      newCollapsed.add(category)
    }
    setCollapsedSections(newCollapsed)
  }

  if (loading) {
    return (
      <div class="container">
        <div class="loading">{appContent.loading}</div>
      </div>
    )
  }

  // Organize parameters by category
  const spotParams = params ? Object.entries(params).filter(([_, param]) => !param.isparam) : []
  const categories = new Map<string, Array<[string, any]>>()

  spotParams.forEach(([key, param]) => {
    const category = param.category || 'Other'
    if (!categories.has(category)) {
      categories.set(category, [])
    }
    categories.get(category)!.push([key, param])
  })

  return (
    <Layout currentSerial={routeParams?.serial}>
      <div class="container">
        <div class="page-header">
          <div class="page-title-row">
            <h1 class="page-title">{deviceName || routeParams?.serial || content.deviceMonitor}</h1>
            <ConnectionStatus
              connected={deviceConnected}
              label={deviceConnected ? content.connected : content.disconnected}
            />
          </div>
          <div class="device-info-inline">
            <div class="info-badge">
              <span class="info-label">{content.serial}</span>
              <span class="info-value">{routeParams?.serial || content.unknown}</span>
            </div>
            <div class="info-badge">
              <span class="info-label">{content.nodeId}</span>
              <span class="info-value">{nodeId || 'N/A'}</span>
            </div>
            <div class="info-badge">
              <span class="info-label">{content.firmware}</span>
              <span class="info-value">{firmwareVersion || content.unknown}</span>
            </div>
          </div>
        </div>

      {/* Spot Values Monitoring Card */}
      <section id="live-monitoring" class="card">
        <h2 class="section-header" onClick={(e) => {
          const target = e.currentTarget.parentElement
          if (target) target.scrollIntoView({ behavior: 'smooth', block: 'start' })
        }}>
          {content.liveMonitoring}
        </h2>

        <div class="spot-values-controls">
          <div class="form-group">
            <label>{content.updateInterval}</label>
            <input
              type="number"
              value={updateInterval}
              onInput={(e) => setUpdateInterval(parseInt((e.target as HTMLInputElement).value))}
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
              disabled={!isConnected || !params}
            >
              {streaming ? `⏸ ${content.stopMonitoring}` : `▶ ${content.startMonitoring}`}
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
            {content.streaming} {selectedParams.size} {content.parametersEvery} {updateInterval}ms
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

        {viewMode === 'table' && params && (
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
                      const paramId = param.id?.toString()
                      const value = paramId && spotValues[paramId] !== undefined
                        ? spotValues[paramId]
                        : param.value

                      const displayValue = value !== undefined && value !== null
                        ? `${typeof value === 'number' ? value.toFixed(2) : value}${param.unit ? ' ' + param.unit : ''}`
                        : ''

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

        {viewMode === 'chart' && params && (
          <div class="chart-view">
            <div class="chart-param-selector">
              <h3>{content.selectParametersToChart}</h3>
              <div class="param-checkboxes">
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
                        {!hasData && ` (${content.noData})`}
                      </span>
                    </label>
                  )
                })}
              </div>
            </div>

            <div class="chart-container">
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
                  width={Math.min(typeof window !== 'undefined' ? window.innerWidth - 100 : 1000, 1000)}
                  height={500}
                />
              ) : (
                <div class="chart-placeholder">
                  <p>{content.chartPlaceholder}</p>
                </div>
              )}
            </div>
          </div>
        )}
      </section>

      {/* Device Parameters - All Updatable Parameters */}
      <section id="device-parameters" class="card">
        <h2 class="section-header" onClick={(e) => {
          const target = e.currentTarget.parentElement
          if (target) target.scrollIntoView({ behavior: 'smooth', block: 'start' })
        }}>
          {content.deviceParameters}
        </h2>

        {params && (() => {
          // Group parameters by category
          const categorizedParams = Object.entries(params)
            .filter(([_, param]) => param.isparam)
            .sort((a, b) => {
              // Sort by category, then by name
              const catA = a[1].category || 'Other'
              const catB = b[1].category || 'Other'
              if (catA !== catB) return catA.localeCompare(catB)
              return getDisplayName(a[0]).localeCompare(getDisplayName(b[0]))
            })
            .reduce((acc, [key, param]) => {
              const category = param.category || 'Other'
              if (!acc[category]) acc[category] = []
              acc[category].push([key, param])
              return acc
            }, {} as Record<string, Array<[string, any]>>)

          return (
            <div class="parameters-grid">
              {Object.entries(categorizedParams).map(([category, categoryParams]) => {
                const isCollapsed = collapsedSections.has(category)
                return (
                  <div key={category} class={`parameter-category${isCollapsed ? ' collapsed' : ''}`}>
                    <h3 class="category-title" onClick={() => toggleSection(category)} style={{ cursor: 'pointer' }}>
                      <span class="collapse-icon">{isCollapsed ? '▶' : '▼'}</span>
                      {category}
                      <span class="param-count">({categoryParams.length})</span>
                    </h3>
                    {!isCollapsed && (
                      <div class="parameters-list">
                    {categoryParams.map(([key, param]) => {
                      const paramId = param.id
                      const hasEnum = param.enums && Object.keys(param.enums).length > 0

                      return (
                        <div key={key} class="parameter-item">
                          <div class="parameter-header">
                            <label class="parameter-label">
                              {getDisplayName(key)}
                              {param.unit && <span class="parameter-unit"> ({param.unit})</span>}
                            </label>
                          </div>
                          
                          <div class="parameter-input-group">
                            {hasEnum ? (
                              <select
                                value={param.value}
                                onChange={async (e) => {
                                  const newValue = (e.target as HTMLSelectElement).value
                                  try {
                                    await api.setParamById(paramId, newValue)
                                    showSuccess(`${getDisplayName(key)} ${content.parameterUpdated}`)
                                    // Refresh params to show new value
                                    await refreshParams()
                                  } catch (error) {
                                    showError(`${content.failedToUpdate} ${getDisplayName(key)}`)
                                  }
                                }}
                                disabled={!isConnected}
                              >
                                {Object.entries(param.enums).map(([value, label]) => (
                                  <option key={value} value={value}>
                                    {String(label)}
                                  </option>
                                ))}
                              </select>
                            ) : (
                              <input
                                type="number"
                                value={param.value}
                                min={param.minimum}
                                max={param.maximum}
                                step={param.unit === 'Hz' || param.unit === 'A' ? '0.1' : '1'}
                                onBlur={async (e) => {
                                  const target = e.target as HTMLInputElement
                                  const newValue = parseFloat(target.value)
                                  if (isNaN(newValue)) return
                                  
                                  // Validate range
                                  if (param.minimum !== undefined && newValue < param.minimum) {
                                    showError(`${content.valueMustBeAtLeast} ${param.minimum}`)
                                    target.value = param.value.toString()
                                    return
                                  }
                                  if (param.maximum !== undefined && newValue > param.maximum) {
                                    showError(`${content.valueMustBeAtMost} ${param.maximum}`)
                                    target.value = param.value.toString()
                                    return
                                  }

                                  if (newValue !== param.value) {
                                    try {
                                      await api.setParamById(paramId, newValue)
                                      showSuccess(`${getDisplayName(key)} ${content.parameterUpdated}`)
                                      // Refresh params to show new value
                                      await refreshParams()
                                    } catch (error) {
                                      showError(`${content.failedToUpdate} ${getDisplayName(key)}`)
                                      target.value = param.value.toString()
                                    }
                                  }
                                }}
                                disabled={!isConnected}
                              />
                            )}
                            
                            {param.minimum !== undefined && param.maximum !== undefined && !hasEnum && (
                              <small class="parameter-hint">
                                {content.range} {param.minimum} - {param.maximum}
                                {param.default !== undefined && ` (${content.default} ${param.default})`}
                              </small>
                            )}
                          </div>
                        </div>
                      )
                    })}
                      </div>
                    )}
                  </div>
                )
              })}
            </div>
          )
        })()}

        <div class="form-group" style={{ marginTop: '2rem', paddingTop: '1rem', borderTop: '1px solid #ddd' }}>
          <label>{content.nodeId}</label>
          <input
            type="text"
            value={nodeId}
            onInput={(e) => setNodeId((e.target as HTMLInputElement).value)}
            placeholder="Enter Node ID"
          />
          <small class="hint">{content.canSpeedNote}</small>
        </div>

        <div class="button-group">
          <button class="btn-primary" onClick={handleSaveNodeId} disabled={!isConnected}>
            {content.saveNodeId}
          </button>
          <button class="btn-secondary" onClick={async () => {
            try {
              await api.saveParams()
              showSuccess(content.parametersSaved)
            } catch (error) {
              showError(content.saveParametersFailed)
            }
          }} disabled={!isConnected}>
            {content.saveAllToFlash}
          </button>
        </div>
      </section>

      {/* OTA Firmware Update Card */}
      <section id="firmware-update" class="card">
        <h2 class="section-header" onClick={(e) => {
          const target = e.currentTarget.parentElement
          if (target) target.scrollIntoView({ behavior: 'smooth', block: 'start' })
        }}>
          {content.firmwareUpdate}
        </h2>

        {otaStatus === 'idle' && (
          <div class="ota-upload-section">
            <p class="info">{content.otaInfo}</p>

            <div class="form-group">
              <label>{content.firmwareFile}</label>
              <input
                type="file"
                accept=".bin"
                onChange={handleOtaFileSelect}
                disabled={streaming}
              />
              {otaFile && (
                <div class="file-info">
                  {content.selected} {otaFile.name} ({(otaFile.size / 1024).toFixed(2)} KB)
                </div>
              )}
            </div>

            {otaError && (
              <div class="error-message">{otaError}</div>
            )}

            <div class="button-group">
              <button
                class="btn-primary"
                onClick={handleOtaUpload}
                disabled={!otaFile || streaming}
              >
                {content.startFirmwareUpdate}
              </button>
            </div>

            {streaming && (
              <div class="warning-message">
                {content.monitoringWarning}
              </div>
            )}
          </div>
        )}

        {(otaStatus === 'uploading' || otaStatus === 'updating') && (
          <div class="ota-progress-section">
            <div class="progress-info">
              <h3>
                {otaStatus === 'uploading' ? content.uploadingFirmware : content.updatingDevice}
              </h3>
              <p>{content.updateProgress}</p>
            </div>

            <div class="progress-bar-container">
              <div class="progress-bar" style={{ width: `${otaProgress}%` }}>
                <span class="progress-text">{otaProgress}%</span>
              </div>
            </div>

            <div class="progress-status">
              {otaStatus === 'uploading' && content.transferringFirmware}
              {otaStatus === 'updating' && content.installingFirmware}
            </div>
          </div>
        )}

        {otaStatus === 'success' && (
          <div class="ota-success-section">
            <div class="success-message">
              <h3>{content.updateSuccessful}</h3>
              <p>{content.updateSuccessInfo}</p>
              <p>{content.reconnectInfo}</p>
            </div>

            <button class="btn-secondary" onClick={handleOtaReset}>
              {content.uploadAnother}
            </button>
          </div>
        )}

        {otaStatus === 'error' && (
          <div class="ota-error-section">
            <div class="error-message">
              <h3>{content.updateFailed}</h3>
              <p>{otaError}</p>
            </div>

            <button class="btn-secondary" onClick={handleOtaReset}>
              {content.tryAgain}
            </button>
          </div>
        )}
      </section>
      </div>
    </Layout>
  )
}
