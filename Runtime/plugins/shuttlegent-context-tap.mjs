// Shuttlegent context tap: after each assistant message (when the ACP adapter sends usage_update), reads the session's
// context breakdown from the token meter's contextBreakdown projection (system prompts, tool definitions, messages; a
// heuristic estimate) and sends it to the editor endpoint as the JSON-RPC notification shuttlegent/contextBreakdown.
// Loaded by the patch the editor generates before every launch; the endpoint URL and token come from the environment.
// A failure only logs once: the conversation never depends on this plugin.

export const name = 'shuttlegent-context-tap'

const METHOD = 'shuttlegent/contextBreakdown'

export function apply(ctx) {
  const url = process.env.SHUTTLEGENT_EDITOR_ENDPOINT
  const token = process.env.SHUTTLEGENT_EDITOR_TOKEN
  if (!url || !token) return

  let warned = false
  const warn = (error) => {
    if (warned) return
    warned = true
    console.warn(`[${name}] context breakdown not sent: ${error?.message ?? error}`)
  }

  ctx.on('session/event', (session, event) => {
    if (event?.type !== 'assistant/message') return

    let breakdown
    try {
      breakdown = ctx.get('sessionProjections')?.stateOf(session, 'contextBreakdown')?.breakdown
    } catch (error) {
      warn(error)
      return
    }
    if (!breakdown) return

    const body = JSON.stringify({
      jsonrpc: '2.0',
      method: METHOD,
      params: {
        sessionId: session.header.id,
        systemTokens: breakdown.systemTokens,
        toolsTokens: breakdown.toolsTokens,
        messageTokens: breakdown.messageTokens,
      },
    })
    fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${token}` },
      body,
    }).catch(warn)
  })
}
