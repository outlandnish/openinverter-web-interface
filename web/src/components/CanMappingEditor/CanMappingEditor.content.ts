import { t, insert, type DeclarationContent } from 'intlayer'

const canMappingEditorContent = {
  key: 'can-mapping-editor',
  content: {
    title: t({
      en: 'CAN Mappings',
      de: 'CAN-Zuordnungen',
    }),
    loading: t({
      en: 'Loading CAN mappings...',
      de: 'CAN-Zuordnungen werden geladen...',
    }),
    errorPrefix: t({
      en: 'Error:',
      de: 'Fehler:',
    }),
    txSection: t({
      en: 'TX Mappings (Transmit)',
      de: 'TX-Zuordnungen (Senden)',
    }),
    rxSection: t({
      en: 'RX Mappings (Receive)',
      de: 'RX-Zuordnungen (Empfangen)',
    }),
    noTxMappings: t({
      en: 'No TX mappings configured',
      de: 'Keine TX-Zuordnungen konfiguriert',
    }),
    noRxMappings: t({
      en: 'No RX mappings configured',
      de: 'Keine RX-Zuordnungen konfiguriert',
    }),
    tableHeaderCanId: t({
      en: 'CAN ID',
      de: 'CAN ID',
    }),
    tableHeaderParameter: t({
      en: 'Parameter',
      de: 'Parameter',
    }),
    tableHeaderPosition: t({
      en: 'Position',
      de: 'Position',
    }),
    tableHeaderLength: t({
      en: 'Length',
      de: 'Länge',
    }),
    tableHeaderGain: t({
      en: 'Gain',
      de: 'Verstärkung',
    }),
    tableHeaderOffset: t({
      en: 'Offset',
      de: 'Offset',
    }),
    tableHeaderActions: t({
      en: 'Actions',
      de: 'Aktionen',
    }),
    bitsUnit: t({
      en: 'bits',
      de: 'Bits',
    }),
    removeButton: t({
      en: 'Remove',
      de: 'Entfernen',
    }),
    addMappingButton: t({
      en: 'Add CAN Mapping',
      de: 'CAN-Zuordnung hinzufügen',
    }),
    addMappingFormTitle: t({
      en: 'Add New CAN Mapping',
      de: 'Neue CAN-Zuordnung hinzufügen',
    }),
    directionLabel: t({
      en: 'Direction:',
      de: 'Richtung:',
    }),
    directionTx: t({
      en: 'TX (Transmit)',
      de: 'TX (Senden)',
    }),
    directionRx: t({
      en: 'RX (Receive)',
      de: 'RX (Empfangen)',
    }),
    canIdLabel: t({
      en: 'CAN ID (hex):',
      de: 'CAN ID (hex):',
    }),
    canIdPlaceholder: t({
      en: '0x180',
      de: '0x180',
    }),
    parameterLabel: t({
      en: 'Parameter:',
      de: 'Parameter:',
    }),
    selectParameter: t({
      en: 'Select parameter...',
      de: 'Parameter auswählen...',
    }),
    bitPositionLabel: t({
      en: 'Bit Position:',
      de: 'Bit-Position:',
    }),
    bitLengthLabel: t({
      en: 'Bit Length:',
      de: 'Bit-Länge:',
    }),
    gainLabel: t({
      en: 'Gain:',
      de: 'Verstärkung:',
    }),
    offsetLabel: t({
      en: 'Offset:',
      de: 'Offset:',
    }),
    cancelButton: t({
      en: 'Cancel',
      de: 'Abbrechen',
    }),
    addButton: t({
      en: 'Add Mapping',
      de: 'Zuordnung hinzufügen',
    }),
    paramFallback: insert(
      t({
        en: 'Param {{paramId}}',
        de: 'Parameter {{paramId}}',
      })
    ),
    removeConfirmation: insert(
      t({
        en: 'Remove {{direction}} mapping for {{paramName}} (CAN ID {{canId}})?',
        de: '{{direction}}-Zuordnung für {{paramName}} (CAN ID {{canId}}) entfernen?',
      })
    ),
    addSuccess: t({
      en: 'CAN mapping added successfully',
      de: 'CAN-Zuordnung erfolgreich hinzugefügt',
    }),
    removeSuccess: t({
      en: 'CAN mapping removed successfully',
      de: 'CAN-Zuordnung erfolgreich entfernt',
    }),
    addError: t({
      en: 'Failed to add CAN mapping',
      de: 'Fehler beim Hinzufügen der CAN-Zuordnung',
    }),
    removeError: t({
      en: 'Failed to remove CAN mapping',
      de: 'Fehler beim Entfernen der CAN-Zuordnung',
    }),
  },
} satisfies DeclarationContent

export default canMappingEditorContent
