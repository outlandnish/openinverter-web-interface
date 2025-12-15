interface ProgressBarProps {
  progress: number // 0-100
  label?: string
  showPercentage?: boolean
}

export default function ProgressBar({ progress, label, showPercentage = true }: ProgressBarProps) {
  const clampedProgress = Math.min(100, Math.max(0, progress))

  return (
    <div class="progress-bar-container">
      {label && <div class="progress-bar-label">{label}</div>}
      <div class="progress-bar-track">
        <div
          class="progress-bar-fill"
          style={{ width: `${clampedProgress}%` }}
        >
          {showPercentage && clampedProgress > 10 && (
            <span class="progress-bar-percentage">{Math.round(clampedProgress)}%</span>
          )}
        </div>
      </div>
      {showPercentage && clampedProgress <= 10 && (
        <div class="progress-bar-percentage-outside">{Math.round(clampedProgress)}%</div>
      )}
    </div>
  )
}
