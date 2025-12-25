import { ComponentChildren } from 'preact'
import { useState } from 'preact/hooks'

interface TooltipProps {
  content: string
  children: ComponentChildren
  position?: 'top' | 'bottom' | 'left' | 'right'
}

export default function Tooltip({ content, children, position = 'top' }: TooltipProps) {
  const [isVisible, setIsVisible] = useState(false)

  return (
    <div
      class="tooltip-container"
      onMouseEnter={() => setIsVisible(true)}
      onMouseLeave={() => setIsVisible(false)}
    >
      {children}
      {isVisible && content && (
        <div class={`tooltip tooltip-${position}`}>
          {content}
        </div>
      )}
    </div>
  )
}
