interface DataPoint {
  timestamp: number
  value: number
}

interface DataSeries {
  label: string
  unit?: string
  color: string
  data: DataPoint[]
}

interface MultiLineChartProps {
  series: DataSeries[]
  width?: number
  height?: number
  showLegend?: boolean
}

const COLORS = [
  '#4CAF50', // Green
  '#2196F3', // Blue
  '#FF9800', // Orange
  '#9C27B0', // Purple
  '#F44336', // Red
  '#00BCD4', // Cyan
  '#FFEB3B', // Yellow
  '#795548', // Brown
  '#E91E63', // Pink
  '#3F51B5', // Indigo
  '#009688', // Teal
  '#FF5722', // Deep Orange
  '#8BC34A', // Light Green
  '#673AB7', // Deep Purple
  '#FFC107', // Amber
  '#607D8B', // Blue Grey
  '#00E676', // Bright Green
  '#FF4081', // Hot Pink
  '#536DFE', // Bright Blue
  '#FFD740', // Bright Yellow
]

export default function MultiLineChart({
  series,
  width = 800,
  height = 400,
  showLegend = true
}: MultiLineChartProps) {
  if (series.length === 0 || series.every(s => s.data.length === 0)) {
    return (
      <div class="chart-placeholder" style={{ width: `${width}px`, height: `${height}px` }}>
        <p>No data yet. Start monitoring to see chart.</p>
      </div>
    )
  }

  const padding = { top: 10, right: showLegend ? 100 : 15, bottom: 40, left: 35 }
  const chartWidth = width - padding.left - padding.right
  const chartHeight = height - padding.top - padding.bottom

  // Collect all data points from all series
  const allDataPoints = series.flatMap(s => s.data)

  if (allDataPoints.length === 0) {
    return (
      <div class="chart-placeholder" style={{ width: `${width}px`, height: `${height}px` }}>
        <p>No data available</p>
      </div>
    )
  }

  // Calculate min/max for scaling
  const allValues = allDataPoints.map(d => d.value)
  const allTimestamps = allDataPoints.map(d => d.timestamp)

  const minValue = Math.min(...allValues)
  const maxValue = Math.max(...allValues)
  const minTime = Math.min(...allTimestamps)
  const maxTime = Math.max(...allTimestamps)

  const valueRange = maxValue - minValue || 1
  const timeRange = maxTime - minTime || 1

  // Scale functions
  const scaleX = (timestamp: number) => {
    return padding.left + ((timestamp - minTime) / timeRange) * chartWidth
  }

  const scaleY = (value: number) => {
    return padding.top + chartHeight - ((value - minValue) / valueRange) * chartHeight
  }

  // Generate Y-axis ticks
  const yTicks = 5
  const yTickValues = Array.from({ length: yTicks }, (_, i) => {
    return minValue + (valueRange * i) / (yTicks - 1)
  })

  // Generate X-axis ticks (time labels)
  const xTicks = 6
  const xTickValues = Array.from({ length: xTicks }, (_, i) => {
    return minTime + (timeRange * i) / (xTicks - 1)
  })

  const formatTime = (timestamp: number) => {
    const seconds = Math.floor(timestamp / 1000)
    return `${seconds}s`
  }

  const formatValue = (value: number) => {
    if (Math.abs(value) < 0.01 && value !== 0) return value.toExponential(2)
    if (Math.abs(value) > 10000) return value.toExponential(2)
    return value.toFixed(2)
  }

  return (
    <div class="multi-line-chart">
      <svg width={width} height={height}>
        {/* Y-axis */}
        <line
          x1={padding.left}
          y1={padding.top}
          x2={padding.left}
          y2={height - padding.bottom}
          stroke="#ccc"
          stroke-width="1"
        />

        {/* X-axis */}
        <line
          x1={padding.left}
          y1={height - padding.bottom}
          x2={width - padding.right}
          y2={height - padding.bottom}
          stroke="#ccc"
          stroke-width="1"
        />

        {/* Y-axis ticks and labels */}
        {yTickValues.map((value, i) => {
          const y = scaleY(value)
          return (
            <g key={i}>
              <line
                x1={padding.left - 5}
                y1={y}
                x2={padding.left}
                y2={y}
                stroke="#ccc"
                stroke-width="1"
              />
              <line
                x1={padding.left}
                y1={y}
                x2={width - padding.right}
                y2={y}
                stroke="#eee"
                stroke-width="1"
                stroke-dasharray="2,2"
              />
              <text
                x={padding.left - 10}
                y={y}
                text-anchor="end"
                dominant-baseline="middle"
                font-size="12"
                fill="#666"
              >
                {formatValue(value)}
              </text>
            </g>
          )
        })}

        {/* X-axis ticks and labels */}
        {xTickValues.map((timestamp, i) => {
          const x = scaleX(timestamp)
          return (
            <g key={i}>
              <line
                x1={x}
                y1={height - padding.bottom}
                x2={x}
                y2={height - padding.bottom + 5}
                stroke="#ccc"
                stroke-width="1"
              />
              <text
                x={x}
                y={height - padding.bottom + 20}
                text-anchor="middle"
                font-size="12"
                fill="#666"
              >
                {formatTime(timestamp)}
              </text>
            </g>
          )
        })}

        {/* Data lines for each series */}
        {series.map((s, seriesIndex) => {
          if (s.data.length === 0) return null

          const color = s.color || COLORS[seriesIndex % COLORS.length]

          // Generate path for line
          const linePath = s.data
            .map((point, index) => {
              const x = scaleX(point.timestamp)
              const y = scaleY(point.value)
              return `${index === 0 ? 'M' : 'L'} ${x} ${y}`
            })
            .join(' ')

          return (
            <g key={seriesIndex}>
              {/* Line */}
              <path
                d={linePath}
                fill="none"
                stroke={color}
                stroke-width="2"
                opacity="0.8"
              />

              {/* Data points */}
              {s.data.map((point, i) => (
                <circle
                  key={i}
                  cx={scaleX(point.timestamp)}
                  cy={scaleY(point.value)}
                  r="3"
                  fill={color}
                />
              ))}
            </g>
          )
        })}

        {/* Legend */}
        {showLegend && series.map((s, index) => {
          const color = s.color || COLORS[index % COLORS.length]
          const yPos = padding.top + index * 25

          return (
            <g key={index}>
              <line
                x1={width - padding.right + 10}
                y1={yPos}
                x2={width - padding.right + 30}
                y2={yPos}
                stroke={color}
                stroke-width="2"
              />
              <circle
                cx={width - padding.right + 20}
                cy={yPos}
                r="3"
                fill={color}
              />
              <text
                x={width - padding.right + 35}
                y={yPos}
                dominant-baseline="middle"
                font-size="11"
                fill="#666"
              >
                {s.label} {s.unit && `(${s.unit})`}
              </text>
            </g>
          )
        })}

        {/* X-axis label */}
        <text
          x={width / 2}
          y={height - 5}
          text-anchor="middle"
          font-size="12"
          fill="#666"
        >
          Time
        </text>
      </svg>
    </div>
  )
}

export { COLORS }
export type { DataSeries, DataPoint }
