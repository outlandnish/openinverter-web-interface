import { useState, useEffect } from 'preact/hooks'
import { useIntlayer } from 'preact-intlayer'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { useToast } from '@hooks/useToast'
import { ProgressBar } from '@components/ProgressBar'

interface OTAUpdateProps {
  onUploadStart?: () => void
  onUploadComplete?: () => void
  onUploadError?: (error: string) => void
}

export default function OTAUpdate({
  onUploadStart,
  onUploadComplete,
  onUploadError
}: OTAUpdateProps) {
  const content = useIntlayer('device-details')
  const { showError, showSuccess } = useToast()
  const { subscribe } = useWebSocketContext()

  const [otaFile, setOtaFile] = useState<File | null>(null)
  const [otaProgress, setOtaProgress] = useState<number>(0)
  const [otaStatus, setOtaStatus] = useState<'idle' | 'uploading' | 'updating' | 'success' | 'error'>('idle')
  const [otaError, setOtaError] = useState<string>('')

  // Subscribe to WebSocket messages for OTA progress
  useEffect(() => {
    const unsubscribe = subscribe((message: any) => {
      switch (message.event) {
        case 'otaProgress':
          setOtaProgress(message.data.progress)
          setOtaStatus('updating')
          break

        case 'otaSuccess':
          setOtaProgress(100)
          setOtaStatus('success')
          showSuccess(content.updateSuccessful)
          onUploadComplete?.()
          break

        case 'otaError':
          const otaErrorMsg = message.data.error || 'Unknown error occurred'
          setOtaStatus('error')
          setOtaError(otaErrorMsg)
          showError(content.otaUpdateFailedWithError({ error: otaErrorMsg }))
          onUploadError?.(otaErrorMsg)
          break
      }
    })

    return unsubscribe
  }, [subscribe])

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

    try {
      setOtaStatus('uploading')
      setOtaProgress(0)
      setOtaError('')
      onUploadStart?.()

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
      showError(content.otaUpdateFailedWithError({ error: errorMessage }))
      onUploadError?.(errorMessage)
    }
  }

  const handleOtaReset = () => {
    setOtaFile(null)
    setOtaProgress(0)
    setOtaStatus('idle')
    setOtaError('')
  }

  return (
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
              disabled={!otaFile}
            >
              {content.startFirmwareUpdate}
            </button>
          </div>
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

          <ProgressBar
            progress={otaProgress}
            label={otaStatus === 'uploading' ? content.transferringFirmware : content.installingFirmware}
            showPercentage={true}
          />
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
  )
}
