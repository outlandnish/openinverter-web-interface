import { useIntlayer } from 'preact-intlayer'
import { type MergedDevice } from '@contexts/DeviceContext'

interface DeviceListItemProps {
  device: MergedDevice
  isLastConnected?: boolean
  isOnline: boolean
  onSelect: (device: MergedDevice) => void
  onRename: (device: MergedDevice, e: Event) => void
  onDelete: (device: MergedDevice, e: Event) => void
}

export default function DeviceListItem({
  device,
  isLastConnected,
  isOnline,
  onSelect,
  onRename,
  onDelete
}: DeviceListItemProps) {
  const content = useIntlayer('device-list-item')

  return (
    <div
      class="device-card"
      onClick={() => onSelect(device)}
    >
      <div class="device-name">
        {device.name || content.unnamedDevice}
      </div>
      <div class="device-info">
        <div class="device-serial">{content.serial} {device.serial}</div>
        {device.nodeId !== undefined && (
          <div class="device-node">{content.nodeId} {device.nodeId}</div>
        )}
        <div class={`device-status ${isOnline ? 'online' : 'offline'}`}>
          {isOnline ? content.online : content.offline}
        </div>
      </div>
      <div class="device-card-actions">
        {device.name && (
          <button 
            class="device-action-btn" 
            onClick={(e) => onRename(device, e)}
            title={content.rename}
          >
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <path d="M11 4H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h14a2 2 0 0 0 2-2v-7"></path>
              <path d="M18.5 2.5a2.121 2.121 0 0 1 3 3L12 15l-4 1 1-4 9.5-9.5z"></path>
            </svg>
          </button>
        )}
        <button 
          class="device-action-btn device-action-btn-danger" 
          onClick={(e) => onDelete(device, e)}
          title={content.delete}
        >
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <polyline points="3 6 5 6 21 6"></polyline>
            <path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6m3 0V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2"></path>
            <line x1="10" y1="11" x2="10" y2="17"></line>
            <line x1="14" y1="11" x2="14" y2="17"></line>
          </svg>
        </button>
      </div>
    </div>
  )
}
