// Probe plugin for scenario s12_plugin: proves the packed runtime loads a plugin from an absolute path without
// changing Harness sources, and that the plugin receives the same session events the ACP adapter consumes.
// Each record goes as one JSON line to the file named by SHUTTLEGENT_EVENT_TAP_LOG.

import { appendFileSync } from 'node:fs'

export const name = 'shuttlegent-event-tap'

export function apply(ctx) {
  const file = process.env.SHUTTLEGENT_EVENT_TAP_LOG
  const write = (record) => {
    if (file) appendFileSync(file, JSON.stringify(record) + '\n')
  }

  write({ kind: 'loaded' })
  ctx.on('session/event', (session, event) => {
    write({ kind: 'session/event', session: session?.header?.id, type: event?.type, keys: event ? Object.keys(event) : [] })
  })
  ctx.on('agent/status', ({ status }) => write({ kind: 'agent/status', status }))
}
