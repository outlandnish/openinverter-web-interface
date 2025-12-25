import { ComponentChildren } from 'preact'
import { useState, useEffect, useRef } from 'preact/hooks'
import { useIntlayer } from 'preact-intlayer'
import Sidebar from '@components/Sidebar'
import { useWebSocketContext } from '@contexts/WebSocketContext'
import { useDeviceContext } from '@contexts/DeviceContext'
import { useToast } from '@hooks/useToast'
import { useSwipeGesture } from '@hooks/useSwipeGesture'

interface LayoutProps {
  children: ComponentChildren
  currentSerial?: string
  onQuickScan?: () => void
  pageTitle?: string
  onTitleClick?: () => void
}

export default function Layout({ children, currentSerial, onQuickScan, pageTitle, onTitleClick }: LayoutProps) {
  const content = useIntlayer('layout')
  const [sidebarOpen, setSidebarOpen] = useState(false)
  const { showError, showWarning, showInfo } = useToast()
  const prevConnectedRef = useRef<boolean>(false)
  const hasConnectedOnceRef = useRef<boolean>(false)

  // Use shared WebSocket connection
  const { isConnected, isConnecting } = useWebSocketContext()

  // Use shared Device context
  const { savedDevices, scanning } = useDeviceContext()

  // Handle connection state changes
  useEffect(() => {
    // Only show messages if connection state actually changed
    if (isConnected !== prevConnectedRef.current) {
      if (isConnected) {
        if (hasConnectedOnceRef.current) {
          showInfo(content.reconnected)
        } else {
          hasConnectedOnceRef.current = true
        }
      } else if (hasConnectedOnceRef.current) {
        showWarning(content.connectionLost)
        if (isConnecting) {
          showInfo(content.connecting)
        } else {
          showError(content.attemptingReconnect)
        }
      }
      prevConnectedRef.current = isConnected
    }
  }, [isConnected, isConnecting])

  // Auto-open sidebar on desktop
  useEffect(() => {
    const handleResize = () => {
      if (window.innerWidth >= 768) {
        setSidebarOpen(true)
      } else {
        setSidebarOpen(false)
      }
    }

    handleResize()
    window.addEventListener('resize', handleResize)
    return () => window.removeEventListener('resize', handleResize)
  }, [])

  const toggleSidebar = () => {
    setSidebarOpen(!sidebarOpen)
  }

  const openSidebar = () => {
    setSidebarOpen(true)
  }

  const closeSidebar = () => {
    setSidebarOpen(false)
  }

  // Enable swipe gestures on mobile
  useSwipeGesture({
    onSwipeRight: () => {
      // Only open on swipe right if on mobile and sidebar is closed
      if (window.innerWidth < 768 && !sidebarOpen) {
        openSidebar()
      }
    },
    onSwipeLeft: () => {
      // Only close on swipe left if sidebar is open
      if (sidebarOpen) {
        closeSidebar()
      }
    },
    minSwipeDistance: 50,
    maxVerticalDistance: 100
  })

  return (
    <div class="app-layout">
      <Sidebar
        devices={savedDevices}
        isOpen={sidebarOpen}
        onToggle={toggleSidebar}
        currentSerial={currentSerial}
        onQuickScan={onQuickScan}
        scanning={scanning}
        wsConnected={isConnected}
        wsConnecting={isConnecting}
        scanDisabled={!isConnected}
      />

      {/* Mobile Header Bar */}
      <div class="mobile-header-bar">
        <button class="hamburger-menu" onClick={toggleSidebar}>
          <span></span>
          <span></span>
          <span></span>
        </button>
        {pageTitle && (
          <div
            class={`mobile-header-title ${onTitleClick ? 'clickable' : ''}`}
            onClick={onTitleClick}
          >
            {pageTitle}
          </div>
        )}
      </div>

      <div class={`main-content ${sidebarOpen ? 'sidebar-open' : ''}`}>
        {children}
      </div>
    </div>
  )
}
