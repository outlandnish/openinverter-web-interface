import { t, insert, type DeclarationContent } from 'intlayer'

const deviceScannerContent = {
  key: 'device-scanner',
  content: {
    stopScanning: t({
      en: 'Stop scanning',
      de: 'Scannen stoppen',
    }),
    scanCanBus: t({
      en: 'Scan CAN bus (Nodes 0-255)',
      de: 'CAN-Bus scannen (Knoten 0-255)',
    }),
    cannotScanDisconnected: t({
      en: 'Cannot scan: ESP32 disconnected',
      de: 'Scannen nicht möglich: ESP32 getrennt',
    }),
    scanningCanBus: t({
      en: 'Scanning CAN bus (nodes 0-255)...',
      de: 'CAN-Bus wird gescannt (Knoten 0-255)...',
    }),
    foundDevicesCount: insert({
      en: 'Found {{count}} device',
      de: 'Gefunden {{count}} Gerät',
    }),
    foundDevicesCountPlural: insert({
      en: 'Found {{count}} devices',
      de: 'Gefunden {{count}} Geräte',
    }),
  },
} satisfies DeclarationContent

export default deviceScannerContent
