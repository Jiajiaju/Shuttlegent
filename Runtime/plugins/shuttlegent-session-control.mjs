// Shuttlegent session control: lets the editor cut a conversation at one of its user messages. ACP has no method for
// it, so this plugin listens on a localhost port guarded by the editor endpoint token and announces the port to the
// editor endpoint (notification shuttlegent/runtimeControl). A fork copies the source session's event log up to the
// cut into a new, ordinary session (no parentSession, so session/resume accepts it), writes it to storage and lets go
// of it; the editor then resumes it over ACP.
// Loaded by the patch the editor generates before every launch; the endpoint URL and token come from the environment.

import { createServer } from 'node:http'
import { randomUUID } from 'node:crypto'

export const name = 'shuttlegent-session-control'

const ANNOUNCE_METHOD = 'shuttlegent/runtimeControl'
const MAX_BODY_BYTES = 65536

// session-log-deepseek's acceptance marker, and the format generation that marks one as not this session's
const DELIVERY_MARKER = 'session-log-deepseek/delivery-accepted'
const LEGACY_FORMAT_VERSION = 0

class ControlError extends Error {}

export function apply(ctx) {
  const endpoint = process.env.SHUTTLEGENT_EDITOR_ENDPOINT
  const token = process.env.SHUTTLEGENT_EDITOR_TOKEN
  if (!endpoint || !token) return

  /**
   * Inclusive event index the copy keeps: before = everything ahead of the user message, the inbox splice that
   * queued it included (else resuming would run the message again); after = through the end of its turn.
   * userMessage counts what the user typed: user/message events whose source is the user, from 0. The runtime also
   * records its own context snapshots and reminders as user/message, with another source.
   */
  const boundaryOf = (events, userMessage, cut) => {
    let seen = -1
    let message = -1
    for (let index = 0; index < events.length; ++index) {
      const event = events[index]
      if (event.type !== 'user/message' || event.data?.source?.kind !== 'user') continue
      if (++seen === userMessage) {
        message = index
        break
      }
    }
    if (message < 0) throw new ControlError(`the session has no user message ${userMessage}`)

    if (cut === 'before') {
      const id = events[message].data?.id
      for (let index = message; index >= 0; --index) {
        const event = events[index]
        if (event.type === 'agent/inbox/spliced' && id !== undefined && event.data?.inserted?.some((item) => item?.id === id)) return index - 1
      }
      for (let index = message; index >= 0; --index) {
        if (events[index].type === 'turn/start') return index - 1
      }
      return message - 1
    }
    for (let index = message; index < events.length; ++index) {
      if (events[index].type === 'turn/end') return index
    }
    throw new ControlError('the turn of that message has not ended')
  }

  const fork = async ({ sessionId, userMessage, cut }) => {
    if (typeof sessionId !== 'string' || !Number.isInteger(userMessage) || userMessage < 0 || (cut !== 'before' && cut !== 'after')) {
      throw new ControlError('fork needs sessionId, userMessage (0-based) and cut (before or after)')
    }
    const sessions = ctx.get('sessions')
    const persistence = ctx.get('sessionPersistence')
    const source = sessions?.get(sessionId)
    if (!source) throw new ControlError(`session ${sessionId} is not open in the runtime`)
    if (!persistence) throw new ControlError('the runtime has no session storage')

    const events = source.snapshotEvents()
    const boundary = boundaryOf(events, userMessage, cut)
    const id = randomUUID()

    // DeepSeek delivery markers name the session whose log the server accepted. Kept current, they name the wrong
    // session and storage refuses the copy as corrupt; marked as an older format generation they are ignored by both
    // the format check and the delivery fold, and the new session sends its whole log on its first request
    const seed = events.slice(0, boundary + 1).map((event) => event.type === DELIVERY_MARKER
      ? { ...event, data: { ...event.data, sessionFormatVersion: LEGACY_FORMAT_VERSION } }
      : event)

    // Built but never entered into the live store: the copy goes straight to storage, where session/resume finds it
    const child = sessions.prepare(id, {
      seed,
      meta: source.header.cwd === undefined ? {} : { cwd: source.header.cwd },
    })
    const handle = await persistence.create(child.header)
    try {
      const copied = child.snapshotEvents()
      if (copied.length > 0) await handle.append(copied)
      await handle.flush()
    } finally {
      await handle.close()
    }
    const userMessages = seed.filter((event) => event.type === 'user/message' && event.data?.source?.kind === 'user').length
    return { sessionId: id, events: boundary + 1, userMessages }
  }

  const reply = (response, status, body) => {
    response.writeHead(status, { 'Content-Type': 'application/json' })
    response.end(JSON.stringify(body))
  }

  const server = createServer((request, response) => {
    if (request.method !== 'POST' || request.headers.authorization !== `Bearer ${token}`) {
      reply(response, 403, { error: 'forbidden' })
      return
    }

    const chunks = []
    let size = 0
    request.on('data', (chunk) => {
      size += chunk.length
      if (size <= MAX_BODY_BYTES) chunks.push(chunk)
    })
    request.on('end', async () => {
      let message
      try {
        message = JSON.parse(Buffer.concat(chunks).toString('utf8'))
      } catch {
        reply(response, 400, { error: 'body is not JSON' })
        return
      }
      if (message?.method !== 'fork') {
        reply(response, 400, { error: `unknown method ${message?.method}` })
        return
      }
      try {
        reply(response, 200, { result: await fork(message.params ?? {}) })
      } catch (error) {
        reply(response, error instanceof ControlError ? 400 : 500, { error: error?.message ?? String(error) })
      }
    })
  })

  ctx.effect(() => {
    server.listen(0, '127.0.0.1', () => {
      const body = JSON.stringify({ jsonrpc: '2.0', method: ANNOUNCE_METHOD, params: { url: `http://127.0.0.1:${server.address().port}/` } })
      fetch(endpoint, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${token}` },
        body,
      }).catch((error) => console.warn(`[${name}] control port not announced: ${error?.message ?? error}`))
    })
    return () => server.close()
  }, 'shuttlegent-session-control server')
}
