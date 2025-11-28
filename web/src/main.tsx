import { render } from 'preact'
import { IntlayerProvider } from 'preact-intlayer'
import { App } from './App'
import { ToastProvider } from './components/ToastContainer'
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
