/**
 * Spot value conversion utilities
 *
 * Dynamically converts values with compound units (e.g., "10ms", "100us")
 * to their base units by parsing the numeric prefix and unit suffix.
 */

export interface ConversionResult {
  value: number
  unit: string
}

/**
 * Recognized time units (no conversion factors needed here)
 */
const TIME_UNITS = new Set(['s', 'ms', 'us', 'μs', 'ns', 'min', 'h', 'd'])

/**
 * Parse a unit string that may contain a numeric prefix
 * Examples: "10ms" -> { multiplier: 10, baseUnit: "ms" }
 *           "ms" -> { multiplier: 1, baseUnit: "ms" }
 *           "100us" -> { multiplier: 100, baseUnit: "us" }
 */
function parseUnit(unit: string): { multiplier: number; baseUnit: string } | null {
  if (!unit) return null

  // Try to match a number at the start of the unit string
  const match = unit.match(/^(\d+(?:\.\d+)?)(.+)$/)

  if (match) {
    // Has numeric prefix (e.g., "10ms")
    return {
      multiplier: parseFloat(match[1]),
      baseUnit: match[2]
    }
  }

  // No numeric prefix, treat as base unit with multiplier 1
  return {
    multiplier: 1,
    baseUnit: unit
  }
}

/**
 * Apply conversion to a spot value based on its unit
 *
 * For units with numeric prefixes (e.g., "10ms", "100us"), converts the value
 * by applying the multiplier and normalizes the unit to remove the prefix.
 * Example: rawValue=12345 with unit="10ms" → value=123450 with unit="ms"
 *
 * @param rawValue - The raw numeric value from the device
 * @param unit - The unit string from the parameter definition
 * @returns Object with converted value and display unit
 */
export function convertSpotValue(rawValue: number, unit?: string): ConversionResult {
  if (!unit) {
    return { value: rawValue, unit: '' }
  }

  const parsed = parseUnit(unit)
  if (!parsed) {
    return { value: rawValue, unit }
  }

  const { multiplier, baseUnit } = parsed

  // Check if it's a recognized unit type and has a numeric prefix
  if (multiplier !== 1 && TIME_UNITS.has(baseUnit)) {
    // Apply the multiplier to normalize to the base unit
    // Example: 12345 in "10ms" → 12345 × 10 = 123450 "ms"
    return {
      value: rawValue * multiplier,
      unit: baseUnit
    }
  }

  // No conversion needed, return as-is
  return { value: rawValue, unit }
}

/**
 * Check if a unit can be converted
 */
export function hasConversion(unit?: string): boolean {
  if (!unit) return false

  const parsed = parseUnit(unit)
  if (!parsed) return false

  // Check if it's a recognized unit with a non-1 multiplier
  return parsed.multiplier !== 1 && TIME_UNITS.has(parsed.baseUnit)
}
