import { useState } from 'preact/hooks'
import { useIntlayer } from 'preact-intlayer'
import { useCanMappings, CanMapping } from '@hooks/useCanMappings'
import { useParams } from '@hooks/useParams'
import { useToast } from '@hooks/useToast'
import { getParameterDisplayName } from '@utils/paramStorage'
import './styles.css'

interface CanMappingEditorProps {
  serial: string
  nodeId: number
}

export default function CanMappingEditor({ serial, nodeId }: CanMappingEditorProps) {
  const content = useIntlayer('can-mapping-editor')
  const { mappings, loading, error, addMapping, removeMapping } = useCanMappings()
  const { params } = useParams(serial, nodeId)
  const { showError, showSuccess } = useToast()

  // Form state for adding new mapping
  const [showAddForm, setShowAddForm] = useState(false)
  const [formData, setFormData] = useState({
    isrx: false,
    id: 0,
    paramid: 0,
    position: 0,
    length: 16,
    gain: 1.0,
    offset: 0,
  })

  // Get parameter display name by ID
  const getParamName = (paramId: number): string => {
    if (!params) return content.paramFallback({ paramId })

    for (const [key, param] of Object.entries(params)) {
      if (param.id === paramId) {
        return getParameterDisplayName(key, param)
      }
    }
    return content.paramFallback({ paramId })
  }

  const handleAddMapping = async () => {
    try {
      await addMapping(formData)
      showSuccess(content.addSuccess)
      setShowAddForm(false)
      // Reset form
      setFormData({
        isrx: false,
        id: 0,
        paramid: 0,
        position: 0,
        length: 16,
        gain: 1.0,
        offset: 0,
      })
    } catch (err) {
      showError(err instanceof Error ? err.message : content.addError)
    }
  }

  const handleRemoveMapping = async (mapping: CanMapping) => {
    const direction = mapping.isrx ? 'RX' : 'TX'
    const paramName = getParamName(mapping.paramid)
    const canId = `0x${mapping.id.toString(16).toUpperCase()}`

    if (!confirm(content.removeConfirmation({ direction, paramName, canId }))) {
      return
    }

    try {
      await removeMapping(mapping.index, mapping.subindex)
      showSuccess(content.removeSuccess)
    } catch (err) {
      showError(err instanceof Error ? err.message : content.removeError)
    }
  }

  const txMappings = mappings.filter(m => !m.isrx)
  const rxMappings = mappings.filter(m => m.isrx)

  return (
    <section id="can-mappings" class="card">
      <h2 class="section-header">{content.title}</h2>

      {loading && (
        <div class="loading">
          <p>{content.loading}</p>
        </div>
      )}

      {error && (
        <div class="error-message">
          <p>{content.errorPrefix} {error}</p>
        </div>
      )}

      {!loading && !error && (
        <div class="can-mappings-container">
          {/* TX Mappings */}
          <div class="mapping-section">
            <h3>{content.txSection}</h3>
            {txMappings.length === 0 ? (
              <p class="no-mappings">{content.noTxMappings}</p>
            ) : (
              <table class="mappings-table">
                <thead>
                  <tr>
                    <th>{content.tableHeaderCanId}</th>
                    <th>{content.tableHeaderParameter}</th>
                    <th>{content.tableHeaderPosition}</th>
                    <th>{content.tableHeaderLength}</th>
                    <th>{content.tableHeaderGain}</th>
                    <th>{content.tableHeaderOffset}</th>
                    <th>{content.tableHeaderActions}</th>
                  </tr>
                </thead>
                <tbody>
                  {txMappings.map((mapping, idx) => (
                    <tr key={idx}>
                      <td>0x{mapping.id.toString(16).toUpperCase()}</td>
                      <td>{getParamName(mapping.paramid)}</td>
                      <td>{mapping.position}</td>
                      <td>{mapping.length} {content.bitsUnit}</td>
                      <td>{mapping.gain}</td>
                      <td>{mapping.offset}</td>
                      <td>
                        <button
                          class="btn-remove"
                          onClick={() => handleRemoveMapping(mapping)}
                        >
                          {content.removeButton}
                        </button>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
          </div>

          {/* RX Mappings */}
          <div class="mapping-section">
            <h3>{content.rxSection}</h3>
            {rxMappings.length === 0 ? (
              <p class="no-mappings">{content.noRxMappings}</p>
            ) : (
              <table class="mappings-table">
                <thead>
                  <tr>
                    <th>{content.tableHeaderCanId}</th>
                    <th>{content.tableHeaderParameter}</th>
                    <th>{content.tableHeaderPosition}</th>
                    <th>{content.tableHeaderLength}</th>
                    <th>{content.tableHeaderGain}</th>
                    <th>{content.tableHeaderOffset}</th>
                    <th>{content.tableHeaderActions}</th>
                  </tr>
                </thead>
                <tbody>
                  {rxMappings.map((mapping, idx) => (
                    <tr key={idx}>
                      <td>0x{mapping.id.toString(16).toUpperCase()}</td>
                      <td>{getParamName(mapping.paramid)}</td>
                      <td>{mapping.position}</td>
                      <td>{mapping.length} {content.bitsUnit}</td>
                      <td>{mapping.gain}</td>
                      <td>{mapping.offset}</td>
                      <td>
                        <button
                          class="btn-remove"
                          onClick={() => handleRemoveMapping(mapping)}
                        >
                          {content.removeButton}
                        </button>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            )}
          </div>

          {/* Add Mapping Button */}
          <div class="add-mapping-section">
            {!showAddForm ? (
              <button class="btn-add" onClick={() => setShowAddForm(true)}>
                {content.addMappingButton}
              </button>
            ) : (
              <div class="add-mapping-form">
                <h3>{content.addMappingFormTitle}</h3>
                <div class="form-row">
                  <label>
                    {content.directionLabel}
                    <select
                      value={formData.isrx ? 'rx' : 'tx'}
                      onChange={(e) => setFormData({ ...formData, isrx: (e.currentTarget as HTMLSelectElement).value === 'rx' })}
                    >
                      <option value="tx">{content.directionTx}</option>
                      <option value="rx">{content.directionRx}</option>
                    </select>
                  </label>

                  <label>
                    {content.canIdLabel}
                    <input
                      type="text"
                      placeholder={content.canIdPlaceholder}
                      value={`0x${formData.id.toString(16).toUpperCase()}`}
                      onChange={(e) => {
                        const value = (e.currentTarget as HTMLInputElement).value
                        const parsed = parseInt(value, 16)
                        if (!isNaN(parsed)) {
                          setFormData({ ...formData, id: parsed })
                        }
                      }}
                    />
                  </label>
                </div>

                <div class="form-row">
                  <label>
                    {content.parameterLabel}
                    <select
                      value={formData.paramid}
                      onChange={(e) => setFormData({ ...formData, paramid: parseInt((e.currentTarget as HTMLSelectElement).value) })}
                    >
                      <option value={0}>{content.selectParameter}</option>
                      {params && Object.entries(params).map(([key, param]) => (
                        <option key={param.id} value={param.id}>
                          {getParameterDisplayName(key, param)} (ID: {param.id})
                        </option>
                      ))}
                    </select>
                  </label>

                  <label>
                    {content.bitPositionLabel}
                    <input
                      type="number"
                      min="0"
                      max="63"
                      value={formData.position}
                      onChange={(e) => setFormData({ ...formData, position: parseInt((e.currentTarget as HTMLInputElement).value) || 0 })}
                    />
                  </label>

                  <label>
                    {content.bitLengthLabel}
                    <input
                      type="number"
                      min="1"
                      max="32"
                      value={formData.length}
                      onChange={(e) => setFormData({ ...formData, length: parseInt((e.currentTarget as HTMLInputElement).value) || 16 })}
                    />
                  </label>
                </div>

                <div class="form-row">
                  <label>
                    {content.gainLabel}
                    <input
                      type="number"
                      step="0.001"
                      value={formData.gain}
                      onChange={(e) => setFormData({ ...formData, gain: parseFloat((e.currentTarget as HTMLInputElement).value) || 1.0 })}
                    />
                  </label>

                  <label>
                    {content.offsetLabel}
                    <input
                      type="number"
                      value={formData.offset}
                      onChange={(e) => setFormData({ ...formData, offset: parseInt((e.currentTarget as HTMLInputElement).value) || 0 })}
                    />
                  </label>
                </div>

                <div class="form-actions">
                  <button class="btn-cancel" onClick={() => setShowAddForm(false)}>
                    {content.cancelButton}
                  </button>
                  <button
                    class="btn-save"
                    onClick={handleAddMapping}
                    disabled={formData.paramid === 0}
                  >
                    {content.addButton}
                  </button>
                </div>
              </div>
            )}
          </div>
        </div>
      )}
    </section>
  )
}
