export interface ProgressBarProps {
  progress: number // 0-100
  label?: string
  showPercentage?: boolean
  indeterminate?: boolean
}

export function ProgressBar({ progress, label, showPercentage = true, indeterminate = false }: ProgressBarProps) {
  const clampedProgress = Math.min(100, Math.max(0, progress))

  return (
    <div class="progress-bar-container">
      {label && <div class="progress-bar-label">{label}</div>}
      <div class="progress-bar-track">
        <div
          class={indeterminate ? "progress-bar-fill progress-bar-indeterminate" : "progress-bar-fill"}
          style={indeterminate ? {} : { width: `${clampedProgress}%` }}
        >
        </div>
      </div>
      {showPercentage && !indeterminate && (
        <div class="progress-bar-percentage-outside">{Math.round(clampedProgress)}%</div>
      )}
    </div>
  )
}
