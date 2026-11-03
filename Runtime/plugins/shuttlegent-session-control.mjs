// Shuttlegent session control: lets the editor do what ACP has no method for. The plugin listens on a localhost port
// guarded by the editor endpoint token and announces the port to the editor endpoint (notification
// shuttlegent/runtimeControl). Methods: fork (below), command (run a slash command such as /compact), planMode (enter
// or leave plan mode), answerPlanReview (the user's verdict on a plan the model presented in plan mode), imageInput
// (whether a model route declares image input; ACP decides it once, for the default model) and nextAgent (the agent of
// the next new session, kept by shuttlegent-agent-dispatch). A fork copies the source session's event log up to the
// cut into a new, ordinary session (no parentSession, so session/resume accepts it), writes it to storage and lets go
// of it; the editor then resumes it over ACP.
// Loaded by the patch the editor generates before every launch; the endpoint URL and token come from the environment.

import { createServer } from 'node:http'
import { randomUUID } from 'node:crypto'

export const name = 'shuttlegent-session-control'

const ANNOUNCE_METHOD = 'shuttlegent/runtimeControl'
// A plan waiting for the user's review, and one that stopped waiting (turn cancelled) before an answer
const REVIEW_METHOD = 'shuttlegent/planReview'
const REVIEW_CLOSED_METHOD = 'shuttlegent/planReviewClosed'
const MAX_BODY_BYTES = 65536

// session-log-deepseek's acceptance marker, and the format generation that marks one as not this session's
const DELIVERY_MARKER = 'session-log-deepseek/delivery-accepted'
const LEGACY_FORMAT_VERSION = 0

class ControlError extends Error {}

