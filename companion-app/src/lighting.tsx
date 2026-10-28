import { useTilesSettings } from './settings'

export function Lighting() {
  const keys = {
    idle: 'look.idle_baseline_percent',
    natural: 'look.natural_pad_percent',
    root: 'look.root_pad_percent',
    fifth: 'look.fifth_pad_percent',
    fifthRed: 'look.fifth_red_tint_percent',
    sustainTint: 'look.echo_sustain_tint_percent',
    echoGreen: 'look.echo_secondary_g_percent',
    echoBlue: 'look.echo_secondary_b_percent',
    flashMs: 'look.echo_flash_ms',
  }

  const { values, update, error } = useTilesSettings(keys)

  const percent = [
    ['idle', 'Idle Baseline'],
    ['natural', 'Natural Pad'],
    ['root', 'Root Pad'],
    ['fifth', 'Fifth Pad'],
    ['fifthRed', 'Fifth Red Tint'],
    ['sustainTint', 'Sustain Tint'],
    ['echoGreen', 'Echo Green'],
    ['echoBlue', 'Echo Blue'],
  ]
  const scheme = useTilesSettings({
  root: 'color.root',
  fifth: 'color.fifth',
  third: 'color.third',
  note: 'color.note',
  accidental: 'color.accidental',
})

  return (
    <section>
      <h2>Lighting</h2>
      <h3>Color Scheme</h3>

{[
  ['root', 'Root'],
  ['fifth', 'Fifth'],
  ['third', 'Third'],
  ['note', 'Scale Note'],
  ['accidental', 'Accidental'],
].map(([name, label]) => {
  const current = scheme.values[name] ?? 'none'
  const isOff = current === 'none'

  return (
    <label key={name} style={{ marginRight: 16 }}>
      {label}

      <input
        type="color"
        disabled={isOff}
        value={isOff ? '#000000' : `#${current}`}
        onChange={e =>
          scheme.update(
            name,
            e.currentTarget.value.slice(1).toUpperCase()
          )
        }
      />

      <input
        type="checkbox"
        checked={isOff}
        onChange={e =>
          scheme.update(
            name,
            e.currentTarget.checked
              ? 'none'
              : ({
                  root: '660066',
                  fifth: '240066',
                  third: '663300',
                  note: '363636',
                  accidental: '000000',
                } as Record<string, string>)[name]
          )
        }
      />
      Off
    </label>
  )
})}

{scheme.error && <p>{scheme.error}</p>}

      {percent.map(([name, label]) => (
        <label key={name}>
          {label} %
          <input
            type="range"
            min="0"
            max="100"
            value={values[name] ?? 0}
            onChange={e => update(name, e.currentTarget.value)}
          />
          {values[name] ?? 0}%
        </label>
      ))}

      <label>
        Echo Flash (ms)
        <input
          type="number"
          min="0"
          max="2000"
          value={values.flashMs ?? ''}
          onChange={e => update('flashMs', e.currentTarget.value)}
        />
      </label>

      {error && <p>Error: {error}</p>}
    </section>
  )
}