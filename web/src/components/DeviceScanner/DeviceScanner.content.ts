import { t, insert, type DeclarationContent } from 'intlayer'

const deviceScannerContent = {
  key: 'device-scanner',
  content: {
    stopScanning: t({
      en: 'Stop scanning',
      de: 'Scannen stoppen',
    }),
    scanCanBus: insert(
      t({
        en: 'Scan CAN bus (Nodes {{start}}-{{end}})',
        de: 'CAN-Bus scannen (Knoten {{start}}-{{end}})',
      })
    ),
    cannotScanDisconnected: t({
      en: 'Cannot scan: ESP32 disconnected',
      de: 'Scannen nicht möglich: ESP32 getrennt',
    }),
    scanningCanBus: insert(
      t({
        en: 'Scanning CAN bus (nodes {{start}}-{{end}}): Node {{current}}',
        de: 'CAN-Bus wird gescannt (Knoten {{start}}-{{end}}): Node {{current}}',
      })
    ),
  },
} satisfies DeclarationContent

export default deviceScannerContent
