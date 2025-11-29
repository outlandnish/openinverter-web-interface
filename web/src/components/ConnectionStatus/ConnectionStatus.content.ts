import { t, type DeclarationContent } from 'intlayer'

const connectionStatusContent = {
  key: 'connection-status',
  content: {
    connected: t({
      en: 'Connected',
      de: 'Verbunden',
    }),
    connecting: t({
      en: 'Connecting...',
      de: 'Verbinden...',
    }),
    disconnected: t({
      en: 'Disconnected',
      de: 'Getrennt',
    }),
  },
} satisfies DeclarationContent

export default connectionStatusContent
