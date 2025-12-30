import { Route, Switch } from "wouter";
import { useIntlayer } from "preact-intlayer";
import { useEffect } from "preact/hooks";
import SystemOverview from "@pages/SystemOverview";
import DeviceDetails from "@pages/DeviceDetails";
import Settings from "@pages/Settings";
import { useWebSocketContext } from "@contexts/WebSocketContext";
import { api } from "@api/inverter";

export function App() {
  const content = useIntlayer("app");
  const { sendMessage } = useWebSocketContext();

  // Initialize API WebSocket sender when WebSocket is ready
  useEffect(() => {
    api.setWebSocketSender(sendMessage);
  }, [sendMessage]);

  return (
    <Switch>
      <Route path="/" component={SystemOverview} />
      <Route path="/settings" component={Settings} />
      <Route path="/devices/:serial" component={DeviceDetails} />
      <Route>
        <div class="error-page">
          <h1>{content.notFound.title}</h1>
        </div>
      </Route>
    </Switch>
  );
}
