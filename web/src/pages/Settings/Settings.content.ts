import { t, type DeclarationContent } from 'intlayer'

const settingsContent = {
  key: 'settings',
  content: {
    title: t({
      en: 'Global Settings',
      de: 'Globale Einstellungen',
    }),
    subtitle: t({
      en: 'Configure CAN bus settings for all devices',
      de: 'CAN-Bus-Einstellungen für alle Geräte konfigurieren',
    }),
    loadingSettings: t({
      en: 'Loading settings...',
      de: 'Einstellungen werden geladen...',
    }),
    canBusConfiguration: t({
      en: 'CAN Bus Configuration',
      de: 'CAN-Bus-Konfiguration',
    }),
    canBusInfo: t({
      en: 'These settings apply to all devices on the CAN bus.',
      de: 'Diese Einstellungen gelten für alle Geräte auf dem CAN-Bus.',
    }),
    canSpeed: t({
      en: 'CAN Speed',
      de: 'CAN-Geschwindigkeit',
    }),
    canSpeedHint: t({
      en: 'Select the CAN bus speed. This must match the speed configured on your devices. Common speeds: 500k (default), 250k, 125k',
      de: 'Wählen Sie die CAN-Bus-Geschwindigkeit. Diese muss mit der auf Ihren Geräten konfigurierten Geschwindigkeit übereinstimmen. Übliche Geschwindigkeiten: 500k (Standard), 250k, 125k',
    }),
    canRxPin: t({
      en: 'CAN RX Pin',
      de: 'CAN RX Pin',
    }),
    canRxPinHint: t({
      en: 'GPIO pin for CAN receive (default: 4)',
      de: 'GPIO-Pin für CAN-Empfang (Standard: 4)',
    }),
    canTxPin: t({
      en: 'CAN TX Pin',
      de: 'CAN TX Pin',
    }),
    canTxPinHint: t({
      en: 'GPIO pin for CAN transmit (default: 5)',
      de: 'GPIO-Pin für CAN-Übertragung (Standard: 5)',
    }),
    canEnablePin: t({
      en: 'CAN Enable Pin (Optional)',
      de: 'CAN Enable Pin (Optional)',
    }),
    canEnablePinHint: t({
      en: 'GPIO pin to enable CAN transceiver (set to 0 if not used)',
      de: 'GPIO-Pin zum Aktivieren des CAN-Transceivers (auf 0 setzen, wenn nicht verwendet)',
    }),
    gpioPin: t({
      en: 'GPIO Pin',
      de: 'GPIO-Pin',
    }),
    saveSettings: t({
      en: 'Save Settings',
      de: 'Einstellungen speichern',
    }),
    saving: t({
      en: 'Saving...',
      de: 'Wird gespeichert...',
    }),
    savedSuccessfully: t({
      en: 'Settings saved successfully! Device will reinitialize CAN bus.',
      de: 'Einstellungen erfolgreich gespeichert! Gerät initialisiert CAN-Bus neu.',
    }),
    saveFailed: t({
      en: 'Failed to save settings',
      de: 'Fehler beim Speichern der Einstellungen',
    }),
    scanConfiguration: t({
      en: 'Scan Configuration',
      de: 'Scan-Konfiguration',
    }),
    scanConfigurationInfo: t({
      en: 'Configure the default node ID range for CAN bus scanning.',
      de: 'Konfigurieren Sie den Standard-Node-ID-Bereich für das Scannen des CAN-Bus.',
    }),
    scanStartNode: t({
      en: 'Scan Start Node',
      de: 'Scan-Startknoten',
    }),
    scanStartNodeHint: t({
      en: 'First node ID to scan (0-255, default: 1)',
      de: 'Erste zu scannende Knoten-ID (0-255, Standard: 1)',
    }),
    scanEndNode: t({
      en: 'Scan End Node',
      de: 'Scan-Endknoten',
    }),
    scanEndNodeHint: t({
      en: 'Last node ID to scan (0-255, default: 32)',
      de: 'Letzte zu scannende Knoten-ID (0-255, Standard: 32)',
    }),
  },
} satisfies DeclarationContent

export default settingsContent
