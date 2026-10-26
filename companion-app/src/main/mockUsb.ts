
import fixture from '../../fixtures/tiles-settings.json'

export const mockDevice = {
  vendorId: 0x1209,
  productId: 0x0001,
  productName: 'SENTIA TILES (Simulator)',
  manufacturerName: 'Sentia Instruments',
}

const toWire = (value: unknown): string =>
  typeof value === 'boolean'
    ? (value ? '1' : '0')
    : String(value)

const settings = new Map(
  fixture.settings.map(s => [
    s.key,
    {
      ...s,
      value: toWire(s.value),
    },
  ])
)

export async function mockCommand(
  command: string,
  _multiLine = false
): Promise<string[]> {
  console.log('[SIM]', command)

  const [action, key, ...rest] = command.trim().split(/\s+/)

  if (action === 'GET') {
    const setting = settings.get(key)
    return setting
      ? [setting.value]
      : ['ERR unknown-key']
  }

  if (action === 'SET') {
    const setting = settings.get(key)
    if (!setting) return ['ERR unknown-key']

    const value = rest.join(' ')

    if (setting.type === 'bool' && !['0', '1'].includes(value))
      return ['ERR bad-value']

    if (setting.type === 'enum' &&
        (!setting.values || !setting.values.includes(value)))
      return ['ERR out-of-range']

    if (['int', 'uint', 'float'].includes(setting.type)) {
      const n = Number(value)
      if (!value || !Number.isFinite(n))
        return ['ERR bad-value']
      if ((setting.type !== 'float' && !Number.isInteger(n)) ||
          (setting.type === 'uint' && n < 0))
        return ['ERR bad-value']
      if ((setting.min !== undefined && n < setting.min) ||
          (setting.max !== undefined && n > setting.max))
        return ['ERR out-of-range']
    }

    setting.value = value
    return ['OK']
  }

  if (action === 'LIST') {
    return Array.from(settings.values())
      .map(s => `${s.key}=${s.value}`)
  }

  return ['ERR unsupported-command']
}
