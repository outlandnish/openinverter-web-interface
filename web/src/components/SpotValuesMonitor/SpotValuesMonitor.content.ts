import { t, insert, type DeclarationContent } from 'intlayer'

const spotValuesContent = {
  key: 'spot-values',
  content: {
    title: t({
      en: 'Spot Values Monitor',
    }),
    serialLabel: t({
      en: 'Serial:',
    }),
    startStreaming: t({
      en: 'Start Streaming',
    }),
    stopStreaming: t({
      en: 'Stop Streaming',
    }),
    startMonitoring: t({
      en: 'Start Monitoring',
    }),
    stopMonitoring: t({
      en: 'Stop Monitoring',
    }),
    updateInterval: t({
      en: 'Update Interval',
    }),
    ms: t({
      en: 'ms',
    }),
    selectAll: t({
      en: 'Select All',
    }),
    selectNone: t({
      en: 'Select None',
    }),
    selectAtLeastOne: t({
      en: 'Please select at least one parameter to stream',
    }),
    streamingStatus: insert(
      t({
        en: 'Streaming {{count}} parameters every {{interval}}ms',
      })
    ),
    tableView: t({
      en: 'Table View',
    }),
    chartView: t({
      en: 'Chart View',
    }),
    parameter: t({
      en: 'Parameter',
    }),
    value: t({
      en: 'Value',
    }),
    selectParametersToChart: t({
      en: 'Select Parameters to Chart',
    }),
    timeSeriesChart: t({
      en: 'Time Series Chart',
    }),
    noData: t({
      en: 'no data',
    }),
    chartPlaceholder: t({
      en: 'Select one or more parameters above to view their time series chart',
    }),
    loadingParameters: t({
      en: 'Loading parameters...',
    }),
    noParametersLoaded: t({
      en: 'Failed to load device parameters. Please try refreshing the page.',
    }),
    clearHistory: t({
      en: 'Clear History',
    }),
    back: t({
      en: 'Back',
    }),
    wrongDeviceAlert: insert(
      t({
        en: 'Wrong device connected! Please connect to {{serial}} first.\nCurrently connected to: {{connectedSerial}}',
        de: 'Falsches Gerät verbunden! Bitte verbinden Sie zuerst {{serial}}.\nAktuell verbunden mit: {{connectedSerial}}',
      })
    ),
    downloadingParameters: t({
      en: 'Downloading parameter definitions...',
      de: 'Parameterdefinitionen werden heruntergeladen...',
    }),
    loadingWrongDeviceWarningPrefix: t({
      en: 'Warning: Currently connected to device',
      de: 'Warnung: Aktuell verbunden mit Gerät',
    }),
    loadingWrongDeviceWarningSuffix: t({
      en: ', but loading parameters for',
      de: ', aber Parameter werden geladen für',
    }),
    loadingWrongDeviceWarningEnd: t({
      en: '. The parameter file may be incorrect. Please ensure the correct device is connected.',
      de: '. Die Parameterdatei könnte falsch sein. Bitte stellen Sie sicher, dass das richtige Gerät verbunden ist.',
    }),
    streamingWrongDeviceWarningPrefix: t({
      en: 'Warning:',
      de: 'Warnung:',
    }),
    streamingWrongDeviceWarningBold: t({
      en: 'Warning:',
      de: 'Warnung:',
    }),
    streamingWrongDeviceWarningText1: t({
      en: 'You are viewing parameters for device',
      de: 'Sie betrachten Parameter für Gerät',
    }),
    streamingWrongDeviceWarningText2: t({
      en: ', but device',
      de: ', aber Gerät',
    }),
    streamingWrongDeviceWarningText3: t({
      en: 'is currently connected. Values shown may be incorrect. Please connect to the correct device before streaming.',
      de: 'ist aktuell verbunden. Die angezeigten Werte könnten falsch sein. Bitte verbinden Sie das richtige Gerät, bevor Sie streamen.',
    }),
  },
} satisfies DeclarationContent

export default spotValuesContent
