import { t, type DeclarationContent } from 'intlayer'

const deviceListContent = {
  key: 'device-list',
  content: {
    title: t({
      en: 'Open Inverter Devices',
      de: 'Open Inverter Geräte',
    }),
    settings: t({
      en: '⚙️ Settings',
      de: '⚙️ Einstellungen',
    }),
    scanningForDevices: t({
      en: 'Scanning for devices...',
      de: 'Suche nach Geräten...',
    }),
    noDevicesFound: t({
      en: 'No devices found',
      de: 'Keine Geräte gefunden',
    }),
    scanHint: t({
      en: 'Click "Scan" to discover devices on the CAN bus',
      de: 'Klicken Sie auf „Scannen", um Geräte auf dem CAN-Bus zu entdecken',
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
    rename: t({
      en: 'Rename',
      de: 'Umbenennen',
    }),
    delete: t({
      en: 'Delete',
      de: 'Löschen',
    }),
    confirmDelete: t({
      en: 'Are you sure you want to delete this device?',
      de: 'Sind Sie sicher, dass Sie dieses Gerät löschen möchten?',
    }),
    deviceDeleted: t({
      en: 'Device deleted successfully',
      de: 'Gerät erfolgreich gelöscht',
    }),
    deviceRenamed: t({
      en: 'Device renamed successfully',
      de: 'Gerät erfolgreich umbenannt',
    }),
  },
} satisfies DeclarationContent

export default deviceListContent
