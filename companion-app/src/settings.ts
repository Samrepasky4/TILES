import { useEffect, useState } from 'react'

export function useTilesSettings(keys: Record<string, string>) {
  const [values, setValues] = useState<Record<string, string>>({})
  const [error, setError] = useState('')

  useEffect(() => {
    Object.entries(keys).forEach(async ([name, key]) => {
      const [value] = await window.tiles.usb.command(`GET ${key}`)
      if (value.startsWith('ERR ')) return setError(`${key}: ${value}`)
      setValues(v => ({ ...v, [name]: value }))
    })
  }, [])

  async function update(name: string, value: string) {
  const key = keys[name]

  try {
    const [response] = await window.tiles.usb.command(
      `SET ${key} ${value}`
    )

    if (response !== 'OK') {
      setError(`${key}: ${response}`)
      return
    }

    setValues(prev => ({
      ...prev,
      [name]: value,
    }))

    // Notify the instrument visualizer
    window.dispatchEvent(
      new CustomEvent('tiles:setting-changed', {
        detail: {
          key,
          value,
        },
      })
    )

    setError('')
  } catch (err) {
    setError(String(err))
  }
}

  return { values, update, error }
}