export function apply(ctx) {
  const endpoint = process.env.SHUTTLEGENT_EDITOR_ENDPOINT
  const token = process.env.SHUTTLEGENT_EDITOR_TOKEN
  if (!endpoint || !token) return

  const notify = (method, params) => {
    fetch(endpoint, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${token}` },
      body: JSON.stringify({ jsonrpc: '2.0', method, params }),
    }).catch((error) => console.warn(`[${name}] ${method} not sent: ${error?.message ?? error}`))
  }

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

    // Built but never entered into the live store: the copy goes straight to storage, where session/resume finds it.
    // The copy keeps the source's agent (agentPreset), so the agent dispatch resumes it with the same loop
    const child = sessions.prepare(id, {
      seed,
      meta: {
        ...source.header.cwd === undefined ? {} : { cwd: source.header.cwd },
        ...source.header.agentPreset === undefined ? {} : { agentPreset: source.header.agentPreset },
      },
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

  const agentOf = (sessionId) => {
    if (typeof sessionId !== 'string') throw new ControlError('sessionId is required')
    const agent = ctx.get('agents')?.get(sessionId)
    if (!agent) throw new ControlError(`session ${sessionId} is not open in the runtime`)
    return agent
  }

  /** Runs a slash command line (/compact, /plan ...) as the web client does; ACP would send it to the model as text. */
  const command = async ({ sessionId, line }) => {
    if (typeof line !== 'string' || !line.startsWith('/')) throw new ControlError('command needs a line starting with /')
    const commands = ctx.get('commands')
    if (!commands) throw new ControlError('the runtime has no command registry')

    const agent = agentOf(sessionId)
    const execution = await commands.execute(agent, line, [], new AbortController().signal)
    if (!execution) throw new ControlError(`unknown command ${line.split(/\s/, 1)[0]}`)
    return { commandId: execution.commandId, kind: execution.result.kind, text: execution.result.text }
  }

  /** Enters or leaves plan mode; without active it only reports the state (a resumed session keeps its mode). */
  const planMode = ({ sessionId, active }) => {
    if (active !== undefined && typeof active !== 'boolean') throw new ControlError('planMode takes active as true or false')
    const controller = ctx.get('planMode')
    if (!controller) throw new ControlError('the runtime has no plan mode')
    const agent = agentOf(sessionId)
    const outcome = active === undefined ? 'query' : controller.set(agent, active)
    return { outcome, state: controller.get(agent) }
  }

  // Plan reviews waiting for the user: plan-mode asks through user-questions, which nothing answers under ACP. The
  // editor shows the plan (notification shuttlegent/planReview) and answers through answerPlanReview.
  const reviews = new Map()

  const answerPlanReview = ({ reviewId, approve, feedback }) => {
    const review = reviews.get(reviewId)
    if (!review) throw new ControlError(`plan review ${reviewId} is not waiting`)
    if (typeof approve !== 'boolean') throw new ControlError('answerPlanReview needs approve (true or false)')

    reviews.delete(reviewId)
    const question = review.question
    const answer = approve
      ? { id: question.id, selected: [question.intent.approve] }
      : { id: question.id, selected: [], ...(typeof feedback === 'string' && feedback.trim() ? { custom: feedback.trim() } : {}) }
    review.resolve({ answers: [answer] })
    return { reviewId, approved: approve }
  }

  ctx.on('user-questions/request', function (request, next) {
    const question = request?.questions?.length === 1 ? request.questions[0] : undefined
    const sessionId = request?.agent?.session?.header?.id ?? request?.agent?.id
    if (question?.intent?.kind !== 'plan-review' || !sessionId) return next()

    const reviewId = randomUUID()
    return new Promise((resolve, reject) => {
      reviews.set(reviewId, { question, resolve })
      request.signal?.addEventListener('abort', () => {
        if (!reviews.delete(reviewId)) return
        notify(REVIEW_CLOSED_METHOD, { sessionId, reviewId })
        reject(request.signal.reason)
      }, { once: true })
      notify(REVIEW_METHOD, { sessionId, reviewId, plan: question.detail ?? '' })
    })
  })

  /**
   * Whether a model route accepts images, as ACP checks it before a prompt with images. model is the ACP config value of
   * the session's "model" option (a JSON array [provider, model]); a route the catalog cannot resolve counts as no.
   */
  const imageInput = async ({ model }) => {
    let route
    try {
      route = JSON.parse(model)
    } catch {
      route = undefined
    }
    if (!Array.isArray(route) || typeof route[0] !== 'string' || typeof route[1] !== 'string') throw new ControlError('imageInput needs model as the config value ["provider","model"]')
    const llm = ctx.get('llm')
    if (!llm) throw new ControlError('the runtime has no model registry')

    try {
      const info = await llm.resolveModelInfo(route[0], route[1], new AbortController().signal)
      return { image: Array.isArray(info?.inputModalities) && info.inputModalities.includes('image') }
    } catch {
      return { image: false }
    }
  }

  /**
   * Names the agent of the next new session: ACP session/new carries no agent, so the editor calls this right before it
   * (shuttlegent-agent-dispatch keeps the choice for that one session).
   */
  const nextAgent = ({ agentId }) => {
    if (typeof agentId !== 'string' || !agentId) throw new ControlError('nextAgent needs agentId')
    const dispatch = ctx.get('shuttlegentAgentDispatch')
    if (!dispatch) throw new ControlError('the runtime has no agent dispatch')
    if (!dispatch.setNext(agentId)) throw new ControlError(`agent ${agentId} is not installed in this runtime`)
    return { agentId }
  }

  const METHODS = { fork, command, planMode, answerPlanReview, imageInput, nextAgent }

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
      const handler = Object.hasOwn(METHODS, message?.method) ? METHODS[message.method] : undefined
      if (!handler) {
        reply(response, 400, { error: `unknown method ${message?.method}` })
        return
      }
      try {
        reply(response, 200, { result: await handler(message.params ?? {}) })
      } catch (error) {
        reply(response, error instanceof ControlError ? 400 : 500, { error: error?.message ?? String(error) })
      }
    })
  })

  ctx.effect(() => {
    server.listen(0, '127.0.0.1', () => notify(ANNOUNCE_METHOD, { url: `http://127.0.0.1:${server.address().port}/` }))
    return () => server.close()
  }, 'shuttlegent-session-control server')
}
