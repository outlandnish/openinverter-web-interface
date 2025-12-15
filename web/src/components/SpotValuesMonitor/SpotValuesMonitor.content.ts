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
      en: '📊 Table View',
    }),
    chartView: t({
      en: '📈 Chart View',
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
  },
} satisfies DeclarationContent

export default spotValuesContent
