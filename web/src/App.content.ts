import { t, type DeclarationContent } from 'intlayer'

const appContent = {
  key: 'app',
  content: {
    title: t({
      en: 'Open Inverter',
      de: 'Open Inverter',
    }),
    loading: t({
      en: 'Loading...',
      de: 'Wird geladen...',
    }),
    settings: {
      title: t({
        en: 'Device Parameters',
        de: 'Geräteparameter',
      }),
      nodeId: t({
        en: 'Node ID:',
        de: 'Knoten-ID:',
      }),
      canSpeed: t({
        en: 'CAN Speed:',
        de: 'CAN-Geschwindigkeit:',
      }),
      saveButton: t({
        en: 'Save Settings',
        de: 'Einstellungen speichern',
      }),
      savedAlert: t({
        en: 'Settings saved!',
        de: 'Einstellungen gespeichert!',
      }),
    },
    info: {
      description: t({
        en: 'This is a Preact-based web interface for your ESP32 inverter controller.',
        de: 'Dies ist eine Preact-basierte Weboberfläche für Ihren ESP32-Wechselrichter-Controller.',
      }),
      devProxy: t({
        en: 'Development server proxies all API calls to http://inverter.local',
        de: 'Entwicklungsserver leitet alle API-Aufrufe an http://inverter.local weiter',
      }),
    },
    notFound: {
      title: t({
        en: '404 - Page Not Found',
        de: '404 - Seite nicht gefunden',
      }),
    },
  },
} satisfies DeclarationContent

export default appContent
