import { t, type DeclarationContent } from 'intlayer'

const disconnectedStateContent = {
  key: 'disconnected-state',
  content: {
    title: t({
      en: 'ESP32 Disconnected',
      de: 'ESP32 getrennt',
    }),
    message: t({
      en: 'Unable to establish connection to the device. Please check that your ESP32 is powered on and connected to the network.',
      de: 'Verbindung zum Gerät kann nicht hergestellt werden. Bitte überprüfen Sie, ob Ihr ESP32 eingeschaltet und mit dem Netzwerk verbunden ist.',
    }),
    reconnect: t({
      en: 'Reconnect',
      de: 'Erneut verbinden',
    }),
  },
} satisfies DeclarationContent

export default disconnectedStateContent
