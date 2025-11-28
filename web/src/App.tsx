import { Route, Switch } from 'wouter'
import SystemOverview from './pages/SystemOverview'
import DeviceDetails from './pages/DeviceDetails'
import Settings from './pages/Settings'

export function App() {
  return (
    <Switch>
      <Route path="/" component={SystemOverview} />
      <Route path="/settings" component={Settings} />
      <Route path="/devices/:serial" component={DeviceDetails} />
      <Route>
        <div class="error-page">
          <h1>404 - Page Not Found</h1>
        </div>
      </Route>
    </Switch>
  )
}
