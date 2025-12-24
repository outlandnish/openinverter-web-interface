import { t, type DeclarationContent } from 'intlayer'

const deviceListItemContent = {
  key: 'device-list-item',
  content: {
    unnamedDevice: t({
      en: 'Unnamed Device',
      de: 'Unbenanntes Gerät',
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
    rename: t({
      en: 'Rename',
      de: 'Umbenennen',
    }),
    delete: t({
      en: 'Delete',
      de: 'Löschen',
    }),
  },
} satisfies DeclarationContent

export default deviceListItemContent
