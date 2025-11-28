# Web Interface Development Guide

This project now has a modern Preact-based web interface with hot-reload development.

## Quick Start

```bash
# 1. Navigate to web folder
cd web

# 2. Install dependencies
npm install

# 3. Start development server (with hot reload)
npm run dev

# 4. Open browser to http://localhost:3000
# All API calls proxy to http://inverter.local
```

## Project Structure

```
esp32-web-interface/
├── web/                    # Source code (edit here during development)
│   ├── src/
│   │   ├── App.jsx        # Main Preact component
│   │   ├── main.jsx       # App entry point
│   │   ├── style.css      # Global styles
│   │   └── api/
│   │       └── inverter.js # ESP32 API client
│   └── index.html         # HTML entry point
│
├── data/                   # Build output (upload to SPIFFS)
│   └── (generated files)
│
├── src/                    # ESP32 firmware code
│   └── main.cpp
│
├── vite.config.js         # Build configuration
└── package.json           # Node.js dependencies
```

## Development Workflow

### 1. Development (Hot Reload)

```bash
npm run dev
```

- Edit files in `web/src/`
- Changes appear instantly in browser
- API calls proxy to ESP32 at `http://inverter.local`
- No SPIFFS flashing needed during development!

### 2. Build for Production

```bash
npm run build
```

- Generates optimized files in `data/` folder
- Minified and ready for SPIFFS
- Upload to ESP32:

```bash
pio run --target uploadfs
```

## Technologies Used

- **Preact**: Lightweight React alternative (~3KB)
- **Vite**: Fast development server with HMR
- **Modern JavaScript**: ES2015+ with JSX

## API Client

The `web/src/api/inverter.js` module provides all ESP32 endpoints:

```javascript
import { api } from './api/inverter'

// Examples:
await api.getNodeId()
await api.setParam('paramName', value)
await api.sendCommand('json')
await api.saveParams()
await api.getCanMapping()
```

## Next Steps

1. **Start developing**: Edit `web/src/App.jsx` to add features
2. **Create components**: Add files to `web/src/components/`
3. **Add hooks**: Custom hooks go in `web/src/hooks/`
4. **Style it**: Edit `web/src/style.css` or add component-specific styles

## Tips

- Keep components small and focused
- Use Preact hooks (`useState`, `useEffect`, etc.)
- The API client handles all ESP32 communication
- Build output is optimized for SPIFFS size constraints
- Console logs are preserved for debugging on device

## Migrating Existing Features

To migrate features from the old `data/` files:

1. Create a new component in `web/src/components/`
2. Use the API client from `web/src/api/inverter.js`
3. Import and use in `web/src/App.jsx`
4. Style with CSS or inline styles

See `web/src/App.jsx` for an example of how to fetch and display data.
