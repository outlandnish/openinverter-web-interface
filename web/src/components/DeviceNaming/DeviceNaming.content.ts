import { t, type DeclarationContent } from 'intlayer'

const deviceNamingContent = {
  key: 'device-naming',
  content: {
    title: t({
      en: 'Name Your Device',
      de: 'Benennen Sie Ihr Gerät',
    }),
    serial: t({
      en: 'Serial:',
      de: 'Seriennummer:',
    }),
    deviceName: t({
      en: 'Device Name',
      de: 'Gerätename',
    }),
    placeholder: t({
      en: 'e.g., Main Inverter',
      de: 'z.B. Haupt-Wechselrichter',
    }),
    cancel: t({
      en: 'Cancel',
      de: 'Abbrechen',
    }),
    saveAndConnect: t({
      en: 'Save & Connect',
      de: 'Speichern & Verbinden',
    }),
  },
} satisfies DeclarationContent

export default deviceNamingContent
