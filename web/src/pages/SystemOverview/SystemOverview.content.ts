import { t, type DeclarationContent } from 'intlayer'

const systemOverviewContent = {
  key: 'system-overview',
  content: {
    title: t({
      en: 'System Overview',
      de: 'Systemübersicht',
    }),
    subtitle: t({
      en: 'Scan and manage Open Inverter devices on your CAN bus',
      de: 'Scannen und verwalten Sie Open Inverter-Geräte auf Ihrem CAN-Bus',
    }),
    noDevicesFound: t({
      en: 'No devices found',
      de: 'Keine Geräte gefunden',
    }),
    scanningForDevices: t({
      en: 'Scanning for devices...',
      de: 'Suche nach Geräten...',
    }),
    searchingNodes: t({
      en: 'Searching nodes 0-255 on the CAN bus',
      de: 'Durchsuche Knoten 0-255 auf dem CAN-Bus',
    }),
    startScanHint: t({
      en: 'Start a scan to discover devices on your CAN bus',
      de: 'Starten Sie einen Scan, um Geräte auf Ihrem CAN-Bus zu entdecken',
    }),
    scanDevices: t({
      en: 'Scan Devices',
      de: 'Geräte scannen',
    }),
    unnamedDevice: t({
      en: 'Unnamed Device',
      de: 'Unbenanntes Gerät',
    }),
    lastConnected: t({
      en: 'Last Connected',
      de: 'Zuletzt verbunden',
    }),
    serial: t({
      en: 'Serial:',
      de: 'Seriennummer:',
    }),
    nodeId: t({
      en: 'Node ID:',
      de: 'Knoten-ID:',
    }),
    online: t({
      en: 'Online',
      de: 'Online',
    }),
    offline: t({
      en: 'Offline',
      de: 'Offline',
    }),
  },
} satisfies DeclarationContent

export default systemOverviewContent
