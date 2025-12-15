import { useState } from 'preact/hooks'
import { useIntlayer } from 'preact-intlayer'
import { useParams } from '@hooks/useParams'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { useToast } from '@hooks/useToast'
import { api } from '@api/inverter'
import ParameterCategory from './ParameterCategory'
import ProgressBar from '@components/ProgressBar'

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
  const { params, loading, refresh: refreshParams, getDisplayName, downloadProgress } = useParams(serial, isNaN(numericNodeId) ? undefined : numericNodeId)

  const [collapsedSections, setCollapsedSections] = useState<Set<string>>(new Set())

  const toggleSection = (category: string) => {
    const newCollapsed = new Set(collapsedSections)
    if (newCollapsed.has(category)) {
      newCollapsed.delete(category)
    } else {
      newCollapsed.add(category)
    }
    setCollapsedSections(newCollapsed)
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
    return (
      <section id="device-parameters" class="card">
        <h2 class="section-header">{content.deviceParameters}</h2>
        <div class="loading">
          {downloadProgress > 0 ? (
            <div style={{ width: '100%', maxWidth: '500px' }}>
              <ProgressBar 
                progress={downloadProgress} 
                label="Downloading parameter definitions..." 
              />
            </div>
          ) : (
            <span>Loading parameters...</span>
          )}
        </div>
      </section>
    )
  }

  if (!params) {
    return (
      <section id="device-parameters" class="card">
        <h2 class="section-header">{content.deviceParameters}</h2>
        <div class="error-message">No parameters loaded</div>
      </section>
    )
  }

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
            onUpdate={refreshParams}
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
