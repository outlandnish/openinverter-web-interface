import { t, type DeclarationContent } from 'intlayer'

const layoutContent = {
  key: 'layout',
  content: {
    reconnected: t({
      en: 'Reconnected',
      de: 'Wieder verbunden',
    }),
    connectionLost: t({
      en: 'Connection lost',
      de: 'Verbindung verloren',
    }),
    attemptingReconnect: t({
      en: 'Connection lost. Attempting to reconnect...',
      de: 'Verbindung verloren. Versuche erneut zu verbinden...',
    }),
  },
} satisfies DeclarationContent

export default layoutContent
