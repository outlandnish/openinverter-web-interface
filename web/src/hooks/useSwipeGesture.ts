import { useEffect, useRef } from 'preact/hooks'

interface SwipeGestureOptions {
  onSwipeLeft?: () => void
  onSwipeRight?: () => void
  minSwipeDistance?: number
  maxVerticalDistance?: number
}

/**
 * Custom hook for detecting swipe gestures
 * @param options Configuration for swipe detection
 */
export function useSwipeGesture(options: SwipeGestureOptions) {
  const {
    onSwipeLeft,
    onSwipeRight,
    minSwipeDistance = 50,
    maxVerticalDistance = 100
  } = options

  const touchStartRef = useRef<{ x: number; y: number } | null>(null)

  useEffect(() => {
    const handleTouchStart = (e: TouchEvent) => {
      const touch = e.touches[0]
      touchStartRef.current = {
        x: touch.clientX,
        y: touch.clientY
      }
    }

    const handleTouchEnd = (e: TouchEvent) => {
      if (!touchStartRef.current) return

      const touch = e.changedTouches[0]
      const deltaX = touch.clientX - touchStartRef.current.x
      const deltaY = touch.clientY - touchStartRef.current.y

      // Check if vertical movement is within acceptable range
      if (Math.abs(deltaY) > maxVerticalDistance) {
        touchStartRef.current = null
        return
      }

      // Check if horizontal swipe is significant enough
      if (Math.abs(deltaX) < minSwipeDistance) {
        touchStartRef.current = null
        return
      }

      // Determine swipe direction
      if (deltaX > 0 && onSwipeRight) {
        // Swipe right (opening gesture from left edge)
        onSwipeRight()
      } else if (deltaX < 0 && onSwipeLeft) {
        // Swipe left (closing gesture)
        onSwipeLeft()
      }

      touchStartRef.current = null
    }

    const handleTouchCancel = () => {
      touchStartRef.current = null
    }

    document.addEventListener('touchstart', handleTouchStart, { passive: true })
    document.addEventListener('touchend', handleTouchEnd, { passive: true })
    document.addEventListener('touchcancel', handleTouchCancel, { passive: true })

    return () => {
      document.removeEventListener('touchstart', handleTouchStart)
      document.removeEventListener('touchend', handleTouchEnd)
      document.removeEventListener('touchcancel', handleTouchCancel)
    }
  }, [onSwipeLeft, onSwipeRight, minSwipeDistance, maxVerticalDistance])
}
