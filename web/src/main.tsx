import { render } from 'preact'
import { IntlayerProvider } from 'preact-intlayer'
import { App } from './App'
import { ToastProvider } from '@components/Toast/ToastContainer'
import { WebSocketProvider } from '@contexts/WebSocketContext'
import { DeviceProvider } from '@contexts/DeviceContext'
import './styles/theme.css'
import './style.css'

render(
  <IntlayerProvider>
    <ToastProvider>
      <WebSocketProvider url="/ws">
        <DeviceProvider>
          <App />
        </DeviceProvider>
      </WebSocketProvider>
    </ToastProvider>
  </IntlayerProvider>,
  document.getElementById('app')!
)
