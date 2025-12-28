import { defineConfig } from 'vite'
import preact from '@preact/preset-vite'
import { intlayerPlugin } from 'vite-intlayer'
import { VitePWA } from 'vite-plugin-pwa'
import path from 'path'

// Target for API proxy - real hardware
const apiTarget = 'http://inverter.local'
const wsTarget = 'ws://inverter.local'

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [
    preact(), 
    intlayerPlugin(),
    VitePWA({
      registerType: 'autoUpdate',
      includeAssets: ['openinverter-logo.png', 'icon-192.png', 'icon-512.png'],
      manifest: {
        name: 'OpenInverter Web Interface',
        short_name: 'OpenInverter',
        description: 'Web interface for OpenInverter ESP32 controller',
        theme_color: '#646cff',
        background_color: '#1a1a1a',
        display: 'standalone',
        orientation: 'portrait-primary',
        icons: [
          {
            src: '/openinverter-logo.png',
            sizes: 'any',
            type: 'image/png',
            purpose: 'any maskable'
          },
          {
            src: '/icon-192.png',
            sizes: '192x192',
            type: 'image/png',
            purpose: 'any maskable'
          },
          {
            src: '/icon-512.png',
            sizes: '512x512',
            type: 'image/png',
            purpose: 'any maskable'
          }
        ]
      },
      workbox: {
        globPatterns: ['**/*.{js,css,html,png}'],
        navigateFallback: null,
        runtimeCaching: [
          {
            urlPattern: /^https:\/\/fonts\.googleapis\.com\/.*/i,
            handler: 'CacheFirst',
            options: {
              cacheName: 'google-fonts-cache',
              expiration: {
                maxEntries: 10,
                maxAgeSeconds: 60 * 60 * 24 * 365 // 1 year
              },
              cacheableResponse: {
                statuses: [0, 200]
              }
            }
          },
          {
            urlPattern: /\/(cmd|canmap|nodeid)/,
            handler: 'NetworkFirst',
            options: {
              cacheName: 'api-cache',
              networkTimeoutSeconds: 10,
              expiration: {
                maxEntries: 50,
                maxAgeSeconds: 60 * 5 // 5 minutes
              },
              cacheableResponse: {
                statuses: [0, 200]
              }
            }
          }
        ]
      },
      devOptions: {
        enabled: true,
        type: 'module'
      }
    })
  ],

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
      // Proxy all API endpoints to the ESP32
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
      '/ota/upload': {
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
