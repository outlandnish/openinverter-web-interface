import { t, type DeclarationContent } from 'intlayer'

const deviceControlContent = {
  key: 'device-control',
  content: {
    title: t({
      en: 'Device Control',
    }),
    currentErrors: t({
      en: 'Current Errors',
    }),
    refreshErrors: t({
      en: 'Refresh',
    }),
    errorCode: t({
      en: 'Error Code',
    }),
    errorTime: t({
      en: 'Time',
    }),
    noTimestamp: t({
      en: 'N/A',
    }),
    noErrors: t({
      en: 'No errors detected',
    }),
    deviceActions: t({
      en: 'Device Actions',
    }),
    startDevice: t({
      en: 'Start Device',
    }),
    stopDevice: t({
      en: 'Stop Device',
    }),
    loadFromFlash: t({
      en: 'Load from Flash',
    }),
    loadDefaults: t({
      en: 'Load Defaults',
    }),
    resetDevice: t({
      en: 'Reset Device',
    }),
  },
} satisfies DeclarationContent

export default deviceControlContent
