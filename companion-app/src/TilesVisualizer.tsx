
import { useEffect, useState } from 'react'
import type { CSSProperties } from 'react'
import './TilesVisualizer.css'

type ColorRole =
  | 'root'
  | 'fifth'
  | 'third'
  | 'note'
  | 'accidental'

type PreviewMode = 'press' | 'primary' | 'secondary'

const BUTTONS = ['−', '+', '△', '◇', '□', '○']

const DEFAULT_COLORS: Record<ColorRole, string> = {
  root: '660066',
  fifth: '240066',
  third: 'none',
  note: '363636',
  accidental: '000000',
}

const DEFAULT_LOOK: Record<string, number> = {
  'look.idle_baseline_percent': 50,
  'look.natural_pad_percent': 21,
  'look.root_pad_percent': 40,
  'look.fifth_pad_percent': 40,
  'look.fifth_red_tint_percent': 35,
  'look.echo_sustain_tint_percent': 35,
  'look.echo_secondary_g_percent': 18,
  'look.echo_secondary_b_percent': 12,
  'look.echo_flash_ms': 180,
}

const LEGACY_KEYS = [
  'look.natural_pad_percent',
  'look.root_pad_percent',
  'look.fifth_pad_percent',
  'look.fifth_red_tint_percent',
]

function rgb(r: number, g: number, b: number) {
  const hex = (percent: number) =>
    Math.round(
      Math.max(0, Math.min(100, percent)) * 2.55
    )
      .toString(16)
      .padStart(2, '0')

  return `#${hex(r)}${hex(g)}${hex(b)}`
}

// C chromatic: pad 19 is the lowest C.
// The numbering runs left-to-right, top-to-bottom.
function getPadRole(pad: number): ColorRole {
  const row = Math.floor((pad - 1) / 6)
  const col = (pad - 1) % 6
  const semitone = ((3 - row) * 6 + col) % 12

  if (semitone === 0) return 'root'
  if (semitone === 7) return 'fifth'
  if (semitone === 4) return 'third'
  if ([1, 3, 6, 8, 10].includes(semitone))
    return 'accidental'

  return 'note'
}

function legacyPadColor(
  role: ColorRole,
  look: Record<string, number>
) {
  const root = look['look.root_pad_percent']
  const fifth = look['look.fifth_pad_percent']
  const tint = look['look.fifth_red_tint_percent']
  const natural = look['look.natural_pad_percent']

  if (role === 'root')
    return rgb(root, 0, root)

  if (role === 'fifth')
    return rgb(fifth * tint / 100, 0, fifth)

  if (role === 'accidental')
    return '#000000'

  // Older firmware did not highlight thirds separately.
  return rgb(natural, natural, natural)
}

function modernPadColor(
  role: ColorRole,
  colors: Record<ColorRole, string>
) {
  const value =
    colors[role] === 'none'
      ? colors.note
      : colors[role]

  return /^[0-9a-f]{6}$/i.test(value)
    ? `#${value}`
    : '#000000'
}

function previewPadColor(
  mode: PreviewMode,
  look: Record<string, number>
) {
  if (mode === 'primary') {
    const tint = look['look.echo_sustain_tint_percent']
    return rgb(tint, 100, tint)
  }

  if (mode === 'secondary') {
    return rgb(
      100,
      look['look.echo_secondary_g_percent'],
      look['look.echo_secondary_b_percent']
    )
  }

  // Simulate a 35% press so the idle baseline
  // affects the visible brightness.
  const baseline = look['look.idle_baseline_percent']
  const level = baseline + (100 - baseline) * 0.35

  return rgb(level, level, level)
}

