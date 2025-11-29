import { useIntlayer } from 'preact-intlayer'

interface DeviceScannerProps {
  scanning: boolean
  onScan: () => void
  deviceCount: number
  disabled?: boolean
}

export default function DeviceScanner({ scanning, onScan, deviceCount, disabled = false }: DeviceScannerProps) {
  const content = useIntlayer('device-scanner')
  return (
    <div class="scan-controls-inline">
      <div class="scan-buttons">
        {scanning ? (
          <button
            class="scan-icon-only scanning"
            onClick={onScan}
            title={content.stopScanning}
            disabled={disabled}
          >
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <rect x="6" y="6" width="12" height="12" rx="1"></rect>
            </svg>
          </button>
        ) : (
          <button
            class="scan-icon-only"
            onClick={onScan}
            title={disabled ? content.cannotScanDisconnected : content.scanCanBus}
            disabled={disabled}
          >
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <path d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z"></path>
            </svg>
          </button>
        )}
      </div>

      {scanning && (
        <div class="scan-status-inline">
          <div class="spinner"></div>
          <span>{content.scanningCanBus}</span>
        </div>
      )}

      {!scanning && deviceCount > 0 && (
        <div class="scan-result-inline">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
            <circle cx="12" cy="12" r="10"></circle>
            <path d="M9 12l2 2 4-4"></path>
          </svg>
          <span>{deviceCount !== 1 ? content.foundDevicesCountPlural({ count: deviceCount }) : content.foundDevicesCount({ count: deviceCount })}</span>
        </div>
      )}
    </div>
  )
}
