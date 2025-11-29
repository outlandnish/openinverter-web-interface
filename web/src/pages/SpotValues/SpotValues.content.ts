import { t, insert, type DeclarationContent } from 'intlayer'

const spotValuesContent = {
  key: 'spot-values',
  content: {
    title: t({
      en: 'Spot Values',
      de: 'Aktuelle Werte',
    }),
    back: t({
      en: 'Back',
      de: 'Zurück',
    }),
    serialLabel: t({
      en: 'Serial:',
      de: 'Seriennummer:',
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
    streamingStatus: insert({
      en: 'Streaming {{count}} parameters every {{interval}}ms',
      de: 'Streaming {{count}} Parameter alle {{interval}}ms',
    }),
    tableView: t({
      en: '📊 Table View',
      de: '📊 Tabellenansicht',
    }),
    chartView: t({
      en: '📈 Chart View',
      de: '📈 Diagrammansicht',
    }),
    monitor: t({
      en: 'Monitor',
      de: 'Überwachen',
    }),
    parameter: t({
      en: 'Parameter',
      de: 'Parameter',
    }),
    value: t({
      en: 'Value',
      de: 'Wert',
    }),
    unit: t({
      en: 'Unit',
      de: 'Einheit',
    }),
    selectParametersToChart: t({
      en: 'Select Parameters to Chart',
      de: 'Parameter zum Diagramm auswählen',
    }),
    noData: t({
      en: 'no data',
      de: 'keine Daten',
    }),
    timeSeriesChart: t({
      en: 'Time Series Chart',
      de: 'Zeitreihenverlauf',
    }),
    chartPlaceholder: t({
      en: 'Select one or more parameters above to view their time series chart',
      de: 'Wählen Sie einen oder mehrere Parameter aus, um deren Zeitreihe anzuzeigen',
    }),
    loadingParameters: t({
      en: 'Loading parameters...',
      de: 'Parameter werden geladen...',
    }),
    noParametersLoaded: t({
      en: 'No parameters loaded',
      de: 'Keine Parameter geladen',
    }),
    selectAtLeastOne: t({
      en: 'Please select at least one parameter to monitor',
      de: 'Bitte wählen Sie mindestens einen Parameter zum Überwachen',
    }),
  },
} satisfies DeclarationContent

export default spotValuesContent
