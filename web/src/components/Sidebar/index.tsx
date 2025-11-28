import { useEffect, useState } from 'preact/hooks'
import { useLocation } from 'wouter'
import { useIntlayer } from 'preact-intlayer'
import { SavedDevice } from '@api/inverter'
import ConnectionStatus from '@components/ConnectionStatus'

interface SidebarProps {
  devices: Record<string, SavedDevice>
  isOpen: boolean
  onToggle: () => void
  currentSerial?: string
  onQuickScan?: () => void
  scanning?: boolean
  wsConnected?: boolean
  scanDisabled?: boolean
}

export default function Sidebar({ devices, isOpen, onToggle, currentSerial, onQuickScan, scanning, wsConnected, scanDisabled = false }: SidebarProps) {
  const [location, setLocation] = useLocation()
  const content = useIntlayer('sidebar')

  const deviceEntries = Object.entries(devices).sort((a, b) => {
    // Sort by name if available, otherwise by serial
    const nameA = a[1].name || a[0]
    const nameB = b[1].name || b[0]
    return nameA.localeCompare(nameB)
  })

  const handleDeviceClick = (serial: string) => {
    setLocation(`/devices/${serial}`)
    // Close sidebar on mobile after selection
    if (window.innerWidth < 768) {
      onToggle()
    }
  }

  const handleOverviewClick = () => {
    setLocation('/')
    if (window.innerWidth < 768) {
      onToggle()
    }
  }

  const handleSettingsClick = () => {
    setLocation('/settings')
    if (window.innerWidth < 768) {
      onToggle()
    }
  }

  return (
    <>
      {/* Overlay for mobile */}
      {isOpen && (
        <div class="sidebar-overlay" onClick={onToggle}></div>
      )}

      {/* Sidebar */}
      <aside class={`sidebar ${isOpen ? 'sidebar-open' : ''}`}>
        <div class="sidebar-header">
          <div class="sidebar-branding">
            <div class="sidebar-branding-top">
              <img src="/openinverter-logo.png" alt="OpenInverter" class="sidebar-logo" />
              <h2>{content.openInverter}</h2>
            </div>
            <div class="sidebar-ws-status">
              <ConnectionStatus
                connected={wsConnected ?? false}
                label={wsConnected ? content.connected : content.disconnected}
              />
            </div>
          </div>
          <button class="sidebar-close" onClick={onToggle}>
            ✕
          </button>
        </div>

        <nav class="sidebar-nav">
          <div class="sidebar-section">
            <button
              class={`sidebar-nav-item ${!currentSerial && location.pathname === '/' ? 'active' : ''}`}
              onClick={handleOverviewClick}
            >
              <span class="nav-icon">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                  <rect x="3" y="3" width="7" height="7"></rect>
                  <rect x="14" y="3" width="7" height="7"></rect>
                  <rect x="14" y="14" width="7" height="7"></rect>
                  <rect x="3" y="14" width="7" height="7"></rect>
                </svg>
              </span>
              <span class="nav-label">{content.systemOverview}</span>
            </button>

            <button
              class={`sidebar-nav-item ${location.pathname === '/settings' ? 'active' : ''}`}
              onClick={handleSettingsClick}
            >
              <span class="nav-icon">
                <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                  <circle cx="12" cy="12" r="3"></circle>
                  <path d="M12 1v6m0 6v6M1 12h6m6 0h6"></path>
                  <path d="m4.93 4.93 4.24 4.24m5.66 5.66 4.24 4.24M4.93 19.07l4.24-4.24m5.66-5.66 4.24-4.24"></path>
                </svg>
              </span>
              <span class="nav-label">{content.settings}</span>
            </button>
          </div>

          <div class="sidebar-section">
            <div class="sidebar-section-header">
              <h3 class="sidebar-section-title">{content.devices}</h3>
              {onQuickScan && (
                <button
                  class={`sidebar-scan-button ${scanning ? 'scanning' : ''}`}
                  onClick={onQuickScan}
                  title={scanDisabled ? content.cannotScanDisconnected : (scanning ? content.scanning : content.quickScan)}
                  disabled={scanning || scanDisabled}
                >
                  {scanning ? (
                    <svg class="scan-spinner" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                      <circle cx="12" cy="12" r="10" opacity="0.25"></circle>
                      <path d="M12 2a10 10 0 0 1 10 10" stroke-linecap="round"></path>
                    </svg>
                  ) : (
                    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                      <path d="M21 21l-6-6m2-5a7 7 0 11-14 0 7 7 0 0114 0z"></path>
                    </svg>
                  )}
                </button>
              )}
            </div>
            {deviceEntries.length === 0 ? (
              <div class="sidebar-empty">
                <p>{content.noDevicesFound}</p>
                <p class="hint">{content.scanHint}</p>
              </div>
            ) : (
              <div class="sidebar-devices">
                {deviceEntries.map(([serial, device]) => (
                  <button
                    key={serial}
                    class={`sidebar-device-item ${serial === currentSerial ? 'active' : ''}`}
                    onClick={() => handleDeviceClick(serial)}
                  >
                    <div class="device-item-name">
                      {device.name || content.unnamedDevice}
                    </div>
                    <div class="device-item-serial">
                      {serial.substring(0, 8)}...
                    </div>
                    {device.lastSeen && (
                      <div class="device-item-status online">●</div>
                    )}
                  </button>
                ))}
              </div>
            )}
          </div>
        </nav>
      </aside>
    </>
  )
}
