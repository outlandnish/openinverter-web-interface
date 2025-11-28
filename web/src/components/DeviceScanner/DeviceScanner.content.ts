import { t, type DeclarationContent } from 'intlayer'

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
    foundDevices: t({
      en: 'Found',
      de: 'Gefunden',
    }),
    device: t({
      en: 'device',
      de: 'Gerät',
    }),
    devices: t({
      en: 'devices',
      de: 'Geräte',
    }),
  },
} satisfies DeclarationContent

export default deviceScannerContent