export function TilesVisualizer() {
  const [colors, setColors] = useState(DEFAULT_COLORS)
  const [look, setLook] = useState(DEFAULT_LOOK)
  const [lightingMode, setLightingMode] =
    useState<'legacy' | 'modern'>('legacy')

  const [activePad, setActivePad] = useState<number | null>(null)
  const [previewMode, setPreviewMode] =
    useState<PreviewMode>('press')

  useEffect(() => {
    let cancelled = false

    async function loadSettings() {
      let legacySupported = false
      let modernSupported = false

      // Read all percentage-based lighting values.
      for (const key of Object.keys(DEFAULT_LOOK)) {
        try {
          const [value] = await window.tiles.usb.command(
            `GET ${key}`
          )

          if (cancelled) return

          if (value && !value.startsWith('ERR ') &&
              Number.isFinite(Number(value))) {
            setLook(prev => ({
              ...prev,
              [key]: Number(value),
            }))

            if (LEGACY_KEYS.includes(key))
              legacySupported = true
          }
        } catch (error) {
          console.warn(`GET ${key} failed`, error)
        }
      }

      // Also check for newer firmware color settings.
      for (const role of Object.keys(DEFAULT_COLORS) as ColorRole[]) {
        try {
          const [value] = await window.tiles.usb.command(
            `GET color.${role}`
          )

          if (cancelled) return

          if (value === 'none' || /^[0-9a-f]{6}$/i.test(value)) {
            modernSupported = true
            setColors(prev => ({
              ...prev,
              [role]: value,
            }))
          }
        } catch (error) {
          console.warn(`GET color.${role} failed`, error)
        }
      }

      if (!cancelled) {
        setLightingMode(
          modernSupported ? 'modern'
          : legacySupported ? 'legacy'
          : 'legacy'
        )
      }
    }

    function handleSettingChange(event: Event) {
      const { key, value } = (
        event as CustomEvent<{
          key: string
          value: string
        }>
      ).detail

      // Update the visualizer whenever a slider changes.
      if (key in DEFAULT_LOOK) {
        const number = Number(value)

        if (!Number.isFinite(number)) return

        setLook(prev => ({
          ...prev,
          [key]: number,
        }))

        if (LEGACY_KEYS.includes(key))
          setLightingMode('legacy')

        return
      }

      // Support the newer RGB color scheme too.
      if (key.startsWith('color.')) {
        const role = key.slice(6) as ColorRole

        if (!(role in DEFAULT_COLORS)) return

        setColors(prev => ({
          ...prev,
          [role]: value,
        }))

        setLightingMode('modern')
      }
    }

    window.addEventListener(
      'tiles:setting-changed',
      handleSettingChange
    )

    void loadSettings()

    return () => {
      cancelled = true
      window.removeEventListener(
        'tiles:setting-changed',
        handleSettingChange
      )
    }
  }, [])

  return (
    <section className="tiles-visualizer">
      <div className="tiles-visualizer-heading">
        <h2>Instrument Preview</h2>
        <span>
          24 Pads · C Chromatic · {lightingMode}
        </span>
      </div>

      <div className="tiles-device">
        <div className="tiles-device-header">
          <span className="tiles-brand">SENTIA / TILES</span>

          <div className="tiles-buttons">
            {BUTTONS.map((button, index) => (
              <span
                key={index}
                className="tiles-function-button"
              >
                {button}
              </span>
            ))}
          </div>
        </div>

        <div className="tiles-pad-grid">
          {Array.from({ length: 24 }, (_, index) => {
            const pad = index + 1
            const role = getPadRole(pad)
            const isActive = activePad === pad

            const idleColor =
              lightingMode === 'legacy'
                ? legacyPadColor(role, look)
                : modernPadColor(role, colors)

            const color = isActive
              ? previewPadColor(previewMode, look)
              : idleColor

            return (
              <button
                key={pad}
                type="button"
                className={`tiles-pad ${
                  isActive ? 'is-active' : ''
                }`}
                style={{
                  '--pad-color': color,
                } as CSSProperties}
                title={`Pad ${pad} · ${role}`}
                aria-label={`Preview pad ${pad}`}
                aria-pressed={isActive}
                onClick={() =>
                  setActivePad(isActive ? null : pad)
                }
              >
                <span>{pad}</span>
              </button>
            )
          })}
        </div>
      </div>

      <div style={{ marginTop: 20, textAlign: 'center' }}>
        <label>
          Preview selected pad:{' '}
          <select
            value={previewMode}
            onChange={e =>
              setPreviewMode(e.currentTarget.value as PreviewMode)
            }
          >
            <option value="press">Press (35%)</option>
            <option value="primary">Primary Echo</option>
            <option value="secondary">Secondary Echo</option>
          </select>
        </label>
      </div>

      <p className="tiles-preview-note">
        Select a pad to simulate a press or echo.
        Echo preview shows the settled color.
      </p>
    </section>
  )
}
