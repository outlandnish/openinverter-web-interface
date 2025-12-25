import { t, insert, type DeclarationContent } from 'intlayer'

const canIoControlContent = {
  key: 'can-io-control',
  content: {
    title: t({
      en: 'CAN IO Control (Inverter)',
      de: 'CAN IO Steuerung (Wechselrichter)',
    }),
    configurationSection: t({
      en: 'Configuration',
      de: 'Konfiguration',
    }),
    canIdLabel: t({
      en: 'CAN ID (hex):',
      de: 'CAN ID (hex):',
    }),
    canIdPlaceholder: t({
      en: '3F',
      de: '3F',
    }),
    intervalLabel: t({
      en: 'Interval (ms):',
      de: 'Intervall (ms):',
    }),
    intervalHint: t({
      en: '10-500ms (recommended: 50-100ms)',
      de: '10-500ms (empfohlen: 50-100ms)',
    }),
    useCrcLabel: t({
      en: 'Use CRC-32 (controlcheck=1)',
      de: 'CRC-32 verwenden (controlcheck=1)',
    }),
    useCrcHint: t({
      en: 'Disable for counter-only mode (controlcheck=0)',
      de: 'Deaktivieren für Nur-Zähler-Modus (controlcheck=0)',
    }),
    controlFlagsSection: t({
      en: 'Control Flags',
      de: 'Steuerungsflags',
    }),
    cruiseFlag: t({
      en: 'Cruise (0x01)',
      de: 'Tempomat (0x01)',
    }),
    startFlag: t({
      en: 'Start (0x02)',
      de: 'Start (0x02)',
    }),
    brakeFlag: t({
      en: 'Brake (0x04)',
      de: 'Bremse (0x04)',
    }),
    forwardFlag: t({
      en: 'Forward (0x08)',
      de: 'Vorwärts (0x08)',
    }),
    reverseFlag: t({
      en: 'Reverse (0x10)',
      de: 'Rückwärts (0x10)',
    }),
    bmsFlag: t({
      en: 'BMS (0x20)',
      de: 'BMS (0x20)',
    }),
    throttleSection: t({
      en: 'Throttle & Parameters',
      de: 'Gas & Parameter',
    }),
    throttleLabel: t({
      en: 'Throttle (%):',
      de: 'Gas (%):',
    }),
    cruiseSpeedLabel: t({
      en: 'Cruise Speed:',
      de: 'Tempomat-Geschwindigkeit:',
    }),
    cruiseSpeedHint: t({
      en: '0-16383',
      de: '0-16383',
    }),
    regenPresetLabel: t({
      en: 'Regen Preset:',
      de: 'Regen-Voreinstellung:',
    }),
    regenPresetHint: t({
      en: '0-255',
      de: '0-255',
    }),
    startButton: t({
      en: 'Start CAN IO',
      de: 'CAN IO starten',
    }),
    stopButton: t({
      en: 'Stop CAN IO',
      de: 'CAN IO stoppen',
    }),
    activeStatus: insert(
      t({
        en: 'Active (sending every {{interval}}ms)',
        de: 'Aktiv (sendet alle {{interval}}ms)',
      })
    ),
    notConnectedError: t({
      en: 'Not connected to device',
      de: 'Nicht mit Gerät verbunden',
    }),
    invalidCanIdError: t({
      en: 'Invalid CAN ID (must be 0x000 to 0x7FF)',
      de: 'Ungültige CAN ID (muss zwischen 0x000 und 0x7FF liegen)',
    }),
    intervalStartedSuccess: insert(
      t({
        en: 'CAN IO interval started ({{intervalMs}}ms)',
        de: 'CAN IO Intervall gestartet ({{intervalMs}}ms)',
      })
    ),
    intervalStoppedSuccess: t({
      en: 'CAN IO interval stopped',
      de: 'CAN IO Intervall gestoppt',
    }),
  },
} satisfies DeclarationContent

export default canIoControlContent
