import { ComponentChildren } from 'preact'
import { useState } from 'preact/hooks'
import './styles.css'

export interface Tab {
  id: string
  label: string
  icon?: string
  content: ComponentChildren
  disabled?: boolean
}

interface TabsProps {
  tabs: Tab[]
  defaultTab?: string
  onTabChange?: (tabId: string) => void
}

export default function Tabs({ tabs, defaultTab, onTabChange }: TabsProps) {
  const [activeTab, setActiveTab] = useState(defaultTab || tabs[0]?.id || '')

  const handleTabClick = (tabId: string, disabled?: boolean) => {
    if (disabled) return

    setActiveTab(tabId)
    onTabChange?.(tabId)
  }

  const activeTabContent = tabs.find(tab => tab.id === activeTab)?.content

  return (
    <div class="tabs-container">
      {/* Mobile select dropdown */}
      <div class="tabs-mobile-select">
        <select
          value={activeTab}
          onChange={(e) => handleTabClick((e.target as HTMLSelectElement).value)}
          class="tab-select"
        >
          {tabs.map(tab => (
            <option key={tab.id} value={tab.id} disabled={tab.disabled}>
              {tab.label}
            </option>
          ))}
        </select>
      </div>

      {/* Desktop tab navigation */}
      <div class="tabs-header">
        <div class="tabs-nav">
          {tabs.map(tab => (
            <button
              key={tab.id}
              class={`tab-button ${activeTab === tab.id ? 'active' : ''} ${tab.disabled ? 'disabled' : ''}`}
              onClick={() => handleTabClick(tab.id, tab.disabled)}
              disabled={tab.disabled}
            >
              {tab.icon && <span class="tab-icon">{tab.icon}</span>}
              <span class="tab-label">{tab.label}</span>
            </button>
          ))}
        </div>
      </div>
      <div class="tabs-content">
        {activeTabContent}
      </div>
    </div>
  )
}
