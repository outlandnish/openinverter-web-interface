# Open Inverter Web Interface Development

This folder contains the source code for the Preact + TypeScript web interface.

## Development Setup

1. **Install dependencies:**
   ```bash
   npm install
   ```

2. **Start development server:**
   ```bash
   npm run dev
   ```

   This will start Vite dev server at http://localhost:3000 with hot reload.
   All API calls are proxied to http://inverter.local

3. **Build for production:**
   ```bash
   npm run build
   ```

   This builds the app into the `../data/` folder ready for SPIFFS upload.

## Project Structure

```
web/
├── index.html          # Entry HTML file
├── vite.config.js      # Vite build configuration
├── package.json        # NPM dependencies
├── node_modules/       # Installed dependencies (gitignored)
├── src/
│   ├── main.jsx        # App entry point
│   ├── App.jsx         # Main app component
│   ├── style.css       # Global styles
│   ├── api/
│   │   └── inverter.js # API client for ESP32
│   ├── components/     # Reusable Preact components
│   ├── hooks/          # Custom Preact hooks
│   └── utils/          # Utility functions
└── public/             # Static assets (copied as-is)
```

## Development Workflow

1. Make changes to files in `web/src/`
2. See changes instantly in browser at http://localhost:3000
3. API calls automatically proxy to your ESP32 at http://inverter.local
4. When ready, run `npm run build` to generate production files
5. Upload `data/` folder to SPIFFS

## API Integration

The `src/api/inverter.ts` module provides fully typed methods for all ESP32 endpoints:

```typescript
import { api } from './api/inverter'
import type { NodeIdResponse, ParameterList, CanMapping } from './api/inverter'

// Get node ID (fully typed)
const data: NodeIdResponse = await api.getNodeId()
console.log(data.id, data.speed)

// Set parameter
await api.setParam('paramName', value)

// Get parameter list (typed as ParameterList)
const params: ParameterList = await api.getParamList()

// Send custom command
const response: string = await api.sendCommand('json')
```

### Type Safety

All API methods are fully typed with TypeScript:
- Return types are explicitly defined
- Request parameters are validated at compile time
- Autocomplete works in your IDE
- Catch errors before runtime

## TypeScript Features

Run type checking at any time:
```bash
npm run typecheck
```

Benefits of using TypeScript:
- **Type Safety**: Catch errors at compile time instead of runtime
- **Better IDE Support**: Full autocomplete and IntelliSense
- **Self-Documenting**: Types serve as inline documentation
- **Refactoring**: Rename symbols safely across the codebase
- **Team Collaboration**: Clearer contracts between components

## Notes

- The build output is optimized for size (SPIFFS has limited space)
- Console logs are kept in production for debugging
- Modern ES2015+ syntax is used (supported by ESP32 browsers)
- TypeScript provides full type safety during development
- Build process automatically checks types before bundling
