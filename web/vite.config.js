import { defineConfig } from 'vite'
import preact from '@preact/preset-vite'
import { intlayerPlugin } from 'vite-intlayer'
import path from 'path'

// Use simulator when VITE_USE_SIMULATOR=true or when running dev:sim
const useSimulator = process.env.VITE_USE_SIMULATOR === 'true'

// Target for API proxy - simulator or real hardware
const apiTarget = useSimulator ? 'http://localhost:4000' : 'http://inverter.local'
const wsTarget = useSimulator ? 'ws://localhost:4000' : 'ws://inverter.local'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [preact(), intlayerPlugin()],

  resolve: {
    alias: {
      '@': path.resolve(__dirname, './src'),
      '@components': path.resolve(__dirname, './src/components'),
      '@pages': path.resolve(__dirname, './src/pages'),
      '@hooks': path.resolve(__dirname, './src/hooks'),
      '@api': path.resolve(__dirname, './src/api'),
      '@utils': path.resolve(__dirname, './src/utils'),
      '@styles': path.resolve(__dirname, './src/styles'),
      '@contexts': path.resolve(__dirname, './src/contexts'),
    },
  },

  esbuild: {
    jsx: 'automatic',
    jsxImportSource: 'preact',
  },

  server: {
    port: 3000,
    proxy: {
      // Proxy all API endpoints to the ESP32 or simulator
      '/cmd': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/canmap': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/nodeid': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/settings': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/wifi': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/list': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/edit': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/fwupdate': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/reloadjson': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/resetdevice': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/version': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/baud': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/scan': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/devices': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/device': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/params': {
        target: apiTarget,
        changeOrigin: true,
      },
      '/ws': {
        target: wsTarget,
        ws: true,
        changeOrigin: true,
      },
    }
  },

  build: {
    // Build output goes to data/dist/ folder for SPIFFS
    outDir: '../data/dist',
    emptyOutDir: true,

    // Optimize for size (SPIFFS has limited space)
    minify: 'terser',
    terserOptions: {
      compress: {
        drop_console: false, // Keep console for debugging
        drop_debugger: true,
        passes: 2
      }
    },

    rollupOptions: {
      output: {
        // Use simple names for easier SPIFFS management
        entryFileNames: 'app.js',
        chunkFileNames: 'chunk-[name].js',
        assetFileNames: (assetInfo) => {
          // Keep CSS and other assets with simple names
          if (assetInfo.name.endsWith('.css')) {
            return 'app.css'
          }
          return 'assets/[name].[ext]'
        }
      }
    },

    // Target modern browsers that ESP32 clients will use
    target: 'es2015',
  }
})
