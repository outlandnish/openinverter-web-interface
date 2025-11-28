interface DataPoint {
  timestamp: number
  value: number
}

interface LineChartProps {
  data: DataPoint[]
  label: string
  unit?: string
  color?: string
  width?: number
  height?: number
}

export default function LineChart({
  data,
  label,
  unit = '',
  color = '#4CAF50',
  width = 600,
  height = 300
}: LineChartProps) {
  if (data.length === 0) {
    return (
      <div class="chart-placeholder" style={{ width: `${width}px`, height: `${height}px` }}>
        <p>No data yet. Start monitoring to see chart.</p>
      </div>
    )
  }

  const padding = { top: 20, right: 50, bottom: 40, left: 60 }
  const chartWidth = width - padding.left - padding.right
  const chartHeight = height - padding.top - padding.bottom

  // Calculate min/max for scaling
  const values = data.map(d => d.value)
  const timestamps = data.map(d => d.timestamp)

  const minValue = Math.min(...values)
  const maxValue = Math.max(...values)
  const minTime = Math.min(...timestamps)
  const maxTime = Math.max(...timestamps)

  const valueRange = maxValue - minValue || 1
  const timeRange = maxTime - minTime || 1

  // Scale functions
  const scaleX = (timestamp: number) => {
    return padding.left + ((timestamp - minTime) / timeRange) * chartWidth
  }

  const scaleY = (value: number) => {
    return padding.top + chartHeight - ((value - minValue) / valueRange) * chartHeight
  }

  // Generate path for line
  const linePath = data
    .map((point, index) => {
      const x = scaleX(point.timestamp)
      const y = scaleY(point.value)
      return `${index === 0 ? 'M' : 'L'} ${x} ${y}`
    })
    .join(' ')

  // Generate Y-axis ticks
  const yTicks = 5
  const yTickValues = Array.from({ length: yTicks }, (_, i) => {
    return minValue + (valueRange * i) / (yTicks - 1)
  })

  // Generate X-axis ticks (time labels)
  const xTicks = 5
  const xTickValues = Array.from({ length: xTicks }, (_, i) => {
    return minTime + (timeRange * i) / (xTicks - 1)
  })

  const formatTime = (timestamp: number) => {
    const seconds = Math.floor(timestamp / 1000)
    return `${seconds}s`
  }

  const formatValue = (value: number) => {
    if (Math.abs(value) < 0.01) return value.toExponential(2)
    if (Math.abs(value) > 10000) return value.toExponential(2)
    return value.toFixed(2)
  }

  return (
    <div class="line-chart">
      <div class="chart-title">{label}</div>
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

        {/* Data line */}
        <path
          d={linePath}
          fill="none"
          stroke={color}
          stroke-width="2"
        />

        {/* Data points */}
        {data.map((point, i) => (
          <circle
            key={i}
            cx={scaleX(point.timestamp)}
            cy={scaleY(point.value)}
            r="3"
            fill={color}
          />
        ))}

        {/* Y-axis label */}
        <text
          x={15}
          y={height / 2}
          text-anchor="middle"
          font-size="12"
          fill="#666"
          transform={`rotate(-90, 15, ${height / 2})`}
        >
          {label} {unit && `(${unit})`}
        </text>

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
