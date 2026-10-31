<#
.SYNOPSIS
Runs the Shuttlegent tool-choice evaluation: designer and artist tasks against a mock editor.

.DESCRIPTION
Starts eval_mock.py, which serves the editor's tools (tools.snapshot.json) and answers from a small fake level
(world.json), then sends every task of tasks.json to the runtime as one ACP session, through
Build/Probe/acp_probe.py. score.py checks which tools each task called and with which arguments, prints one line per
task and writes report.md and summary.json. Nothing runs in the editor; the model is real, so the run needs the
DeepSeek key of Editor Preferences > Shuttlegent > General, read from the project's
Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini and passed to the runtime only through its environment.

.PARAMETER Only
Run only the named tasks (e.g. -Only t01_light_intensity,t13_ambiguous).

.PARAMETER OutDir
Output directory. Defaults to <plugin>/Saved/Eval/<yyyyMMdd-HHmmss>.
#>
param(
    [string[]]$Only = @(),
    [string]$OutDir = '',
    [string]$ProjectDir = '',
    [string]$Python = '',
    [int]$Port = 27998
)

$ErrorActionPreference = 'Stop'

$PluginDir = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$EvalDir = $PSScriptRoot
$ProbeScript = Join-Path $PluginDir 'Build\Probe\acp_probe.py'
$PatchFile = Join-Path $PluginDir 'Runtime\profile\shuttlegent.patch.yml'
$RuntimeExe = Join-Path $PluginDir 'Runtime\shuttlegent-runtime.exe'
$Token = [Guid]::NewGuid().ToString('N')
$Utf8NoBom = New-Object System.Text.UTF8Encoding $false

if (-not (Test-Path $RuntimeExe)) { throw "Runtime not found: $RuntimeExe. Run Build/BuildRuntime.ps1 first." }
if (-not $OutDir) { $OutDir = Join-Path $PluginDir ('Saved\Eval\' + (Get-Date -Format 'yyyyMMdd-HHmmss')) }
if (-not $Python) {
    $SearchDir = $PluginDir
    while ($SearchDir) {
        $Candidate = Join-Path $SearchDir 'Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
        if (Test-Path $Candidate) { $Python = $Candidate; break }
        $SearchDir = Split-Path $SearchDir -Parent
    }
}
if (-not $Python) { throw 'Python 3 not found (no bundled engine Python above the plugin); pass -Python <path>.' }

# Model key from the editor preferences (UShuttlegentSettings::DeepSeekApiKey), kept in this process only
if (-not $ProjectDir) {
    $SearchDir = Split-Path $PluginDir -Parent
    while ($SearchDir) {
        if (Get-ChildItem -Path $SearchDir -Filter '*.uproject' -File -ErrorAction SilentlyContinue) { $ProjectDir = $SearchDir; break }
        $SearchDir = Split-Path $SearchDir -Parent
    }
}
$ApiKey = ''
$SettingsFile = Join-Path $ProjectDir 'Saved\Config\WindowsEditor\EditorPerProjectUserSettings.ini'
if (Test-Path $SettingsFile) {
    $InSection = $false
    foreach ($Line in Get-Content $SettingsFile) {
        if ($Line -match '^\s*\[(.+)\]\s*$') { $InSection = $Matches[1] -eq '/Script/ShuttlegentCore.ShuttlegentSettings'; continue }
        if ($InSection -and $Line -match '^DeepSeekApiKey=(.*)$') { $ApiKey = $Matches[1].Trim().Trim('"'); break }
    }
}
if (-not $ApiKey) { throw 'No DeepSeek key in Editor Preferences > Shuttlegent > General; the evaluation needs the real model.' }

$Workspace = Join-Path $OutDir 'ws'
New-Item -ItemType Directory -Force $Workspace | Out-Null
$HomeRoot = Join-Path $env:LOCALAPPDATA 'Shuttlegent\Eval'
if (Test-Path $HomeRoot) { Remove-Item -Recurse -Force $HomeRoot }

$Tasks = (Get-Content (Join-Path $EvalDir 'tasks.json') -Raw -Encoding UTF8 | ConvertFrom-Json).tasks
if ($Only.Count -gt 0) { $Tasks = $Tasks | Where-Object { $Only -contains $_.id } }
$Inherit = @('SystemRoot', 'SystemDrive', 'windir', 'PATH', 'PATHEXT', 'COMSPEC', 'TEMP', 'TMP', 'USERPROFILE', 'APPDATA', 'LOCALAPPDATA', 'HOMEDRIVE', 'HOMEPATH', 'USERNAME', 'COMPUTERNAME', 'DEEPSEEK_API_KEY')

$MockLog = Join-Path $OutDir 'eval_mock.log'
$Mock = Start-Process -FilePath $Python -ArgumentList @('-I', (Join-Path $EvalDir 'eval_mock.py'), $Port, $Token, $MockLog) -WindowStyle Hidden -PassThru
try {
    $Deadline = (Get-Date).AddSeconds(10)
    while (-not (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue)) {
        if ((Get-Date) -gt $Deadline) { throw "Mock editor endpoint did not start on port $Port." }
        Start-Sleep -Milliseconds 200
    }

    $env:DEEPSEEK_API_KEY = $ApiKey
    foreach ($Task in $Tasks) {
        # A fresh DSH_HOME per task, launched from it as the editor launches the runtime
        $DshHome = Join-Path $HomeRoot $Task.id
        New-Item -ItemType Directory -Force $DshHome | Out-Null
        $Scenario = [ordered]@{
            name = $Task.id
            command = @($RuntimeExe, '--profile', 'acp', '--patch', $PatchFile)
            cwd = $Workspace
            process_cwd = $DshHome
            env = @{ DSH_HOME = $DshHome; DSH_AGENTS_HOME = (Join-Path $DshHome 'agents'); DSH_TELEMETRY_DISABLED = '1'; DSH_PERMISSION_MODE = 'read-only' }
            inherit_env = $Inherit
            log = (Join-Path $OutDir "$($Task.id).jsonl")
            mcp = @(@{ name = 'ue'; url = "http://127.0.0.1:$Port/mcp"; token = $Token })
            prompts = @($Task.prompt)
            permission = 'reject'
            timeout = 240
            list_sessions = $false
        }
        $ScenarioFile = Join-Path $OutDir "$($Task.id).json"
        [IO.File]::WriteAllText($ScenarioFile, ($Scenario | ConvertTo-Json -Depth 6), $Utf8NoBom)
        Write-Host "== $($Task.id)"
        & $Python -I $ProbeScript $ScenarioFile
    }
}
finally {
    Remove-Item Env:DEEPSEEK_API_KEY -ErrorAction SilentlyContinue
    if ($Mock -and -not $Mock.HasExited) { Stop-Process -Id $Mock.Id -Force }
}

Write-Host ''
& $Python -I (Join-Path $EvalDir 'score.py') $OutDir
Write-Host "Report: $(Join-Path $OutDir 'report.md')"
exit $LASTEXITCODE
