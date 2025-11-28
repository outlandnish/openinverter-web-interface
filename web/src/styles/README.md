# Theme and Styling Guide

This directory contains the centralized theme and styling system for the OpenInverter web interface.

## Structure

```
styles/
├── theme/
│   ├── colors.css       # Color variables and brand colors
│   ├── spacing.css      # Spacing scale, borders, shadows
│   └── typography.css   # Font families, sizes, weights
├── components/
│   ├── buttons.css      # Button component styles
│   └── icons.css        # Icon sizing and animations
└── theme.css            # Main theme entry point (imports all theme modules)
```

## Usage

### Using Theme Variables

All theme variables are available globally via CSS custom properties:

```css
/* Colors */
.my-element {
  background: var(--oi-blue);
  color: var(--text-primary);
  border: 1px solid var(--border-color);
}

/* Spacing */
.my-element {
  padding: var(--spacing-lg);
  margin-bottom: var(--spacing-xl);
  border-radius: var(--radius-md);
}

/* Typography */
.my-element {
  font-family: var(--font-sans);
  font-size: var(--text-lg);
  font-weight: var(--font-semibold);
}
```

### Using Component Classes

The theme provides reusable component classes:

#### Buttons

```tsx
// Primary button (beige, default style)
<button class="btn-primary">Save</button>

// Button with icon and text (same styling as primary)
<button class="btn-with-icon">
  <svg>...</svg>
  <span>Scan</span>
</button>

// Accent button (blue, for emphasis)
<button class="btn-accent">Submit</button>

// Icon-only button
<button class="btn-icon">
  <svg>...</svg>
</button>

// Active/scanning state
<button class="btn-with-icon scanning">
  <svg>...</svg>
  <span>Scanning...</span>
</button>

// Ghost/minimal button
<button class="btn-ghost">Close</button>

// Danger button
<button class="btn-danger">Delete</button>
```

**Note**: `.btn-primary` and `.btn-with-icon` use the same beige/neutral styling (matches scan buttons). Use `.btn-accent` for prominent blue buttons when needed.

#### Icons

```tsx
// Sized icons
<svg class="icon-sm">...</svg>
<svg class="icon-md">...</svg>
<svg class="icon-lg">...</svg>

// Colored icons
<svg class="icon-primary">...</svg>
<svg class="icon-accent">...</svg>
<svg class="icon-muted">...</svg>

// Animated icons
<svg class="icon-spin">...</svg>
<svg class="icon-pulse">...</svg>
```

## Color Palette

### Brand Colors
- `--oi-blue`: #1e88e5 (Primary brand color)
- `--oi-blue-dark`: #1565c0
- `--oi-blue-light`: #e3f2fd
- `--oi-orange`: #ff8c00 (Accent color)
- `--oi-orange-light`: #ffa726
- `--oi-beige`: #fef8f0 (Warm neutral)
- `--oi-yellow`: #ffd54f

### Neutral Colors
- `--bg-primary`: #f5f5f5 (Main background)
- `--bg-secondary`: white (Card backgrounds)
- `--text-primary`: #333 (Main text)
- `--text-secondary`: #666 (Secondary text)
- `--text-muted`: #999 (Muted text)
- `--border-color`: #e0e0e0

### Status Colors
- `--status-success`: #4caf50
- `--status-warning`: #ff9800
- `--status-error`: #f44336
- `--status-info`: var(--oi-blue)

## Spacing Scale

The spacing scale uses a consistent rem-based system:

- `--spacing-xs`: 0.25rem (4px)
- `--spacing-sm`: 0.5rem (8px)
- `--spacing-md`: 0.75rem (12px)
- `--spacing-lg`: 1rem (16px)
- `--spacing-xl`: 1.5rem (24px)
- `--spacing-2xl`: 2rem (32px)
- `--spacing-3xl`: 3rem (48px)

## Typography Scale

### Font Sizes
- `--text-xs`: 0.75rem (12px)
- `--text-sm`: 0.875rem (14px)
- `--text-base`: 1rem (16px)
- `--text-lg`: 1.125rem (18px)
- `--text-xl`: 1.25rem (20px)
- `--text-2xl`: 1.5rem (24px)
- `--text-3xl`: 1.875rem (30px)

### Font Weights
- `--font-normal`: 400
- `--font-medium`: 500
- `--font-semibold`: 600
- `--font-bold`: 700

## Adding New Components

When creating new reusable component styles:

1. Create a new file in `styles/components/` (e.g., `cards.css`)
2. Add the import to `styles/theme.css`:
   ```css
   @import './components/cards.css';
   ```
3. Use theme variables for consistency:
   ```css
   .card {
     background: var(--bg-secondary);
     padding: var(--spacing-xl);
     border-radius: var(--radius-lg);
     box-shadow: var(--shadow-md);
   }
   ```

## Best Practices

1. **Always use theme variables** instead of hardcoded values
2. **Use semantic color names** (e.g., `--text-primary` not `--color-gray-900`)
3. **Maintain consistent spacing** using the spacing scale
4. **Keep component styles modular** - one file per component type
5. **Test color contrast** for accessibility (especially text on backgrounds)
6. **Use rem units** for spacing and sizing (relative to root font size)
7. **Leverage CSS custom properties** for dynamic theming

## Migration Notes

The theme system replaces inline color values and scattered CSS variables. When updating existing components:

1. Replace hardcoded colors with theme variables
2. Replace spacing values with spacing scale variables
3. Use component classes where applicable
4. Remove duplicate variable definitions

Example migration:
```css
/* Before */
.my-button {
  background: #1e88e5;
  padding: 12px 24px;
  font-weight: 500;
}

/* After */
.my-button {
  background: var(--oi-blue);
  padding: var(--spacing-md) var(--spacing-xl);
  font-weight: var(--font-medium);
}
```
