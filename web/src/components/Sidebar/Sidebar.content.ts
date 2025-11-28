import { t, type DeclarationContent } from 'intlayer'

const sidebarContent = {
  key: 'sidebar',
  content: {
    openInverter: t({
      en: 'Open Inverter',
      de: 'Open Inverter',
    }),
    connected: t({
      en: 'Connected',
      de: 'Verbunden',
    }),
    disconnected: t({
      en: 'Disconnected',
      de: 'Getrennt',
    }),
    systemOverview: t({
      en: 'System Overview',
      de: 'Systemübersicht',
    }),
    settings: t({
      en: 'Settings',
      de: 'Einstellungen',
    }),
    devices: t({
      en: 'Devices',
      de: 'Geräte',
    }),
    noDevicesFound: t({
      en: 'No devices found',
      de: 'Keine Geräte gefunden',
    }),
    scanHint: t({
      en: 'Scan for devices to get started',
      de: 'Scannen Sie nach Geräten, um zu beginnen',
    }),
    unnamedDevice: t({
      en: 'Unnamed Device',
      de: 'Unbenanntes Gerät',
    }),
    cannotScanDisconnected: t({
      en: 'Cannot scan: ESP32 disconnected',
      de: 'Scannen nicht möglich: ESP32 getrennt',
    }),
    scanning: t({
      en: 'Scanning...',
      de: 'Wird gescannt...',
    }),
    quickScan: t({
      en: 'Quick scan for devices',
      de: 'Schnellsuche nach Geräten',
    }),
  },
} satisfies DeclarationContent

export default sidebarContent
