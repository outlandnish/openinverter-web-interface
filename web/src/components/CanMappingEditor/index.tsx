import { useState } from 'preact/hooks'
import { useCanMappings, CanMapping } from '@hooks/useCanMappings'
import { useParams } from '@hooks/useParams'
import { useToast } from '@hooks/useToast'
import './styles.css'

interface CanMappingEditorProps {
  serial: string
  nodeId: number
}

export default function CanMappingEditor({ serial, nodeId }: CanMappingEditorProps) {
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

  // Get parameter name by ID
  const getParamName = (paramId: number): string => {
    if (!params) return `Param ${paramId}`

    for (const [key, param] of Object.entries(params)) {
      if (param.id === paramId) {
        return key
      }
    }
    return `Param ${paramId}`
  }

  const handleAddMapping = async () => {
    try {
      await addMapping(formData)
      showSuccess('CAN mapping added successfully')
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
      showError(err instanceof Error ? err.message : 'Failed to add CAN mapping')
    }
  }

  const handleRemoveMapping = async (mapping: CanMapping) => {
    if (!confirm(`Remove ${mapping.isrx ? 'RX' : 'TX'} mapping for ${getParamName(mapping.paramid)} (CAN ID 0x${mapping.id.toString(16).toUpperCase()})?`)) {
      return
    }

    try {
      await removeMapping(mapping.index, mapping.subindex)
      showSuccess('CAN mapping removed successfully')
    } catch (err) {
      showError(err instanceof Error ? err.message : 'Failed to remove CAN mapping')
    }
  }

  const txMappings = mappings.filter(m => !m.isrx)
  const rxMappings = mappings.filter(m => m.isrx)

  return (
    <section id="can-mappings" class="card">
      <h2 class="section-header">CAN Mappings</h2>

      {loading && (
        <div class="loading">
          <p>Loading CAN mappings...</p>
        </div>
      )}

      {error && (
        <div class="error-message">
          <p>Error: {error}</p>
        </div>
      )}

      {!loading && !error && (
        <div class="can-mappings-container">
          {/* TX Mappings */}
          <div class="mapping-section">
            <h3>TX Mappings (Transmit)</h3>
            {txMappings.length === 0 ? (
              <p class="no-mappings">No TX mappings configured</p>
            ) : (
              <table class="mappings-table">
                <thead>
                  <tr>
                    <th>CAN ID</th>
                    <th>Parameter</th>
                    <th>Position</th>
                    <th>Length</th>
                    <th>Gain</th>
                    <th>Offset</th>
                    <th>Actions</th>
                  </tr>
                </thead>
                <tbody>
                  {txMappings.map((mapping, idx) => (
                    <tr key={idx}>
                      <td>0x{mapping.id.toString(16).toUpperCase()}</td>
                      <td>{getParamName(mapping.paramid)}</td>
                      <td>{mapping.position}</td>
                      <td>{mapping.length} bits</td>
                      <td>{mapping.gain}</td>
                      <td>{mapping.offset}</td>
                      <td>
                        <button
                          class="btn-remove"
                          onClick={() => handleRemoveMapping(mapping)}
                        >
                          Remove
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
            <h3>RX Mappings (Receive)</h3>
            {rxMappings.length === 0 ? (
              <p class="no-mappings">No RX mappings configured</p>
            ) : (
              <table class="mappings-table">
                <thead>
                  <tr>
                    <th>CAN ID</th>
                    <th>Parameter</th>
                    <th>Position</th>
                    <th>Length</th>
                    <th>Gain</th>
                    <th>Offset</th>
                    <th>Actions</th>
                  </tr>
                </thead>
                <tbody>
                  {rxMappings.map((mapping, idx) => (
                    <tr key={idx}>
                      <td>0x{mapping.id.toString(16).toUpperCase()}</td>
                      <td>{getParamName(mapping.paramid)}</td>
                      <td>{mapping.position}</td>
                      <td>{mapping.length} bits</td>
                      <td>{mapping.gain}</td>
                      <td>{mapping.offset}</td>
                      <td>
                        <button
                          class="btn-remove"
                          onClick={() => handleRemoveMapping(mapping)}
                        >
                          Remove
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
                Add CAN Mapping
              </button>
            ) : (
              <div class="add-mapping-form">
                <h3>Add New CAN Mapping</h3>
                <div class="form-row">
                  <label>
                    Direction:
                    <select
                      value={formData.isrx ? 'rx' : 'tx'}
                      onChange={(e) => setFormData({ ...formData, isrx: (e.currentTarget as HTMLSelectElement).value === 'rx' })}
                    >
                      <option value="tx">TX (Transmit)</option>
                      <option value="rx">RX (Receive)</option>
                    </select>
                  </label>

                  <label>
                    CAN ID (hex):
                    <input
                      type="text"
                      placeholder="0x180"
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
                    Parameter:
                    <select
                      value={formData.paramid}
                      onChange={(e) => setFormData({ ...formData, paramid: parseInt((e.currentTarget as HTMLSelectElement).value) })}
                    >
                      <option value={0}>Select parameter...</option>
                      {params && Object.entries(params).map(([key, param]) => (
                        <option key={param.id} value={param.id}>
                          {key} (ID: {param.id})
                        </option>
                      ))}
                    </select>
                  </label>

                  <label>
                    Bit Position:
                    <input
                      type="number"
                      min="0"
                      max="63"
                      value={formData.position}
                      onChange={(e) => setFormData({ ...formData, position: parseInt((e.currentTarget as HTMLInputElement).value) || 0 })}
                    />
                  </label>

                  <label>
                    Bit Length:
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
                    Gain:
                    <input
                      type="number"
                      step="0.001"
                      value={formData.gain}
                      onChange={(e) => setFormData({ ...formData, gain: parseFloat((e.currentTarget as HTMLInputElement).value) || 1.0 })}
                    />
                  </label>

                  <label>
                    Offset:
                    <input
                      type="number"
                      value={formData.offset}
                      onChange={(e) => setFormData({ ...formData, offset: parseInt((e.currentTarget as HTMLInputElement).value) || 0 })}
                    />
                  </label>
                </div>

                <div class="form-actions">
                  <button class="btn-cancel" onClick={() => setShowAddForm(false)}>
                    Cancel
                  </button>
                  <button
                    class="btn-save"
                    onClick={handleAddMapping}
                    disabled={formData.paramid === 0}
                  >
                    Add Mapping
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
