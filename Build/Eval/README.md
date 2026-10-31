# Tool-choice evaluation

Designer and artist tasks sent to the real runtime and model, against a mock editor. Each task is scored on which editor tools the agent called and with which arguments; nothing runs in the editor. Run it after changing the system prompt, tool descriptions, skills or the model, and compare the pass count with the previous run.

## Files

| File | Purpose |
|---|---|
| `RunEval.ps1` | Entry point: starts the mock editor, runs every task as one ACP session (through `Build/Probe/acp_probe.py`), scores the run |
| `tasks.json` | The tasks and their checks |
| `world.json` | The fake editor state the mock answers from: a small level, assets, settings, log lines, engine toolsets |
| `tools.snapshot.json` | The editor's tools as `tools/list` sends them (names, descriptions, schemas, read or write), plus the MCP `instructions` text; reconstructed from the C++ sources |
| `eval_mock.py` | Mock editor MCP endpoint serving the snapshot and answering from `world.json`; each task gets its own copy of the world and write tools change it, so an agent that checks its own change sees it |
| `score.py` | Checks each task, prints PASS / FAIL per task, writes `report.md` and `summary.json` |

## Requirements

Same as `Build/Probe`: the built runtime (`Runtime/shuttlegent-runtime.exe`), the engine's bundled Python, and the DeepSeek key in **Editor Preferences > Shuttlegent > General**. The key reaches the runtime only through its environment.

## Usage

```powershell
Build\Eval\RunEval.ps1                                        # all tasks
Build\Eval\RunEval.ps1 -Only t01_light_intensity,t13_ambiguous   # some tasks
```

Output goes to `Saved/Eval/<timestamp>/` (git-ignored): one ACP log per task, the mock's call log, `report.md` and `summary.json`. Inspect one task with `Build/Probe/show_log.py`.

## Task checks

| Key | Meaning |
|---|---|
| `calls` | Tools that must be called |
| `callsAny` | At least one of these tools must be called |
| `forbid` | Tools that must not be called |
| `args` | Per tool, substrings one of its calls' arguments must contain (case-insensitive) |
| `argsAbsent` | Per tool, substrings none of its calls' arguments may contain |
| `maxWrites` | Most calls to write tools allowed |

Every task must also finish its turn (`end_turn`).

## Keeping the snapshot current

`tools.snapshot.json` must match the editor's tools. After adding a tool or changing a tool's description or parameters in `Source/ShuttlegentTools` or `Source/ShuttlegentToolsetBridge`, update the snapshot entry to the new text. `show_editor_ui` is left out: it only exists when the project configures UI guides.
