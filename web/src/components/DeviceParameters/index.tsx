import { useState, useEffect } from 'preact/hooks'
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
  const { isConnected } = useWebSocketContext()
  // Parse nodeId to number for fetching params from specific device
  const numericNodeId = parseInt(nodeId)
  const { params, loading, getDisplayName, downloadProgress, downloadTotal } = useParams(serial, isNaN(numericNodeId) ? undefined : numericNodeId)

  const [collapsedSections, setCollapsedSections] = useState<Set<string>>(new Set())
  const [localParams, setLocalParams] = useState(params)

  // Sync localParams with params when params changes (e.g., on initial load or refresh)
  useEffect(() => {
    setLocalParams(params)
  }, [params])

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

  const handleSaveToFlash = async () => {
    try {
      await api.saveParams()
      showSuccess(content.parametersSaved)
    } catch (error) {
      showError(content.saveParametersFailed)
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
    </section>
  )
}
