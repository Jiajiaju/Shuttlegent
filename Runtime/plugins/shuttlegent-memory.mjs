// Shuttlegent memory: adds the user's personal prompt and memory, kept in the project's local directory
// Saved/Shuttlegent/ (never versioned), to every request as dynamic context. agent-instructions cannot load them:
// it ignores candidate names that contain a path separator. The text is read again whenever a file changes, so a
// memory the editor adds or removes reaches the model at the next request, in the same session too.
// Loaded by the patch the editor generates before every launch. It needs no editor endpoint; a missing file adds nothing.

import { readFileSync, statSync } from 'node:fs'
import { join } from 'node:path'

export const name = 'shuttlegent-memory'
export const inject = ['systemPrompt']

// Local directory under the session's working directory (the Unreal project)
const LOCAL_DIR = ['Saved', 'Shuttlegent']

const FILES = [
  { file: 'SHUTTLEGENT.local.md', heading: "The user's personal instructions (Saved/Shuttlegent/SHUTTLEGENT.local.md):" },
  { file: 'MEMORY.md', heading: 'What you remember about this user (Saved/Shuttlegent/MEMORY.md, kept with the remember and forget tools):' },
]

// Larger files are skipped rather than truncated; the editor keeps MEMORY.md far below this
const MAX_FILE_BYTES = 32768

// After the harness runtime contexts (working directory 100 ... web surface 210)
const CONTEXT_ORDER = 300

export function apply(ctx) {
  const cache = new Map()
  let warned = false

  const read = (path) => {
    let stat
    try {
      stat = statSync(path)
    } catch {
      cache.delete(path)
      return ''
    }
    if (!stat.isFile() || stat.size > MAX_FILE_BYTES) return ''

    const hit = cache.get(path)
    if (hit && hit.mtimeMs === stat.mtimeMs && hit.size === stat.size) return hit.text
    try {
      const text = readFileSync(path, 'utf8').replace(/^﻿/, '').trim()
      cache.set(path, { mtimeMs: stat.mtimeMs, size: stat.size, text })
      return text
    } catch (error) {
      if (!warned) console.warn(`[${name}] ${path} not read: ${error?.message ?? error}`)
      warned = true
      return ''
    }
  }

  ctx.systemPrompt.context({
    name: 'shuttlegent:memory',
    order: CONTEXT_ORDER,
    interpolate: false,
    required: true,
    text: (context) => {
      const cwd = context.agent?.session?.header?.cwd
      if (typeof cwd !== 'string' || !cwd) return ''

      const parts = []
      for (const { file, heading } of FILES) {
        const text = read(join(cwd, ...LOCAL_DIR, file))
        if (text) parts.push(`${heading}\n\n${text}`)
      }
      return parts.join('\n\n')
    },
  })
}
