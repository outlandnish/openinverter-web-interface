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
    connecting: t({
      en: 'Connecting to ESP32',
      de: 'Verbindung zum ESP32',
    }),
    connectingMessage: t({
      en: 'Establishing connection to the device...',
      de: 'Verbindung zum Gerät wird hergestellt...',
    }),
    reconnecting: t({
      en: 'Reconnecting to ESP32',
      de: 'Verbindung zum ESP32 wird wiederhergestellt',
    }),
    reconnectingMessage: t({
      en: 'Connection lost. Attempting to reconnect to the device...',
      de: 'Verbindung verloren. Versuche, die Verbindung zum Gerät wiederherzustellen...',
    }),
    reconnect: t({
      en: 'Reconnect',
      de: 'Erneut verbinden',
    }),
  },
} satisfies DeclarationContent

export default disconnectedStateContent
