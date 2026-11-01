// Shuttlegent session tap: forwards the session events the ACP adapter drops (todo list, plan mode, compaction, goal)
// to the editor endpoint as the JSON-RPC notification shuttlegent/sessionEvent, one per event, in order. Subagents
// are not forwarded: the Shuttlegent profile turns the subagent tools off, since a child session cannot reach the
// editor. Payloads are trimmed to what the panel shows; the full record stays in the session log.
// Loaded by the patch the editor generates before every launch; the endpoint URL and token come from the environment.
// A failure only logs once: the conversation never depends on this plugin.

export const name = 'shuttlegent-session-tap'

const METHOD = 'shuttlegent/sessionEvent'

/** Picks what the panel needs from each forwarded event type; anything else is not sent. */
const SESSION_EVENTS = {
  'todo/write': (data) => ({ todos: (data.todos ?? []).map((todo) => ({ content: todo.content, status: todo.status })) }),
  'plan/mode': (data) => ({ active: data.active === true }),
  'compaction/start': (data) => ({ compactionId: data.compactionId, manual: data.turn === null }),
  'compaction/summary': (data) => ({ compactionId: data.compactionId, shadowedTokenCount: data.shadowedTokenCount }),
  'compaction/end': (data) => ({ compactionId: data.compactionId, error: data.error === undefined ? undefined : String(data.error?.message ?? data.error) }),
  'compaction/prune': (data) => ({ shadowedTokenCount: data.shadowedTokenCount }),
  'goal/change': (data) => data.operation === 'clear'
    ? { operation: 'clear' }
    : { operation: data.operation, objective: data.goal?.objective, phase: data.goal?.phase },
}

export function apply(ctx) {
  const url = process.env.SHUTTLEGENT_EDITOR_ENDPOINT
  const token = process.env.SHUTTLEGENT_EDITOR_TOKEN
  if (!url || !token) return

  let warned = false
  const warn = (error) => {
    if (warned) return
    warned = true
    console.warn(`[${name}] session event not sent: ${error?.message ?? error}`)
  }

  // One request at a time, so the editor receives events in the order the session logged them
  let queue = Promise.resolve()
  const send = (params) => {
    const body = JSON.stringify({ jsonrpc: '2.0', method: METHOD, params })
    queue = queue.then(() => fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${token}` },
      body,
    })).catch(warn)
  }

  /** Context occupancy after a compaction, measured as the ACP adapter's usage_update measures it. */
  const usedTokens = (session) => {
    try {
      return ctx.get('tokenMeter')?.measure(session)?.totalTokens
    } catch {
      return undefined
    }
  }

  ctx.on('session/event', (session, event) => {
    const header = session?.header
    if (!header || !event?.type) return

    const pick = SESSION_EVENTS[event.type]
    if (!pick) return

    let data
    try {
      data = pick(event.data ?? {})
    } catch (error) {
      warn(error)
      return
    }
    if (event.type === 'compaction/end') data.usedTokens = usedTokens(session)

    send({ sessionId: header.id, type: event.type, data })
  })
}
