import { t, type DeclarationContent } from 'intlayer'

const languageSelectorContent = {
  key: 'language-selector',
  content: {
    language: t({
      en: 'Language',
      de: 'Sprache',
    }),
  },
} satisfies DeclarationContent

export default languageSelectorContent
