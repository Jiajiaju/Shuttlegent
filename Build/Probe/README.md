# Runtime regression probe

Scripts that drive the Shuttlegent agent runtime (DeepSeek Harness over ACP) without the editor, against a mock editor MCP endpoint. Use them after building the runtime and whenever the DeepSeek Harness submodule is upgraded.

## Files

| File | Purpose |
|---|---|
| `RunRegression.ps1` | Entry point: prepares a workspace, starts the mock MCP endpoint, runs every scenario, checks the results |
| `acp_probe.py` | ACP client: spawns the runtime, runs one scenario, logs every protocol message as JSON lines |
| `mcp_mock.py` | Mock editor MCP endpoint (Streamable HTTP, bearer token, one read tool and one write tool) |
| `check_results.py` | Evaluates the logs and prints PASS / FAIL / SKIP per scenario |
| `show_log.py` | Prints one scenario log as a timeline, or only the assistant's messages with `--messages` |
| `gateway.overlay.yml` | Extra patch that routes the model through the OpenAI-compatible protocol |

## Requirements

- Runtime built by `Build/BuildRuntime.ps1` (or `-Runtime node` with the submodule built).
- Python 3 (standard library only).
- For live-model scenarios: `DEEPSEEK_API_KEY=...` in the plugin's `.env` (git-ignored). The key is passed to the runtime through its environment only and is never written to scenario files or logs.

## Usage

```powershell
Build\Probe\RunRegression.ps1                        # all scenarios, launched as the editor does
Build\Probe\RunRegression.ps1 -SkipLiveModel         # only scenarios that need no valid key
Build\Probe\RunRegression.ps1 -LaunchFromWorkspace -OutDir $env:TEMP\ShuttlegentProbe   # launch from the workspace (Q5 comparison)
```

By default the runtime is launched from a private `DSH_HOME` under `%LOCALAPPDATA%\Shuttlegent\Probe` while the ACP session `cwd` is the test workspace, exactly as the editor launches it. Logs and the test workspace go to `Saved/Probe/` (git-ignored).

The packed runtime fails to load its native `koffi` addon (used for session persistence on Windows) when its launch directory is on a drive other than the system drive. `-LaunchFromWorkspace` therefore needs an `OutDir` on the system drive when testing the exe.

Inspect one scenario with:

```powershell
python -I Build\Probe\show_log.py Saved\Probe\s05_tools.jsonl
```

## Scenarios

| Scenario | Checks |
|---|---|
| `s01_init` | `initialize`, `session/new`, MCP handshake with the bearer token |
| `s02_nokey` | No key: explicit missing-key error |
| `s03_envkey` | Invalid key from the environment reaches DeepSeek (authentication error) |
| `s04_dotenv` | Workspace `.env` ignored by default; read with `-LaunchFromWorkspace` |
| `s05_tools` | Model calls the read and write MCP tools; no permission prompt; `AGENTS.md` and `.agents/skills` not loaded |
| `s06_sandbox` | Read-only sandbox: file writes blocked, escalation requests rejected, reads still work |
| `s07_resume` | `session/resume` restores context |
| `s08_cancel` | `session/cancel` ends the turn; the session keeps working |
| `s09_gateway` | Model routed through `llm-pi-ai` with the `openai-completions` protocol |
