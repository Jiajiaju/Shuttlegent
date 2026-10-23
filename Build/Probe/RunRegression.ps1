<#
.SYNOPSIS
Runs the Shuttlegent runtime regression suite (the M0 scenarios) and checks the results.

.DESCRIPTION
Starts a mock editor MCP endpoint, drives the dsh runtime over ACP through a set of
scenarios, then evaluates the logs with check_results.py. Scenarios that call a real
model need DEEPSEEK_API_KEY in the plugin's .env file; without it they are skipped.
The key reaches the runtime only through the child process environment and is never
written to scenario files or logs.

.PARAMETER Runtime
exe: Runtime/shuttlegent-runtime.exe (default). node: the built dsh CLI in ThirdParty.

.PARAMETER Python
Python 3 interpreter. Defaults to `python` on PATH.

.PARAMETER OutDir
Output directory for logs and the test workspace. Defaults to <plugin>/Saved/Probe.

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
    [int]$Port = 27999,
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
    $PythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if (-not $PythonCommand) { throw 'Python 3 not found on PATH; pass -Python <path>.' }
    $Python = $PythonCommand.Source
}

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

# Model key from the plugin .env, kept in this process only.
$ApiKey = ''
$EnvFile = Join-Path $PluginDir '.env'
if (-not $SkipLiveModel -and (Test-Path $EnvFile)) {
    $KeyLine = Get-Content $EnvFile | Where-Object { $_ -match '^DEEPSEEK_API_KEY=.+' } | Select-Object -First 1
    if ($KeyLine) { $ApiKey = $KeyLine.Substring('DEEPSEEK_API_KEY='.Length).Trim() }
}
$LiveModel = [bool]$ApiKey
if (-not $LiveModel) { Write-Host 'No DEEPSEEK_API_KEY in plugin .env (or -SkipLiveModel): live-model scenarios are skipped.' }

$BaseInherit = @('SystemRoot', 'SystemDrive', 'windir', 'PATH', 'PATHEXT', 'COMSPEC', 'TEMP', 'TMP', 'USERPROFILE', 'APPDATA', 'LOCALAPPDATA', 'HOMEDRIVE', 'HOMEPATH', 'USERNAME', 'COMPUTERNAME')
Remove-Item Env:DEEPSEEK_API_KEY -ErrorAction SilentlyContinue

function Invoke-Scenario {
    param(
        [string]$Name,
        [string]$HomeName,
        [string[]]$Prompts = @(),
        [string]$Permission = 'reject',
        [double]$CancelAfter = -1,
        [string]$Resume = '',
        [string]$Key = '',
        [string[]]$ExtraPatches = @()
    )

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
    if ($CancelAfter -ge 0) { $Scenario.cancel_after = $CancelAfter }
    if ($Resume) { $Scenario.resume = $Resume }
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

    $Line = Get-Content (Join-Path $OutDir "$Name.jsonl") | Where-Object { $_ -match '"kind": "session"' } | Select-Object -First 1
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
    }
}
finally {
    if ($Mock -and -not $Mock.HasExited) { Stop-Process -Id $Mock.Id -Force }
    Remove-Item Env:DEEPSEEK_API_KEY -ErrorAction SilentlyContinue
}

Write-Host ''
& $Python -I (Join-Path $ProbeDir 'check_results.py') $OutDir
exit $LASTEXITCODE
