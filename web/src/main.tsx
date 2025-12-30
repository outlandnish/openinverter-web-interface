import { render } from "preact";
import { IntlayerProvider } from "preact-intlayer";
import { App } from "./App";
import { ToastProvider } from "@components/Toast/ToastContainer";
import { WebSocketProvider } from "@contexts/WebSocketContext";
import { DeviceProvider } from "@contexts/DeviceContext";
import "preact-hint/dist/style.css";
import "./styles/theme.css";
import "./style.css";

// Register PWA Service Worker
import { registerSW } from "virtual:pwa-register";

const updateSW = registerSW({
  onNeedRefresh() {
    if (confirm("New content available. Reload?")) {
      updateSW(true);
    }
  },
  onOfflineReady() {
    console.log("App ready to work offline");
  },
});

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
  document.getElementById("app")!,
);
