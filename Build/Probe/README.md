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
| `fixtures/split-red-blue.png` | Image for the image-input scenarios: left half red, right half blue |
| `fixtures/event-tap.mjs` | Harness plugin for `s12_plugin`: writes the session events it receives to `SHUTTLEGENT_EVENT_TAP_LOG` |

## Requirements

- Runtime built by `Build/BuildRuntime.ps1` (or `-Runtime node` with the submodule built).
- Python 3 (standard library only). `RunRegression.ps1` uses, in order: `-Python`, the engine's bundled `Engine/Binaries/ThirdParty/Python3/Win64/python.exe` found by walking up from the plugin, then `python` on PATH. Avoid a Python Install Manager build: it cannot see the launch directories created under `%LOCALAPPDATA%`, and the runtime fails to start with WinError 267.
- For live-model scenarios: the DeepSeek key set in **Editor Preferences > Shuttlegent > General** (the project's `Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini`). The project is the first directory above the plugin with a `.uproject`; pass `-ProjectDir` to use another. The key is passed to the runtime through its environment only and is never written to scenario files or logs.

## Usage

```powershell
Build\Probe\RunRegression.ps1                        # all scenarios, launched as the editor does
Build\Probe\RunRegression.ps1 -SkipLiveModel         # only scenarios that need no valid key
Build\Probe\RunRegression.ps1 -Only s11_image,s13_image_flash   # only the named scenarios
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
| `s10_parallel` | Two sessions of one runtime prompted at once both finish |
| `s11_image` | Image input on the default model: read by the model, or declined at `initialize` (`promptCapabilities.image`) |
| `s12_plugin` | A plugin inserted by a patch from an absolute path loads in the packed runtime and receives session events |
| `s13_image_flash` | Image input on `deepseek-flash` |
