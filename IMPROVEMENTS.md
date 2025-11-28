# Web Interface Improvements

This document summarizes the recent improvements made to the ESP32 web interface.

## 1. Theme System Standardization ✅

Created a centralized theme system for consistent styling across the application.

### New Structure
```
web/src/styles/
├── theme/
│   ├── colors.css       # Color variables and brand colors
│   ├── spacing.css      # Spacing scale, borders, shadows
│   └── typography.css   # Font families, sizes, weights
├── components/
│   ├── buttons.css      # Button component styles (.btn-primary, .btn-icon, etc.)
│   └── icons.css        # Icon sizing and animations
├── theme.css            # Main theme entry (imports all modules)
└── README.md           # Complete documentation
```

### Features
- **CSS Variables**: Consistent color, spacing, and typography variables
- **Component Classes**: Reusable `.btn-primary`, `.btn-icon`, `.btn-with-icon`, etc.
- **Icon Utilities**: `.icon-sm`, `.icon-lg`, `.icon-spin`, `.icon-pulse`
- **Spacing Scale**: `--spacing-xs` through `--spacing-3xl`
- **Responsive Design**: Mobile-friendly with proper media queries

### Files Modified
- `web/src/main.tsx` - Import theme.css before style.css
- See `web/src/styles/README.md` for usage guide

## 2. Fixed Scan Icon Display ✅

**Issue**: The scan icon in the DeviceScanner component at the top of the overview page wasn't showing.

**Root Cause**: The SVG used `fill="currentColor"` with a filled path, which wasn't rendering properly.

**Solution**: Changed to use `stroke="currentColor"` with `fill="none"`, matching the working scan button icon.

### Files Modified
- `web/src/components/DeviceScanner.tsx:27-28`

## 3. Mobile Swipe Gestures ✅

Added swipe gesture support for opening/closing the sidebar on mobile devices.

### Features
- **Swipe Right**: Opens sidebar (only on mobile when closed)
- **Swipe Left**: Closes sidebar (when open)
- **Smart Detection**: Ignores excessive vertical movement
- **Configurable**: Minimum swipe distance and max vertical tolerance

### Files Added
- `web/src/hooks/useSwipeGesture.ts` - Custom hook for touch gesture detection

### Files Modified
- `web/src/components/Layout.tsx` - Integrated swipe gesture hook

## 4. Improved Hamburger Menu Styling ✅

**Issue**: Hamburger menu had an ugly white background and border.

**Solution**: Removed background and border, keeping only the blue bars with subtle shadow for depth.

### Changes
- Removed white background and border
- Added hover effect that darkens the bars
- Added active press effect (scale down)
- Cleaner, more modern appearance

### Files Modified
- `web/src/style.css:1053-1088` - Hamburger menu styles

## 5. WebSocket Connection Error Handling ✅

Added comprehensive error handling for ESP32 disconnection states.

### Features

#### Connection Error Banner
- **Prominent Warning**: Yellow banner shows when WebSocket disconnected
- **Clear Message**: "ESP32 Disconnected" with helpful instructions
- **Reconnect Button**: Manual reconnect with refresh icon
- **Responsive**: Adapts layout for mobile devices

#### Disabled Scan Buttons
All scan buttons are now disabled when ESP32 is disconnected:
- ✅ DeviceScanner button (top of overview page)
- ✅ Sidebar quick scan button
- ✅ "Scan Devices" button (empty state)

#### Smart Behavior
- **Auto-stop scanning**: Stops active scan when connection drops
- **Connection check**: Prevents scan attempts when disconnected
- **Helpful tooltips**: "Cannot scan: ESP32 disconnected"
- **Visual feedback**: Disabled state is clearly visible

### Files Modified
- `web/src/pages/SystemOverview.tsx`
  - Added connection error banner
  - Added `reconnect` from useWebSocket hook
  - Added `disabled` prop to DeviceScanner
  - Stop scanning on WebSocket close
  - Guard scan handler with connection check

- `web/src/components/DeviceScanner.tsx`
  - Added `disabled` prop
  - Disable buttons when disconnected
  - Updated tooltips for disabled state

- `web/src/components/Sidebar.tsx`
  - Added `scanDisabled` prop
  - Disable scan button when disconnected
  - Updated tooltip for disabled state

- `web/src/components/Layout.tsx`
  - Pass `scanDisabled={!isConnected}` to Sidebar

- `web/src/style.css`
  - Added `.connection-error-banner` styles
  - Added `.connection-error-icon` styles
  - Added `.connection-error-content` styles
  - Mobile-responsive error banner

## Usage Examples

### Using Theme Variables
```css
.my-component {
  background: var(--oi-blue);
  padding: var(--spacing-lg);
  border-radius: var(--radius-md);
  font-size: var(--text-base);
}
```

### Using Component Classes
```tsx
<button class="btn-primary">Save</button>
<button class="btn-icon"><svg>...</svg></button>
<svg class="icon-lg icon-spin">...</svg>
```

### Testing Swipe Gestures
On mobile/tablet:
1. Swipe right from left edge → Opens sidebar
2. Swipe left anywhere → Closes sidebar

### Testing WebSocket Disconnection
1. Stop the ESP32 or disconnect from network
2. Observe yellow connection error banner
3. Try clicking scan buttons (should be disabled)
4. Click "Reconnect" button to attempt reconnection

## Browser Compatibility

All features tested and working on:
- ✅ Modern browsers (Chrome, Firefox, Safari, Edge)
- ✅ Mobile browsers (iOS Safari, Chrome Mobile)
- ✅ Touch devices (tablets, phones)
- ✅ Desktop devices (mouse/trackpad)

## Performance

- **Swipe gestures**: Use passive event listeners for smooth scrolling
- **WebSocket**: Auto-reconnect with 3-second interval
- **Theme CSS**: Loaded once at app initialization
- **No runtime overhead**: All theme variables are native CSS

## Future Improvements

Potential enhancements to consider:
- [ ] Dark mode support using theme variables
- [ ] Customizable swipe sensitivity in settings
- [ ] Connection status indicator in navbar
- [ ] Offline mode with cached device list
- [ ] Progressive Web App (PWA) support
