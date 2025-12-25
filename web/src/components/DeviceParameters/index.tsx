import { useState, useEffect, useRef } from 'preact/hooks'
import { useIntlayer } from 'preact-intlayer'
import { useParams } from '@hooks/useParams'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { useToast } from '@hooks/useToast'
import { api } from '@api/inverter'
import ParameterCategory from './ParameterCategory'
import { ProgressBar } from '@components/ProgressBar'

interface DeviceParametersProps {
  serial: string
  nodeId: string
  onNodeIdChange: (nodeId: string) => void
  onSaveNodeId: () => void
}

export default function DeviceParameters({
  serial,
  nodeId,
  onNodeIdChange,
  onSaveNodeId
}: DeviceParametersProps) {
  const content = useIntlayer('device-details')
  const { showError, showSuccess } = useToast()
  const { isConnected, subscribe } = useWebSocketContext()
  // Parse nodeId to number for fetching params from specific device
  const numericNodeId = parseInt(nodeId)
  const { params, loading, getDisplayName, downloadProgress, downloadTotal, refresh } = useParams(serial, isNaN(numericNodeId) ? undefined : numericNodeId)

  const [collapsedSections, setCollapsedSections] = useState<Set<string>>(new Set())
  const [localParams, setLocalParams] = useState(params)
  const [isImporting, setIsImporting] = useState(false)
  const [importProgress, setImportProgress] = useState({ current: 0, total: 0 })
  const fileInputRef = useRef<HTMLInputElement>(null)

  // Sync localParams with params when params changes (e.g., on initial load or refresh)
  useEffect(() => {
    setLocalParams(params)
  }, [params])

  // Subscribe to WebSocket events for save to flash responses and parameter update errors
  useEffect(() => {
    const unsubscribe = subscribe((message) => {
      if (message.event === 'saveToFlashSuccess') {
        // Refresh parameters from device to verify they were saved correctly
        // Note: Schema is still cached, only values are refreshed
        refresh()
        showSuccess(content.parametersSaved)
      } else if (message.event === 'saveToFlashError') {
        showError(content.saveParametersFailed)
      } else if (message.event === 'paramUpdateError') {
        // Show parameter update errors to the user
        const paramName = message.data?.paramId ? `Parameter ${message.data.paramId}` : 'Parameter'
        const errorMsg = message.data?.error || 'Unknown error'
        showError(`${paramName}: ${errorMsg}`)
      }
    })

    return unsubscribe
  }, [subscribe, showSuccess, showError, content, refresh])

  const toggleSection = (category: string) => {
    const newCollapsed = new Set(collapsedSections)
    if (newCollapsed.has(category)) {
      newCollapsed.delete(category)
    } else {
      newCollapsed.add(category)
    }
    setCollapsedSections(newCollapsed)
  }

  // Update a single parameter locally without full reload
  const handleParamUpdate = (paramId: number, newValue: number) => {
    if (!localParams) return

    // Find and update the parameter by ID
    const updatedParams = { ...localParams }
    for (const key in updatedParams) {
      if (updatedParams[key].id === paramId) {
        updatedParams[key] = {
          ...updatedParams[key],
          value: newValue
        }
        break
      }
    }
    setLocalParams(updatedParams)
  }

  const handleSaveToFlash = () => {
    try {
      api.saveParams()
      // Success/error will be shown via WebSocket event handlers
    } catch (error) {
      showError(content.saveParametersFailed)
    }
  }

  const handleExportToJSON = () => {
    if (!localParams) {
      showError(content.noParametersToExport)
      return
    }

    // Create export object with only parameter values
    const exportData: Record<string, number | string> = {}
    Object.entries(localParams).forEach(([key, param]) => {
      if (param.isparam) {
        exportData[key] = param.value
      }
    })

    // Create blob and download
    const blob = new Blob([JSON.stringify(exportData, null, 2)], { type: 'application/json' })
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = `parameters_${serial}_${new Date().toISOString().split('T')[0]}.json`
    document.body.appendChild(a)
    a.click()
    document.body.removeChild(a)
    URL.revokeObjectURL(url)

    showSuccess(content.parametersExported)
  }

  const handleImportFromJSON = () => {
    fileInputRef.current?.click()
  }

  const handleFileSelected = async (e: Event) => {
    const target = e.target as HTMLInputElement
    const file = target.files?.[0]

    if (!file) return

    try {
      const text = await file.text()
      const importedData = JSON.parse(text)

      if (!localParams) {
        showError(content.noParameterDefinitions)
        return
      }

      let validCount = 0
      let invalidCount = 0
      const errors: string[] = []
      const updates: Array<{ paramId: number; value: number; key: string }> = []

      // Validate all parameters first
      Object.entries(importedData).forEach(([key, value]) => {
        if (typeof value !== 'number') {
          invalidCount++
          errors.push(`${key}: value must be a number`)
          return
        }

        const param = localParams[key]

        if (!param) {
          invalidCount++
          errors.push(`${key}: parameter not found`)
          return
        }

        if (!param.isparam) {
          invalidCount++
          errors.push(`${key}: not a settable parameter`)
          return
        }

        if (param.id === undefined) {
          invalidCount++
          errors.push(`${key}: parameter ID not defined`)
          return
        }

        // Validate range
        if (param.minimum !== undefined && value < param.minimum) {
          invalidCount++
          errors.push(`${key}: value ${value} below minimum ${param.minimum}`)
          return
        }

        if (param.maximum !== undefined && value > param.maximum) {
          invalidCount++
          errors.push(`${key}: value ${value} above maximum ${param.maximum}`)
          return
        }

        // Add to updates list
        validCount++
        updates.push({ paramId: param.id, value, key })
      })

      // Clear file input
      target.value = ''

      // If we have valid updates, apply them
      if (updates.length > 0) {
        setIsImporting(true)
        setImportProgress({ current: 0, total: updates.length })

        // Update local state first
        updates.forEach(({ paramId, value }) => {
          handleParamUpdate(paramId, value)
        })

        // Send updates to device with rate limiting to prevent overwhelming the ESP32
        // The ESP32 can only queue a limited number of WebSocket messages
        const delay = (ms: number) => new Promise(resolve => setTimeout(resolve, ms))

        for (let i = 0; i < updates.length; i++) {
          const { paramId, value, key } = updates[i]
          try {
            await api.setParamById(paramId, value)
            setImportProgress({ current: i + 1, total: updates.length })
            // Wait 50ms between each update to avoid overwhelming the ESP32 message queue
            await delay(50)
          } catch (error) {
            showError(content.failedToUpdate({ key, error: (error as Error).message }))
          }
        }

        setIsImporting(false)
        showSuccess(content.importedSuccessfully({ count: validCount, plural: validCount === 1 ? '' : 's' }))
      }

      if (invalidCount > 0) {
        const errorList = errors.slice(0, 5).join('\n') + (errors.length > 5 ? `\n...and ${errors.length - 5} more` : '')
        showError(content.validationFailed({ count: invalidCount, plural: invalidCount === 1 ? '' : 's', errors: errorList }))
      }

      if (validCount === 0 && invalidCount === 0) {
        showError(content.noValidParameters)
      }
    } catch (error) {
      showError(content.parseJSONFailed({ error: (error as Error).message }))
      target.value = ''
      setIsImporting(false)
    }
  }

  if (loading) {
    // Determine if we should show determinate or indeterminate progress
    const isIndeterminate = downloadTotal === 0 && downloadProgress === 0
    const label = downloadTotal > 0
      ? `Downloading parameter definitions... (${Math.round((downloadProgress * downloadTotal) / 100)} / ${downloadTotal} bytes)`
      : downloadProgress > 0
      ? `Downloading parameter definitions... ${downloadProgress}%`
      : "Loading parameters..."

    return (
      <section id="device-parameters" class="card">
        <h2 class="section-header">{content.deviceParameters}</h2>
        <div class="loading">
          <div style={{ width: '100%', maxWidth: '500px' }}>
            <ProgressBar
              progress={downloadProgress}
              label={label}
              indeterminate={isIndeterminate}
            />
          </div>
        </div>
      </section>
    )
  }

  if (!localParams) {
    return (
      <section id="device-parameters" class="card">
        <h2 class="section-header">{content.deviceParameters}</h2>
        <div class="error-message">No parameters loaded</div>
      </section>
    )
  }

  // Group parameters by category
  const categorizedParams = Object.entries(localParams)
    .filter(([_, param]) => param.isparam)
    .sort((a, b) => {
      // Sort by category, then by name
      const catA = a[1].category || 'Spot Values'
      const catB = b[1].category || 'Spot Values'
      if (catA !== catB) return catA.localeCompare(catB)
      return getDisplayName(a[0]).localeCompare(getDisplayName(b[0]))
    })
    .reduce((acc, [key, param]) => {
      const category = param.category || 'Spot Values'
      if (!acc[category]) acc[category] = []
      acc[category].push([key, param])
      return acc
    }, {} as Record<string, Array<[string, any]>>)

  return (
    <section id="device-parameters" class="card">
      <h2
        class="section-header"
        onClick={(e) => {
          const target = e.currentTarget.parentElement
          if (target) target.scrollIntoView({ behavior: 'smooth', block: 'start' })
        }}
      >
        {content.deviceParameters}
      </h2>

      <div class="parameters-grid">
        {Object.entries(categorizedParams).map(([category, categoryParams]) => (
          <ParameterCategory
            key={category}
            category={category}
            params={categoryParams}
            isCollapsed={collapsedSections.has(category)}
            onToggle={toggleSection}
            getDisplayName={getDisplayName}
            isConnected={isConnected}
            onUpdate={handleParamUpdate}
          />
        ))}
      </div>

      <div class="form-group" style={{ marginTop: '2rem', paddingTop: '1rem', borderTop: '1px solid #ddd' }}>
        <label>{content.nodeId}</label>
        <input
          type="text"
          value={nodeId}
          onInput={(e) => onNodeIdChange((e.target as HTMLInputElement).value)}
          placeholder="Enter Node ID"
        />
        <small class="hint">{content.canSpeedNote}</small>
      </div>

      <div class="button-group">
        <button class="btn-primary" onClick={onSaveNodeId} disabled={!isConnected}>
          {content.saveNodeId}
        </button>
        <button class="btn-secondary" onClick={handleSaveToFlash} disabled={!isConnected}>
          {content.saveAllToFlash}
        </button>
      </div>

      <div class="form-group" style={{ marginTop: '1.5rem', paddingTop: '1.5rem', borderTop: '1px solid #ddd' }}>
        <label style={{ marginBottom: '1rem' }}>{content.importExportParameters}</label>
        <div class="button-group" style={{ marginTop: 0 }}>
          <button class="btn-secondary" onClick={handleExportToJSON} disabled={!localParams || isImporting}>
            {content.exportToJSON}
          </button>
          <button class="btn-secondary" onClick={handleImportFromJSON} disabled={!localParams || isImporting}>
            {isImporting
              ? content.importing({ current: importProgress.current, total: importProgress.total })
              : content.importFromJSON
            }
          </button>
        </div>
        <small class="hint" style={{ display: 'block', marginTop: '0.5rem' }}>
          {content.importExportHint}
        </small>
        <input
          ref={fileInputRef}
          type="file"
          accept=".json"
          style={{ display: 'none' }}
          onChange={handleFileSelected}
        />
      </div>
    </section>
  )
}
