# Exports the Shuttlegent mark animation sprite sheets into Resources/anim.
# Requires Node 22+ (built-in WebSocket) and Google Chrome or Microsoft Edge.
#
# Usage: .\Build\MarkAnim\ExportMarkAnim.ps1 [-Palette <name>] [-Chrome <path to chrome.exe or msedge.exe>]
# -Palette picks Resources/svg/mark-<name>.svg for colours; "reverse" (default) writes to Resources/anim,
# other palettes to Resources/anim/<name>.

param(
    [string]$Palette = "reverse",
    [string]$Chrome
)

$ErrorActionPreference = "Stop"
$PluginRoot = Resolve-Path (Join-Path $PSScriptRoot "..\..")

$node = Get-Command node -ErrorAction SilentlyContinue
if (-not $node) { throw "Node.js was not found on PATH. Install Node 22 or newer." }
$nodeMajor = [int]((& node --version).TrimStart("v").Split(".")[0])
if ($nodeMajor -lt 22) { throw "Node $nodeMajor found; Node 22 or newer is required for the built-in WebSocket." }

if (-not $Chrome) {
    $candidates = @(
        "$env:ProgramFiles\Google\Chrome\Application\chrome.exe",
        "${env:ProgramFiles(x86)}\Google\Chrome\Application\chrome.exe",
        "$env:LOCALAPPDATA\Google\Chrome\Application\chrome.exe",
        "${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
        "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe"
    )
    $Chrome = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Chrome -or -not (Test-Path $Chrome)) { throw "Chrome or Edge was not found. Pass -Chrome <path>." }

Write-Host "Using browser: $Chrome"
& node (Join-Path $PSScriptRoot "ExportMarkAnim.mjs") --chrome $Chrome --plugin $PluginRoot --palette $Palette
if ($LASTEXITCODE -ne 0) { throw "Mark animation export failed with exit code $LASTEXITCODE." }
