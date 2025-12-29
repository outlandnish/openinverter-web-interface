import { useLocale } from 'preact-intlayer'
import { Locales } from 'intlayer'
import { useIntlayer } from 'preact-intlayer'

const LOCALE_NAMES: Record<string, string> = {
  [Locales.ENGLISH]: 'English',
  [Locales.GERMAN]: 'Deutsch',
}

export default function LanguageSelector() {
  const { locale, setLocale } = useLocale()
  const content = useIntlayer('language-selector')

  const availableLocales = [Locales.ENGLISH, Locales.GERMAN]

  const handleChange = (e: Event) => {
    const target = e.target as HTMLSelectElement
    setLocale(target.value as typeof Locales.ENGLISH)
  }

  return (
    <div class="language-selector">
      <label class="language-selector-label" for="language-select">
        {content.language}
      </label>
      <select
        id="language-select"
        class="language-select"
        value={locale}
        onChange={handleChange}
      >
        {availableLocales.map((loc) => (
          <option key={loc} value={loc}>
            {LOCALE_NAMES[loc]}
          </option>
        ))}
      </select>
    </div>
  )
}
