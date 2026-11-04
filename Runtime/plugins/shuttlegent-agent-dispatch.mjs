// Shuttlegent agent dispatch: holds the runtime's single agent factory slot and hands each session to the loop of its
// agent, so one runtime can host several agent loops. The patch the editor
// generates disables the agent-loop row and loads this plugin instead; the default loop (dsh-agent-loop) and every
// installed third-party loop run below it, each in a scope whose `agents` is a stand-in that records the factory the
// loop registers instead of taking the slot.
// A new session takes the agent the editor named through session control (nextAgent, good for the next session only),
// else the default agent; the choice is written to the session header (agentPreset) and read back on resume. A session
// whose agent is not installed is refused, never silently run by the default loop.
// Config (from the generated patch): { agents: [{ id, entry }] } with entry the absolute path of a loop module.

export const name = 'shuttlegent-agent-dispatch'
export const inject = ['agents', 'sessionPersistence']

// The service session control reaches this plugin through
const SERVICE = 'shuttlegentAgentDispatch'

const DEFAULT_AGENT = 'default'
const DEFAULT_LOOP = '@deepseek-ai/dsh-agent-loop'
const PRESET_PREFIX = 'shuttlegent:'

// How long a loop may take to register its factory after the runtime starts, and how long a nextAgent choice waits for
// its session/new, in milliseconds
const READY_TIMEOUT_MS = 10000
const NEXT_AGENT_TTL_MS = 30000

export async function apply(ctx, config) {
  const loops = new Map()
  const sessionAgents = new Map()
  let next

  /** The agent a session header names; sessions from before the dispatch carry none and belong to the default agent. */
  const agentOfHeader = (header) => {
    const preset = header?.agentPreset
    return typeof preset === 'string' && preset.startsWith(PRESET_PREFIX) ? preset.slice(PRESET_PREFIX.length) : DEFAULT_AGENT
  }

  const factoryOf = async (agentId) => {
    const loop = loops.get(agentId)
    if (!loop) throw new Error(`[${name}] agent "${agentId}" is not installed in this runtime`)
    return loop.whenReady()
  }

  /** The agent of a new session: a child keeps its parent's, a top-level session takes the editor's choice once. */
  const agentForNew = (options) => {
    if (options.parentAgent) {
      const parentId = options.parentAgent.session?.header?.id ?? options.parentAgent.id
      return sessionAgents.get(String(parentId)) ?? DEFAULT_AGENT
    }
    const chosen = next !== undefined && Date.now() - next.at <= NEXT_AGENT_TTL_MS ? next.agentId : undefined
    next = undefined
    return chosen ?? DEFAULT_AGENT
  }

  ctx.agents.setFactory({
    async createAgent(owner, options) {
      const agentId = agentForNew(options)
      const factory = await factoryOf(agentId)
      const meta = options.meta?.agentPreset === undefined ? { ...options.meta, agentPreset: PRESET_PREFIX + agentId } : options.meta
      const handle = await factory.createAgent(owner, { ...options, meta })
      sessionAgents.set(String(options.sessionId), agentId)
      return handle
    },
    async resume(owner, options) {
      const snapshot = await ctx.sessionPersistence.stat(options.resumeSessionId)
      const agentId = agentOfHeader(snapshot?.header)
      const factory = await factoryOf(agentId)
      const handle = await factory.resume(owner, options)
      sessionAgents.set(String(options.resumeSessionId), agentId)
      return handle
    },
  })

  ctx.provide(SERVICE, {
    /** Names the agent of the next new session; false when that agent is not installed. */
    setNext(agentId) {
      if (!loops.has(agentId)) return false
      next = { agentId, at: Date.now() }
      return true
    },
    agents: () => [...loops.keys()],
  })

  // Each loop runs in a scope of its own `agents`: a stand-in provided by a child plugin of that scope (Cordis tells
  // dependents a service is ready by the provider's scope) over a plain object (forwarding Cordis' symbol keys would
  // let the trace layer unwrap it back to the real registry); everything but setFactory goes to the real registry.
  const real = ctx.agents
  const mount = (agentId, loopPlugin, loopConfig) => {
    let factory
    let markReady
    const ready = new Promise((resolve) => { markReady = resolve })
    const standIn = new Proxy({}, {
      get(_, prop) {
        if (typeof prop === 'symbol') return undefined
        if (prop === 'setFactory') {
          return (registered) => {
            factory = registered
            markReady()
            return () => { factory = undefined }
          }
        }
        const value = Reflect.get(real, prop)
        return typeof value === 'function' ? value.bind(real) : value
      },
    })

    const scope = ctx.isolate('agents')
    scope.plugin({ name: `${name}:${agentId}:agents`, apply: (scoped) => { scoped.provide('agents', standIn) } })
    scope.plugin(loopPlugin, loopConfig)
    loops.set(agentId, {
      async whenReady() {
        if (!factory) await Promise.race([ready, new Promise((resolve) => setTimeout(resolve, READY_TIMEOUT_MS))])
        if (!factory) throw new Error(`[${name}] agent "${agentId}" did not start`)
        return factory
      },
    })
  }

  const tree = ctx.fiber.entry.parent.tree
  const defaultLoop = await tree.import(DEFAULT_LOOP)
  mount(DEFAULT_AGENT, defaultLoop.default ?? defaultLoop.AgentLoop, { agents: [] })

  for (const agent of config?.agents ?? []) {
    if (typeof agent?.id !== 'string' || typeof agent?.entry !== 'string' || agent.id === DEFAULT_AGENT) continue
    try {
      const loop = await tree.import(agent.entry)
      const plugin = loop.default ?? loop
      if (typeof plugin !== 'function' && typeof plugin?.apply !== 'function') throw new Error('the module exports no plugin')
      mount(agent.id, plugin, agent.config ?? {})
    } catch (error) {
      console.warn(`[${name}] agent ${agent.id} not loaded from ${agent.entry}: ${error?.message ?? error}`)
    }
  }
}
