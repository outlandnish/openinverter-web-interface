import { t, type DeclarationContent } from 'intlayer'

const deviceDetailsContent = {
  key: 'device-details',
  content: {
    deviceMonitor: t({
      en: 'Device Monitor',
      de: 'Geräteüberwachung',
    }),
    connected: t({
      en: 'Connected',
      de: 'Verbunden',
    }),
    disconnected: t({
      en: 'Disconnected',
      de: 'Getrennt',
    }),
    deviceDisconnected: t({
      en: 'Device disconnected',
      de: 'Gerät getrennt',
    }),
    serial: t({
      en: 'Serial:',
      de: 'Seriennummer:',
    }),
    nodeId: t({
      en: 'Node ID:',
      de: 'Knoten-ID:',
    }),
    firmware: t({
      en: 'Firmware:',
      de: 'Firmware:',
    }),
    unknown: t({
      en: 'Unknown',
      de: 'Unbekannt',
    }),
    liveMonitoring: t({
      en: 'Live Monitoring',
      de: 'Live-Überwachung',
    }),
    updateInterval: t({
      en: 'Update Interval (ms)',
      de: 'Aktualisierungsintervall (ms)',
    }),
    startMonitoring: t({
      en: 'Start Monitoring',
      de: 'Überwachung starten',
    }),
    stopMonitoring: t({
      en: 'Stop Monitoring',
      de: 'Überwachung stoppen',
    }),
    selectAll: t({
      en: 'Select All',
      de: 'Alle auswählen',
    }),
    selectNone: t({
      en: 'Select None',
      de: 'Keine auswählen',
    }),
    streaming: t({
      en: 'Streaming',
      de: 'Streaming',
    }),
    parametersEvery: t({
      en: 'parameters every',
      de: 'Parameter alle',
    }),
    tableView: t({
      en: '📊 Table View',
      de: '📊 Tabellenansicht',
    }),
    chartView: t({
      en: '📈 Chart View',
      de: '📈 Diagrammansicht',
    }),
    parameter: t({
      en: 'Parameter',
      de: 'Parameter',
    }),
    value: t({
      en: 'Value',
      de: 'Wert',
    }),
    selectParametersToChart: t({
      en: 'Select Parameters to Chart',
      de: 'Parameter zum Diagramm auswählen',
    }),
    noData: t({
      en: 'no data',
      de: 'keine Daten',
    }),
    chartPlaceholder: t({
      en: 'Select one or more parameters above to view their time series chart',
      de: 'Wählen Sie einen oder mehrere Parameter aus, um deren Zeitreihe anzuzeigen',
    }),
    deviceParameters: t({
      en: 'Device Parameters',
      de: 'Geräteparameter',
    }),
    saveNodeId: t({
      en: 'Save Node ID',
      de: 'Knoten-ID speichern',
    }),
    saveAllToFlash: t({
      en: 'Save All to Flash',
      de: 'Alle in Flash speichern',
    }),
    canSpeedNote: t({
      en: 'Note: CAN speed is configured globally in Settings',
      de: 'Hinweis: CAN-Geschwindigkeit wird global in den Einstellungen konfiguriert',
    }),
    nodeIdSaved: t({
      en: 'Node ID saved successfully!',
      de: 'Knoten-ID erfolgreich gespeichert!',
    }),
    parametersSaved: t({
      en: 'All parameters saved to device flash memory',
      de: 'Alle Parameter im Flash-Speicher des Geräts gespeichert',
    }),
    saveParametersFailed: t({
      en: 'Failed to save parameters',
      de: 'Fehler beim Speichern der Parameter',
    }),
    parameterUpdated: t({
      en: 'updated',
      de: 'aktualisiert',
    }),
    failedToUpdate: t({
      en: 'Failed to update',
      de: 'Fehler beim Aktualisieren',
    }),
    range: t({
      en: 'Range:',
      de: 'Bereich:',
    }),
    default: t({
      en: 'default:',
      de: 'Standard:',
    }),
    valueMustBeAtLeast: t({
      en: 'Value must be at least',
      de: 'Wert muss mindestens sein',
    }),
    valueMustBeAtMost: t({
      en: 'Value must be at most',
      de: 'Wert darf höchstens sein',
    }),
    firmwareUpdate: t({
      en: 'Firmware Update (OTA)',
      de: 'Firmware-Update (OTA)',
    }),
    otaInfo: t({
      en: 'Upload a new firmware (.bin file) to update this device over-the-air.',
      de: 'Laden Sie eine neue Firmware (.bin-Datei) hoch, um dieses Gerät drahtlos zu aktualisieren.',
    }),
    firmwareFile: t({
      en: 'Firmware File',
      de: 'Firmware-Datei',
    }),
    selected: t({
      en: 'Selected:',
      de: 'Ausgewählt:',
    }),
    startFirmwareUpdate: t({
      en: 'Start Firmware Update',
      de: 'Firmware-Update starten',
    }),
    monitoringWarning: t({
      en: 'Live monitoring must be stopped before starting a firmware update. Click "Stop Monitoring" above to proceed.',
      de: 'Die Live-Überwachung muss vor dem Start eines Firmware-Updates gestoppt werden. Klicken Sie oben auf „Überwachung stoppen", um fortzufahren.',
    }),
    uploadingFirmware: t({
      en: 'Uploading Firmware...',
      de: 'Firmware wird hochgeladen...',
    }),
    updatingDevice: t({
      en: 'Updating Device...',
      de: 'Gerät wird aktualisiert...',
    }),
    updateProgress: t({
      en: 'Please wait while the firmware update is in progress. Do not disconnect power or close this page.',
      de: 'Bitte warten Sie, während das Firmware-Update läuft. Trennen Sie die Stromversorgung nicht und schließen Sie diese Seite nicht.',
    }),
    transferringFirmware: t({
      en: 'Transferring firmware file to device...',
      de: 'Firmware-Datei wird auf Gerät übertragen...',
    }),
    installingFirmware: t({
      en: 'Device is installing the new firmware...',
      de: 'Gerät installiert die neue Firmware...',
    }),
    updateSuccessful: t({
      en: '✓ Firmware Update Successful!',
      de: '✓ Firmware-Update erfolgreich!',
    }),
    updateSuccessInfo: t({
      en: 'The device has been updated successfully and will restart automatically.',
      de: 'Das Gerät wurde erfolgreich aktualisiert und wird automatisch neu gestartet.',
    }),
    reconnectInfo: t({
      en: 'You may need to reconnect to the device after it reboots.',
      de: 'Sie müssen möglicherweise nach dem Neustart erneut eine Verbindung zum Gerät herstellen.',
    }),
    uploadAnother: t({
      en: 'Upload Another Firmware',
      de: 'Eine weitere Firmware hochladen',
    }),
    updateFailed: t({
      en: '✗ Firmware Update Failed',
      de: '✗ Firmware-Update fehlgeschlagen',
    }),
    tryAgain: t({
      en: 'Try Again',
      de: 'Erneut versuchen',
    }),
    otaUpdateFailed: t({
      en: 'OTA Update Failed:',
      de: 'OTA-Update fehlgeschlagen:',
    }),
  },
} satisfies DeclarationContent

export default deviceDetailsContent
