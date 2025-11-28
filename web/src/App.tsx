import { Route, Switch } from 'wouter'
import { useIntlayer } from 'preact-intlayer'
import SystemOverview from '@pages/SystemOverview'
import DeviceDetails from '@pages/DeviceDetails'
import Settings from '@pages/Settings'

export function App() {
  const content = useIntlayer('app')
  
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
  )
}
