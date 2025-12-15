import { useIntlayer } from 'preact-intlayer'

interface DeviceScannerProps {
  scanning: boolean
  onScan: () => void
  deviceCount: number
  disabled?: boolean
  currentScanNode?: number | null
  scanRange?: { start: number; end: number }
}

export default function DeviceScanner({
  scanning,
  onScan,
  disabled = false,
  currentScanNode,
  scanRange = { start: 0, end: 255 }
}: DeviceScannerProps) {
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
            title={disabled ? content.cannotScanDisconnected : (scanRange ? content.scanCanBus({ start: scanRange.start, end: scanRange.end }) : 'Scan CAN bus')}
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
          <span>{content.scanningCanBus({ start: scanRange.start, end: scanRange.end, current: currentScanNode ?? '' })}</span>
        </div>
      )}
    </div>
  )
}
