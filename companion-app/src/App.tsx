import { useEffect, useState } from 'react'
import { Pedal } from './pedal'
import { Cvgate } from './cvGate'
import { Expression } from './expression'
import { Midi } from './midi'
import { Lighting } from './lighting'
import { FeaturesHarmonics } from './featuresHarmonics'
import { TilesVisualizer } from './TilesVisualizer'
import { CustomScales } from './CustomScales'

const TILES_IDS = [
  [0x1209, 0x0001], // newer firmware
  [0x2e8a, 0x100a], // older firmware
]

function App() {
  const [connected, setConnected] = useState(false)
  const [error, setError] = useState('')
  const [simFirmware, setSimFirmware] = useState<string | null>(null)

  async function connect() {
    try {
      setError('')
      setConnected(false)
      const devices = await window.tiles.usb.list()
      const tiles = devices.find(d =>
        TILES_IDS.some(([vid, pid]) => d.vendorId === vid && d.productId === pid)
      )
      if (!tiles) throw new Error('SENTIA TILES not found')
      await window.tiles.usb.connect(tiles.vendorId, tiles.productId)
      const info = await window.tiles.usb.command('INFO', true)
      const firmware = info.find(line => line.startsWith('firmware='))?.slice(9) ?? '0.3.6'
      setSimFirmware(info.includes('simulator=1') ? firmware : null)
      setConnected(true)
    } catch (reason) {
      setError(reason instanceof Error ? reason.message : String(reason))
    }
  }

  async function changeSimFirmware(next: string) {
    try {
      setError('')
      const [response] = await window.tiles.usb.command(`SIM VERSION ${next}`)
      if (response !== 'OK') throw new Error(response)
      setSimFirmware(next)
    } catch (reason) {
      setError(String(reason))
    }
  }

  useEffect(() => { void connect() }, [])

  if (!connected) {
    return (
      <main>
        <h1>SENTIA TILES</h1>
        <p>{error || 'Connecting...'}</p>
        {error && <button onClick={() => void connect()}>Reconnect</button>}
      </main>
    )
  }

  return (
    <main>
      {simFirmware !== null && (
        <div>
          <h2>Virtual TILES · firmware {simFirmware}</h2>
          <label>Simulated firmware{' '}
            <select value={simFirmware} onChange={e => void changeSimFirmware(e.currentTarget.value)}>
              <option value="0.2.0">0.2.0 — legacy</option>
              <option value="0.3.6">0.3.6 — latest</option>
            </select>
          </label>
          <p>Simulator settings are kept in memory only and reset when the app restarts.
            Switching firmware versions resets the simulated device.</p>
        </div>
      )}
      {error && <p role="alert">{error}</p>}
      <div key={simFirmware ?? 'real-device'}>
        <TilesVisualizer />
        <Lighting />
        <CustomScales />
        <Pedal />
        <Cvgate />
        <Expression />
        <Midi />
        <FeaturesHarmonics />
      </div>
    </main>
  )
}
export default App
