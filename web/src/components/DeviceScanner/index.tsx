import { useIntlayer } from 'preact-intlayer'

interface DeviceScannerProps {
  scanning: boolean
  currentScanNode?: number | null
  scanRange?: { start: number; end: number }
}

export default function DeviceScanner({
  scanning,
  currentScanNode,
  scanRange = { start: 0, end: 255 }
}: DeviceScannerProps) {
  const content = useIntlayer('device-scanner')

  if (!scanning) {
    return null
  }

  return (
    <div class="scan-controls-inline">
      <div class="scan-status-inline">
        <div class="spinner"></div>
        <span>{content.scanningCanBus({ start: scanRange.start, end: scanRange.end, current: currentScanNode ?? '' })}</span>
      </div>
    </div>
  )
}
