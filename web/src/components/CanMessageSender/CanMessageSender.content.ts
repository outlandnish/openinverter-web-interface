import { t, insert, type DeclarationContent } from 'intlayer'

const canMessageSenderContent = {
  key: 'can-message-sender',
  content: {
    title: t({
      en: 'CAN Message Sender',
      de: 'CAN Nachrichtensender',
    }),
    oneShotSection: t({
      en: 'One-Shot Message',
      de: 'Einmalige Nachricht',
    }),
    oneShotDescription: t({
      en: 'Send a single CAN message immediately',
      de: 'Eine einzelne CAN-Nachricht sofort senden',
    }),
    canIdLabel: t({
      en: 'CAN ID (hex):',
      de: 'CAN ID (hex):',
    }),
    canIdPlaceholder: t({
      en: '0x180',
      de: '0x180',
    }),
    dataBytesLabel: t({
      en: 'Data Bytes (hex):',
      de: 'Datenbytes (hex):',
    }),
    dataBytesPlaceholder: t({
      en: '00 00 00 00 00 00 00 00',
      de: '00 00 00 00 00 00 00 00',
    }),
    sendButton: t({
      en: 'Send Message',
      de: 'Nachricht senden',
    }),
    periodicSection: t({
      en: 'Periodic Messages',
      de: 'Periodische Nachrichten',
    }),
    periodicDescription: t({
      en: 'Configure messages to be sent at regular intervals',
      de: 'Nachrichten konfigurieren, die in regelmäßigen Abständen gesendet werden',
    }),
    noPeriodicMessages: t({
      en: 'No periodic messages configured',
      de: 'Keine periodischen Nachrichten konfiguriert',
    }),
    tableHeaderCanId: t({
      en: 'CAN ID',
      de: 'CAN ID',
    }),
    tableHeaderData: t({
      en: 'Data',
      de: 'Daten',
    }),
    tableHeaderInterval: t({
      en: 'Interval (ms)',
      de: 'Intervall (ms)',
    }),
    tableHeaderStatus: t({
      en: 'Status',
      de: 'Status',
    }),
    tableHeaderActions: t({
      en: 'Actions',
      de: 'Aktionen',
    }),
    statusActive: t({
      en: 'Active',
      de: 'Aktiv',
    }),
    statusStopped: t({
      en: 'Stopped',
      de: 'Gestoppt',
    }),
    startButton: t({
      en: 'Start',
      de: 'Starten',
    }),
    stopButton: t({
      en: 'Stop',
      de: 'Stoppen',
    }),
    removeButton: t({
      en: 'Remove',
      de: 'Entfernen',
    }),
    addPeriodicButton: t({
      en: 'Add Periodic Message',
      de: 'Periodische Nachricht hinzufügen',
    }),
    addPeriodicFormTitle: t({
      en: 'Add New Periodic Message',
      de: 'Neue periodische Nachricht hinzufügen',
    }),
    intervalLabel: t({
      en: 'Interval (ms):',
      de: 'Intervall (ms):',
    }),
    cancelButton: t({
      en: 'Cancel',
      de: 'Abbrechen',
    }),
    addMessageButton: t({
      en: 'Add Message',
      de: 'Nachricht hinzufügen',
    }),
    notConnectedError: t({
      en: 'Not connected to device',
      de: 'Nicht mit Gerät verbunden',
    }),
    invalidCanIdError: t({
      en: 'Invalid CAN ID (must be 0x000 to 0x7FF)',
      de: 'Ungültige CAN ID (muss zwischen 0x000 und 0x7FF liegen)',
    }),
    invalidDataBytesError: t({
      en: 'Invalid data bytes',
      de: 'Ungültige Datenbytes',
    }),
    invalidIntervalError: t({
      en: 'Interval must be between 10ms and 10000ms',
      de: 'Intervall muss zwischen 10ms und 10000ms liegen',
    }),
    messageSentSuccess: insert(
      t({
        en: 'CAN message sent: ID {{canId}}',
        de: 'CAN-Nachricht gesendet: ID {{canId}}',
      })
    ),
    messageSentError: insert(
      t({
        en: 'Failed to send CAN message: {{error}}',
        de: 'Fehler beim Senden der CAN-Nachricht: {{error}}',
      })
    ),
    periodicAddedSuccess: t({
      en: 'Periodic message added',
      de: 'Periodische Nachricht hinzugefügt',
    }),
    periodicRemovedSuccess: t({
      en: 'Periodic message removed',
      de: 'Periodische Nachricht entfernt',
    }),
  },
} satisfies DeclarationContent

export default canMessageSenderContent
