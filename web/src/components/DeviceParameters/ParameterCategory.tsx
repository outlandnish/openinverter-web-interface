import ParameterInput from './ParameterInput'

interface ParameterCategoryProps {
  category: string
  params: Array<[string, any]>
  isCollapsed: boolean
  onToggle: (category: string) => void
  getDisplayName: (key: string) => string
  isConnected: boolean
  onUpdate: (paramId: number, newValue: number) => void
}

export default function ParameterCategory({
  category,
  params,
  isCollapsed,
  onToggle,
  getDisplayName,
  isConnected,
  onUpdate
}: ParameterCategoryProps) {
  return (
    <div class={`parameter-category${isCollapsed ? ' collapsed' : ''}`}>
      <h3
        class="category-title"
        onClick={() => onToggle(category)}
        style={{ cursor: 'pointer' }}
      >
        <span class="collapse-icon">{isCollapsed ? '▶' : '▼'}</span>
        {category}
        <span class="param-count">({params.length})</span>
      </h3>
      {!isCollapsed && (
        <div class="parameters-list">
          {params.map(([key, param]) => (
            <ParameterInput
              key={key}
              paramKey={key}
              param={param}
              displayName={getDisplayName(key)}
              isConnected={isConnected}
              onUpdate={onUpdate}
            />
          ))}
        </div>
      )}
    </div>
  )
}
