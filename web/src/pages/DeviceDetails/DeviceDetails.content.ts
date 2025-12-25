import { t, insert, type DeclarationContent } from 'intlayer'

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
    connectingToDevice: t({
      en: 'Connecting to device...',
      de: 'Verbindung zum Gerät wird hergestellt...',
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
    streamingStatus: insert(
      t({
        en: 'Streaming {{count}} parameters every {{interval}}ms',
        de: 'Streaming {{count}} Parameter alle {{interval}}ms',
      })
    ),
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
    parameterUpdatedSuccess: insert(
      t({
        en: '{{paramName}} updated',
        de: '{{paramName}} aktualisiert',
      })
    ),
    failedToUpdateParam: insert(
      t({
        en: 'Failed to update {{paramName}}',
        de: 'Fehler beim Aktualisieren {{paramName}}',
      })
    ),
    range: t({
      en: 'Range:',
      de: 'Bereich:',
    }),
    default: t({
      en: 'default:',
      de: 'Standard:',
    }),
    valueMustBeAtLeast: insert(
      t({
        en: 'Value must be at least {{min}}',
        de: 'Wert muss mindestens {{min}} sein',
      })
    ),
    valueMustBeAtMost: insert(
      t({
        en: 'Value must be at most {{max}}',
        de: 'Wert darf höchstens {{max}} sein',
      })
    ),
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
    otaUpdateFailedWithError: insert(
      t({
        en: 'OTA Update Failed: {{error}}',
        de: 'OTA-Update fehlgeschlagen: {{error}}',
      })
    ),
    reloadParameters: t({
      en: 'Reload Parameters',
      de: 'Parameter neu laden',
    }),
    reloadingParameters: t({
      en: 'Reloading...',
      de: 'Wird neu geladen...',
    }),
    parametersReloaded: t({
      en: 'Parameters reloaded successfully',
      de: 'Parameter erfolgreich neu geladen',
    }),
    reloadParametersFailed: t({
      en: 'Failed to reload parameters',
      de: 'Fehler beim Neuladen der Parameter',
    }),
    importExportParameters: t({
      en: 'Import/Export Parameters',
      de: 'Parameter importieren/exportieren',
    }),
    exportToJSON: t({
      en: 'Export to JSON',
      de: 'Als JSON exportieren',
    }),
    importFromJSON: t({
      en: 'Import from JSON',
      de: 'Aus JSON importieren',
    }),
    importing: insert(
      t({
        en: 'Importing... ({{current}}/{{total}})',
        de: 'Importiere... ({{current}}/{{total}})',
      })
    ),
    importExportHint: t({
      en: 'Export saves all parameter values to a JSON file. Import loads and validates parameters from a JSON file.',
      de: 'Export speichert alle Parameterwerte in einer JSON-Datei. Import lädt und validiert Parameter aus einer JSON-Datei.',
    }),
    noParametersToExport: t({
      en: 'No parameters to export',
      de: 'Keine Parameter zum Exportieren',
    }),
    parametersExported: t({
      en: 'Parameters exported successfully',
      de: 'Parameter erfolgreich exportiert',
    }),
    noParameterDefinitions: t({
      en: 'No parameter definitions loaded',
      de: 'Keine Parameterdefinitionen geladen',
    }),
    importedSuccessfully: insert(
      t({
        en: 'Imported {{count}} parameter{{plural}} successfully. Don\'t forget to save to flash!',
        de: '{{count}} Parameter erfolgreich importiert. Vergessen Sie nicht, im Flash zu speichern!',
      })
    ),
    validationFailed: insert(
      t({
        en: '{{count}} parameter{{plural}} failed validation:\n{{errors}}',
        de: '{{count}} Parameter haben die Validierung nicht bestanden:\n{{errors}}',
      })
    ),
    noValidParameters: t({
      en: 'No valid parameters found in file',
      de: 'Keine gültigen Parameter in der Datei gefunden',
    }),
    parseJSONFailed: insert(
      t({
        en: 'Failed to parse JSON file: {{error}}',
        de: 'Fehler beim Parsen der JSON-Datei: {{error}}',
      })
    ),
    failedToUpdate: insert(
      t({
        en: 'Failed to update {{key}}: {{error}}',
        de: 'Fehler beim Aktualisieren von {{key}}: {{error}}',
      })
    ),
    resetDevice: t({
      en: 'Reset Device',
      de: 'Gerät zurücksetzen',
    }),
    resetDeviceConfirm: t({
      en: 'Are you sure you want to reset the device? The device will restart and you may need to reconnect.',
      de: 'Sind Sie sicher, dass Sie das Gerät zurücksetzen möchten? Das Gerät wird neu gestartet und Sie müssen möglicherweise die Verbindung wiederherstellen.',
    }),
    deviceResetSuccess: t({
      en: 'Device reset command sent successfully',
      de: 'Befehl zum Zurücksetzen des Geräts erfolgreich gesendet',
    }),
    deviceResetFailed: t({
      en: 'Failed to reset device',
      de: 'Fehler beim Zurücksetzen des Geräts',
    }),
    noDataAvailable: t({
      en: 'No data available',
      de: 'Keine Daten verfügbar',
    }),
  },
} satisfies DeclarationContent

export default deviceDetailsContent
