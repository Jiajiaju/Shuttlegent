<#
.SYNOPSIS
Runs the Shuttlegent runtime regression suite (the M0 scenarios) and checks the results.

.DESCRIPTION
Starts a mock editor MCP endpoint, drives the dsh runtime over ACP through a set of
scenarios, then evaluates the logs with check_results.py. Scenarios that call a real
model need the DeepSeek key the editor uses (Editor Preferences > Shuttlegent > General,
saved in the project's Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini);
without it they are skipped. The key reaches the runtime only through the child process
environment and is never written to scenario files or logs.

.PARAMETER Runtime
exe: Runtime/shuttlegent-runtime.exe (default). node: the built dsh CLI in ThirdParty.

.PARAMETER Python
Python 3 interpreter. Defaults to the engine's bundled Python
(Engine/Binaries/ThirdParty/Python3/Win64/python.exe, found by walking up from the
plugin directory), then to `python` on PATH. A Python Install Manager build cannot see
the launch directories this script creates under %LOCALAPPDATA%, so starting the runtime
fails with WinError 267 (observed 2026-10-23); pass -Python to override.

.PARAMETER OutDir
Output directory for logs and the test workspace. Defaults to <plugin>/Saved/Probe.

.PARAMETER ProjectDir
Project whose editor preferences hold the model key. Defaults to the first directory
above the plugin that contains a .uproject file.

.PARAMETER Only
Run only the named scenarios (e.g. -Only s11_image,s13_image_flash); the others are reported as SKIP.
s07_resume needs s05_tools in the same run.

.PARAMETER SkipLiveModel
Skip scenarios that need a valid model key.

.PARAMETER LaunchFromWorkspace
By default the runtime is launched, as the editor does, from a private DSH_HOME under
%LOCALAPPDATA%\Shuttlegent\Probe while the ACP session cwd is the workspace; the
workspace .env is then expected not to be read. This switch launches it from the
workspace instead and keeps DSH_HOME under OutDir. With the exe runtime that only works
when OutDir is on the system drive: the packed runtime fails to load its native koffi
addon when its launch directory is on another drive.
#>
param(
    [ValidateSet('exe', 'node')]
    [string]$Runtime = 'exe',
    [string]$Python = '',
    [string]$OutDir = '',
    [string]$ProjectDir = '',
    [int]$Port = 27999,
    [string[]]$Only = @(),
    [switch]$SkipLiveModel,
    [switch]$LaunchFromWorkspace
)

$ErrorActionPreference = 'Stop'

$PluginDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$ProbeDir = $PSScriptRoot
$DshDir = Join-Path $PluginDir 'ThirdParty\deepseek-harness'
$PatchFile = Join-Path $PluginDir 'Runtime\profile\shuttlegent.patch.yml'
$GatewayOverlay = Join-Path $ProbeDir 'gateway.overlay.yml'
$Token = [Guid]::NewGuid().ToString('N')
$Utf8NoBom = New-Object System.Text.UTF8Encoding $false

if (-not $OutDir) { $OutDir = Join-Path $PluginDir 'Saved\Probe' }
if (-not $Python) {
    $EnginePython = 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
    $SearchDir = $PluginDir
    while ($SearchDir) {
        $Candidate = Join-Path $SearchDir $EnginePython
        if (Test-Path $Candidate) { $Python = $Candidate; break }
        $SearchDir = Split-Path $SearchDir -Parent
    }
}
if (-not $Python) {
    $PythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if (-not $PythonCommand) { throw 'Python 3 not found (no bundled engine Python above the plugin, none on PATH); pass -Python <path>.' }
    $Python = $PythonCommand.Source
}
Write-Host "Using Python: $Python"

if ($Runtime -eq 'exe') {
    $RuntimeExe = Join-Path $PluginDir 'Runtime\shuttlegent-runtime.exe'
    if (-not (Test-Path $RuntimeExe)) { throw "Runtime not found: $RuntimeExe. Run Build/BuildRuntime.ps1 first." }
    $BaseCommand = @($RuntimeExe)
}
else {
    $NodeCommand = Get-Command node -ErrorAction SilentlyContinue
    if (-not $NodeCommand) { throw 'node not found on PATH.' }
    $BaseCommand = @($NodeCommand.Source, (Join-Path $DshDir 'apps\cli\lib\bin.js'))
}

# Fresh output directory and a workspace with marker files.
if (Test-Path $OutDir) { Remove-Item -Recurse -Force $OutDir }
$Workspace = Join-Path $OutDir 'ws'
New-Item -ItemType Directory -Force (Join-Path $Workspace '.git'), (Join-Path $Workspace '.agents\skills\m0-agents-skill'), (Join-Path $Workspace 'Docs') | Out-Null
[IO.File]::WriteAllText((Join-Path $Workspace 'AGENTS.md'), "# Dev rules`nMARKER_AGENTS_MD: if you can read this line, say the word PINEAPPLE.`n", $Utf8NoBom)
[IO.File]::WriteAllText((Join-Path $Workspace 'SHUTTLEGENT.md'), "# Project prompt`nMARKER_SHUTTLEGENT_MD: if you can read this line, say the word MANGO.`n", $Utf8NoBom)
[IO.File]::WriteAllText((Join-Path $Workspace '.agents\skills\m0-agents-skill\SKILL.md'), "---`nname: m0-agents-skill`ndescription: MARKER_AGENTS_SKILL probe skill`n---`nProbe.`n", $Utf8NoBom)
[IO.File]::WriteAllText((Join-Path $Workspace 'Docs\Readme.txt'), "Probe doc for read tool.`n", $Utf8NoBom)
$SplitLaunchDir = -not $LaunchFromWorkspace
$HomeRoot = $OutDir
if ($SplitLaunchDir) {
    $HomeRoot = Join-Path $env:LOCALAPPDATA 'Shuttlegent\Probe'
    if (Test-Path $HomeRoot) { Remove-Item -Recurse -Force $HomeRoot }
}
$Mode = [ordered]@{ runtime = $Runtime; splitLaunchDir = $SplitLaunchDir }
[IO.File]::WriteAllText((Join-Path $OutDir 'mode.json'), ($Mode | ConvertTo-Json), $Utf8NoBom)

# Model key from the editor preferences (UShuttlegentSettings::DeepSeekApiKey), kept in this process only.
$ApiKey = ''
if (-not $SkipLiveModel) {
    if (-not $ProjectDir) {
        $SearchDir = Split-Path $PluginDir -Parent
        while ($SearchDir) {
            if (Get-ChildItem -Path $SearchDir -Filter '*.uproject' -File -ErrorAction SilentlyContinue) { $ProjectDir = $SearchDir; break }
            $SearchDir = Split-Path $SearchDir -Parent
        }
    }
    $SettingsFile = ''
    if ($ProjectDir) { $SettingsFile = Join-Path $ProjectDir 'Saved\Config\WindowsEditor\EditorPerProjectUserSettings.ini' }
    if ($SettingsFile -and (Test-Path $SettingsFile)) {
        $InSection = $false
        foreach ($Line in Get-Content $SettingsFile) {
            if ($Line -match '^\s*\[(.+)\]\s*$') { $InSection = $Matches[1] -eq '/Script/ShuttlegentCore.ShuttlegentSettings'; continue }
            if ($InSection -and $Line -match '^DeepSeekApiKey=(.*)$') { $ApiKey = $Matches[1].Trim().Trim('"'); break }
        }
    }
    if ($ApiKey) { Write-Host "Model key: DeepSeekApiKey from $SettingsFile" }
}
$LiveModel = [bool]$ApiKey
if (-not $LiveModel) { Write-Host 'No DeepSeek key in Editor Preferences > Shuttlegent > General (or -SkipLiveModel): live-model scenarios are skipped.' }

$BaseInherit = @('SystemRoot', 'SystemDrive', 'windir', 'PATH', 'PATHEXT', 'COMSPEC', 'TEMP', 'TMP', 'USERPROFILE', 'APPDATA', 'LOCALAPPDATA', 'HOMEDRIVE', 'HOMEPATH', 'USERNAME', 'COMPUTERNAME')
Remove-Item Env:DEEPSEEK_API_KEY -ErrorAction SilentlyContinue

function Invoke-Scenario {
    param(
        [string]$Name,
        [string]$HomeName,
        [object[]]$Prompts = @(),
        [string]$Permission = 'reject',
        [double]$CancelAfter = -1,
        [string]$Resume = '',
        [string]$Key = '',
        [string[]]$ExtraPatches = @(),
        [string[]]$ParallelPrompts = @(),
        [hashtable]$ExtraEnv = @{}
    )

    if ($Only.Count -gt 0 -and $Only -notcontains $Name) { return }

    $Command = $BaseCommand + @('--profile', 'acp', '--patch', $PatchFile)
    foreach ($Extra in $ExtraPatches) { $Command += @('--patch', $Extra) }

    $Inherit = $BaseInherit
    if ($Key) {
        $env:DEEPSEEK_API_KEY = $Key
        $Inherit += 'DEEPSEEK_API_KEY'
    }

    $DshHome = Join-Path $HomeRoot "home-$HomeName"
    $Scenario = [ordered]@{
        name = $Name
        command = $Command
        cwd = $Workspace
        env = @{ DSH_HOME = $DshHome; DSH_AGENTS_HOME = (Join-Path $DshHome 'agents'); DSH_TELEMETRY_DISABLED = '1'; DSH_PERMISSION_MODE = 'read-only' }
        inherit_env = $Inherit
        log = (Join-Path $OutDir "$Name.jsonl")
        mcp = @(@{ name = 'ue'; url = "http://127.0.0.1:$Port/mcp"; token = $Token })
        prompts = $Prompts
        permission = $Permission
        timeout = 240
    }
    foreach ($EnvName in $ExtraEnv.Keys) { $Scenario.env[$EnvName] = $ExtraEnv[$EnvName] }
    if ($CancelAfter -ge 0) { $Scenario.cancel_after = $CancelAfter }
    if ($Resume) { $Scenario.resume = $Resume }
    if ($ParallelPrompts.Count -gt 0) { $Scenario.parallel_prompts = $ParallelPrompts }
    if ($SplitLaunchDir) {
        New-Item -ItemType Directory -Force $DshHome | Out-Null
        $Scenario.process_cwd = $DshHome
    }

    $ScenarioFile = Join-Path $OutDir "$Name.json"
    [IO.File]::WriteAllText($ScenarioFile, ($Scenario | ConvertTo-Json -Depth 6), $Utf8NoBom)
    Write-Host "== $Name"
    & $Python -I (Join-Path $ProbeDir 'acp_probe.py') $ScenarioFile
    Remove-Item Env:DEEPSEEK_API_KEY -ErrorAction SilentlyContinue
}

function Get-SessionId {
    param([string]$Name)

    $LogFile = Join-Path $OutDir "$Name.jsonl"
    if (-not (Test-Path $LogFile)) { return '' }
    $Line = Get-Content $LogFile | Where-Object { $_ -match '"kind": "session"' } | Select-Object -First 1
    if (-not $Line) { return '' }
    return ($Line | ConvertFrom-Json).data.sessionId
}

$MockLog = Join-Path $OutDir 'mcp_mock.log'
$Mock = Start-Process -FilePath $Python -ArgumentList @('-I', (Join-Path $ProbeDir 'mcp_mock.py'), $Port, $Token, $MockLog) -WindowStyle Hidden -PassThru
try {
    $Deadline = (Get-Date).AddSeconds(10)
    while (-not (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue)) {
        if ((Get-Date) -gt $Deadline) { throw "Mock MCP endpoint did not start on port $Port." }
        Start-Sleep -Milliseconds 200
    }

    Invoke-Scenario -Name 's01_init' -HomeName 'init'
    Invoke-Scenario -Name 's02_nokey' -HomeName 'nokey' -Prompts @('Say hello in one word.')
    Invoke-Scenario -Name 's03_envkey' -HomeName 'envkey' -Prompts @('Say hello in one word.') -Key 'sk-probe-invalid-from-env'

    [IO.File]::WriteAllText((Join-Path $Workspace '.env'), "DEEPSEEK_API_KEY=sk-probe-invalid-from-dotenv`n", $Utf8NoBom)
    Invoke-Scenario -Name 's04_dotenv' -HomeName 'dotenv' -Prompts @('Say hello in one word.')
    Remove-Item (Join-Path $Workspace '.env')

    if ($LiveModel) {
        $ToolsPrompt = 'Do these steps in order. 1) Call the ue_read_probe tool. 2) Call the ue_write_probe tool with actor "Cube" and newName "Cube2". 3) Tell me whether your instructions contain any special marker word, and quote it.'
        Invoke-Scenario -Name 's05_tools' -HomeName 'tools' -Prompts @($ToolsPrompt) -Key $ApiKey

        $SandboxPrompt = 'Do these steps. 1) Use your write tool to create the file probe_write.txt in the workspace root with the content hello. 2) Use your edit tool to append a line to Docs/Readme.txt. 3) Read Docs/Readme.txt and quote its content. Report what succeeded and what failed.'
        Invoke-Scenario -Name 's06_sandbox' -HomeName 'sandbox' -Prompts @($SandboxPrompt) -Permission 'reject' -Key $ApiKey

        $ToolsSession = Get-SessionId 's05_tools'
        Invoke-Scenario -Name 's07_resume' -HomeName 'tools' -Resume $ToolsSession -Prompts @('Without calling any tool: which tool did you call first earlier in this conversation, and what exact text did it return?') -Key $ApiKey

        Invoke-Scenario -Name 's08_cancel' -HomeName 'cancel' -Prompts @('Write the numbers from 1 to 400 in English words, one per line, without using any tool.', 'Without any tool, reply with just the word OK.') -CancelAfter 4 -Key $ApiKey

        Invoke-Scenario -Name 's09_gateway' -HomeName 'gateway' -Prompts @('Call the ue_read_probe tool, then tell me its result in one sentence.') -Key $ApiKey -ExtraPatches @($GatewayOverlay)

        # M3.7 precondition: two sessions of one runtime prompted at the same time (Shuttlegent.md section 14.3.7)
        $LongPrompt = 'Without using any tool, write the numbers from 1 to 120 in English words, one per line.'
        Invoke-Scenario -Name 's10_parallel' -HomeName 'parallel' -ParallelPrompts @($LongPrompt, $LongPrompt) -Key $ApiKey

        # H6: does the default model route accept and read an image (left half red, right half blue)
        $ImagePrompt = @{ text = 'Without using any tool: what colour is the left half of this image, and what colour is the right half? Answer in English in one short sentence.'; image = (Join-Path $ProbeDir 'fixtures\split-red-blue.png') }
        Invoke-Scenario -Name 's11_image' -HomeName 'image' -Prompts @($ImagePrompt) -Key $ApiKey

        # H6: the same image on deepseek-flash, which the llm-deepseek catalog lists with image input
        $FlashOverlay = Join-Path $OutDir 'image-flash.overlay.yml'
        [IO.File]::WriteAllText($FlashOverlay, "- id: acp`n  config:`n    provider: deepseek-official`n    model: deepseek-flash`n", $Utf8NoBom)
        Invoke-Scenario -Name 's13_image_flash' -HomeName 'imageflash' -Prompts @($ImagePrompt) -Key $ApiKey -ExtraPatches @($FlashOverlay)

        # H1: a plugin loaded from an absolute path by a patch, without changing Harness sources, receives session events
        $TapPlugin = (Join-Path $ProbeDir 'fixtures\event-tap.mjs').Replace('\', '/')
        $TapOverlay = Join-Path $OutDir 'event-tap.overlay.yml'
        [IO.File]::WriteAllText($TapOverlay, "- insert:`n    - id: shuttlegent-event-tap`n      name: '$TapPlugin'`n", $Utf8NoBom)
        Invoke-Scenario -Name 's12_plugin' -HomeName 'plugin' -Prompts @('Call the ue_read_probe tool, then tell me its result in one sentence.') -Key $ApiKey -ExtraPatches @($TapOverlay) -ExtraEnv @{ SHUTTLEGENT_EVENT_TAP_LOG = (Join-Path $OutDir 's12_plugin.events.jsonl') }

        # Context tap: the shipped plugin posts each session's context breakdown to the editor endpoint (here the mock)
        $ContextTap = (Join-Path $PluginDir 'Runtime\plugins\shuttlegent-context-tap.mjs').Replace('\', '/')
        $ContextOverlay = Join-Path $OutDir 'context-tap.overlay.yml'
        [IO.File]::WriteAllText($ContextOverlay, "- insert:`n    - id: shuttlegent-context-tap`n      name: '$ContextTap'`n", $Utf8NoBom)
        Invoke-Scenario -Name 's14_context' -HomeName 'context' -Prompts @('Say hello in one word.') -Key $ApiKey -ExtraPatches @($ContextOverlay) -ExtraEnv @{ SHUTTLEGENT_EDITOR_ENDPOINT = "http://127.0.0.1:$Port/mcp"; SHUTTLEGENT_EDITOR_TOKEN = $Token }
    }
}
finally {
    if ($Mock -and -not $Mock.HasExited) { Stop-Process -Id $Mock.Id -Force }
    Remove-Item Env:DEEPSEEK_API_KEY -ErrorAction SilentlyContinue
}

Write-Host ''
& $Python -I (Join-Path $ProbeDir 'check_results.py') $OutDir
exit $LASTEXITCODE
