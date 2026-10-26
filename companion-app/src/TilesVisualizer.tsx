
import { useEffect, useState } from 'react'
import type { CSSProperties } from 'react'
import './TilesVisualizer.css'

type ColorRole =
  | 'root'
  | 'fifth'
  | 'third'
  | 'note'
  | 'accidental'

const DEFAULT_COLORS: Record<ColorRole, string> = {
  root: '660066',
  fifth: '240066',
  third: 'none',
  note: '363636',
  accidental: '000000',
}

const ROLES = Object.keys(DEFAULT_COLORS) as ColorRole[]

const BUTTONS = ['−', '+', '△', '◇', '□', '○']

// Default C chromatic layout.
// Pads 1-6 are top, 19-24 are bottom.
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

function padColor(
  role: ColorRole,
  colors: Record<ColorRole, string>
) {
  // A disabled role falls back to the scale note color.
  const value =
    colors[role] === 'none' ? colors.note : colors[role]

  return /^[0-9a-f]{6}$/i.test(value)
    ? `#${value}`
    : '#242633'
}

export function TilesVisualizer() {
  const [colors, setColors] = useState(DEFAULT_COLORS)
  const [activePad, setActivePad] = useState<number | null>(null)

  useEffect(() => {
    let cancelled = false

    // Read settings from the real device or simulator.
    async function loadColors() {
      for (const role of ROLES) {
        try {
          const [value] = await window.tiles.usb.command(
            `GET color.${role}`
          )

          if (
            cancelled ||
            !(/^[0-9a-f]{6}$/i.test(value) || value === 'none')
          ) continue

          setColors(prev => ({ ...prev, [role]: value }))
        } catch (error) {
          console.warn(`Unable to read color.${role}`, error)
        }
      }
    }

    // Respond to settings updated elsewhere in the UI.
    function handleSettingChange(event: Event) {
      const { key, value } = (
        event as CustomEvent<{ key: string; value: string }>
      ).detail

      if (!key.startsWith('color.')) return

      const role = key.slice(6) as ColorRole
      if (!ROLES.includes(role)) return

      setColors(prev => ({ ...prev, [role]: value }))
    }

    window.addEventListener(
      'tiles:setting-changed',
      handleSettingChange
    )

    void loadColors()

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
        <span>24 Pads · C Chromatic</span>
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
            const color = padColor(role, colors)

            return (
              <button
                key={pad}
                type="button"
                className={`tiles-pad ${
                  activePad === pad ? 'is-active' : ''
                }`}
                style={{
                  '--pad-color': color,
                } as CSSProperties}
                title={`Pad ${pad} · ${role}`}
                aria-label={`Preview pad ${pad}`}
                aria-pressed={activePad === pad}
                onClick={() =>
                  setActivePad(
                    activePad === pad ? null : pad
                  )
                }
              >
                <span>{pad}</span>
              </button>
            )
          })}
        </div>
      </div>

      <p className="tiles-preview-note">
        Preview only · Select a pad to simulate a press
      </p>
    </section>
  )
}
