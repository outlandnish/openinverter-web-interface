import { render } from 'preact'
import { IntlayerProvider } from 'preact-intlayer'
import { App } from './App'
import { ToastProvider } from '@components/Toast/ToastContainer'
import './styles/theme.css'
import './style.css'

render(
  <IntlayerProvider>
    <ToastProvider>
      <App />
    </ToastProvider>
  </IntlayerProvider>,
  document.getElementById('app')!
)
