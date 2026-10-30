# Exports every Shuttlegent mark, lockup and icon from Build/Brand/brand-geometry.json:
# Resources/svg/{mark,lockup}-*.svg, icon128.svg, the matching PNG files, Resources/Icon128.png,
# and Resources/Web/brand-geometry.js for the panel's animated marks.
# Requires Node 22+ (built-in WebSocket) and Google Chrome or Microsoft Edge.
#
# Usage: .\Build\Brand\ExportBrand.ps1 [-Chrome <path to chrome.exe or msedge.exe>]

param(
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
& node (Join-Path $PSScriptRoot "ExportBrand.mjs") --chrome $Chrome --plugin $PluginRoot
if ($LASTEXITCODE -ne 0) { throw "Brand export failed with exit code $LASTEXITCODE." }
