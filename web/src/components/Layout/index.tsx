import { ComponentChildren } from 'preact'
import { useState, useEffect } from 'preact/hooks'
import { useIntlayer } from 'preact-intlayer'
import Sidebar from '@components/Sidebar'
import { SavedDevice } from '@api/inverter'
import { useWebSocket } from '@hooks/useWebSocket'
import { useToast } from '@hooks/useToast'
import { useSwipeGesture } from '@hooks/useSwipeGesture'

interface LayoutProps {
  children: ComponentChildren
  currentSerial?: string
  onQuickScan?: () => void
}

export default function Layout({ children, currentSerial, onQuickScan }: LayoutProps) {
  const content = useIntlayer('layout')
  const [sidebarOpen, setSidebarOpen] = useState(false)
  const [savedDevices, setSavedDevices] = useState<Record<string, SavedDevice>>({})
  const [scanning, setScanning] = useState(false)
  const { showError, showWarning, showInfo } = useToast()
  const [hasConnectedOnce, setHasConnectedOnce] = useState(false)

  // WebSocket connection for real-time device updates
  const { isConnected, sendMessage } = useWebSocket('/ws', {
    onMessage: (message) => {
      switch (message.event) {
        case 'deviceDiscovered':
          // Update device in saved devices
          setSavedDevices(prev => {
            const serial = message.data.serial
            const existing = prev[serial] || {}
            return {
              ...prev,
              [serial]: {
                ...existing,
                nodeId: message.data.nodeId,
                lastSeen: message.data.lastSeen,
                name: message.data.name || existing.name,
              }
            }
          })
          break

        case 'savedDevices':
          if (message.data.devices) {
            setSavedDevices(message.data.devices)
          }
          break

        case 'scanStatus':
          setScanning(message.data.active)
          break
      }
    },
    onOpen: () => {
      if (hasConnectedOnce) {
        showInfo(content.reconnected)
      }
      setHasConnectedOnce(true)
    },
    onClose: () => {
      if (hasConnectedOnce) {
        showWarning(content.connectionLost)
      }
    }
  })

  // Track connection status changes
  useEffect(() => {
    if (!isConnected && hasConnectedOnce) {
      showError(content.attemptingReconnect)
    }
  }, [isConnected, hasConnectedOnce])

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
        scanDisabled={!isConnected}
      />

      <div class={`main-content ${sidebarOpen ? 'sidebar-open' : ''}`}>
        <button class="hamburger-menu" onClick={toggleSidebar}>
          <span></span>
          <span></span>
          <span></span>
        </button>

        {children}
      </div>
    </div>
  )
}
