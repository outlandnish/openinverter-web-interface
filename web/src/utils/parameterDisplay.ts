/**
 * Formats a parameter value for display based on its type
 *
 * @param param - The parameter object containing enums, unit, etc.
 * @param value - The raw value to format (number, string, null, or undefined)
 * @returns Formatted string for display, or empty string if value is null/undefined
 */
export function formatParameterValue(
  param: any,
  value: number | string | null | undefined
): string {
  // Handle null/undefined - show nothing
  if (value === null || value === undefined) {
    return ''
  }

  // Debug logging
  if (param.unit && param.unit.includes('=')) {
    console.warn('Parameter has unparsed enum in unit field:', {
      unit: param.unit,
      value,
      hasEnums: !!param.enums
    })
  }

  // Handle enum values - show the mapped label
  if (param.enums && Object.keys(param.enums).length > 0) {
    const enumValue = String(Math.round(Number(value)))
    const label = param.enums[enumValue]
    console.log('Enum conversion:', { value, enumValue, label, enums: param.enums })
    return label || String(value)
  }

  // Handle numeric values with units
  if (typeof value === 'number') {
    const formattedValue = value.toFixed(2)
    return param.unit ? `${formattedValue} ${param.unit}` : formattedValue
  }

  // Fallback to string value
  return String(value)
}

/**
 * Gets the enum label for a numeric value
 *
 * @param param - The parameter object containing enums
 * @param value - The numeric value to convert
 * @returns The enum label or the original value as string
 */
export function getEnumLabel(
  param: any,
  value: number | string
): string {
  if (!param.enums || Object.keys(param.enums).length === 0) {
    return String(value)
  }

  const enumValue = String(Math.round(Number(value)))
  return param.enums[enumValue] || String(value)
}

/**
 * Converts a parameter value to a string suitable for dropdown selection
 * Rounds numeric values and converts to string to match enum keys
 *
 * @param value - The value to convert
 * @returns String representation suitable for dropdown value
 */
export function normalizeEnumValue(value: number | string | null | undefined): string {
  if (value === null || value === undefined) {
    return '0'
  }
  return String(Math.round(Number(value)))
}
