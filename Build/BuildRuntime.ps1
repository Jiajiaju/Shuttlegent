<#
.SYNOPSIS
Builds the Shuttlegent agent runtime from the pinned DeepSeek Harness submodule.

.DESCRIPTION
Installs dependencies, builds DeepSeek Harness, packs the single-file executable
(Node SEA via @yao-pkg/pkg) and copies it into Runtime/ as shuttlegent-runtime.exe,
together with its ripgrep sidecar and resource directory.

Requirements on the build machine only: Node.js 22.19+ or 24+ with corepack, and git.
pnpm is invoked through corepack at the version pinned by the submodule.

.PARAMETER SkipInstall
Skip `pnpm install` (dependencies already installed).

.PARAMETER SkipBuild
Skip `pnpm run build` (lib/ outputs already built).

.PARAMETER Target
pkg target triple. Only node24-win-x64 is supported for now.
#>
param(
    [switch]$SkipInstall,
    [switch]$SkipBuild,
    [string]$Target = 'node24-win-x64'
)

$ErrorActionPreference = 'Stop'

$PluginDir = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$DshDir = Join-Path $PluginDir 'ThirdParty\deepseek-harness'
$RuntimeDir = Join-Path $PluginDir 'Runtime'
$MinNodeVersion = [Version]'22.19.0'
$ProductBase = 'deepseek-harness-sdk-runtime-win-x64'
$RuntimeBase = 'shuttlegent-runtime'

function Invoke-Step {
    param([string]$Label, [scriptblock]$Action)

    Write-Host "== $Label"
    $Watch = [Diagnostics.Stopwatch]::StartNew()
    & $Action
    if ($LASTEXITCODE -ne 0) { throw "$Label failed with exit code $LASTEXITCODE." }
    Write-Host ("   done in {0:N1}s" -f $Watch.Elapsed.TotalSeconds)
}

if ($Target -ne 'node24-win-x64') { throw "Unsupported target: $Target" }
if (-not (Test-Path (Join-Path $DshDir 'package.json'))) {
    throw "DeepSeek Harness submodule missing: $DshDir. Run: git submodule update --init --depth 1"
}

$NodeCommand = Get-Command node -ErrorAction SilentlyContinue
if (-not $NodeCommand) { throw 'node not found on PATH.' }
$NodeVersion = [Version]((& node --version).TrimStart('v'))
if ($NodeVersion -lt $MinNodeVersion) { throw "Node $NodeVersion is too old; need $MinNodeVersion or newer." }
if (-not (Get-Command corepack -ErrorAction SilentlyContinue)) { throw 'corepack not found on PATH.' }

# CI=true skips the submodule's git-hook installer (lefthook), which fails in a
# submodule checkout and would otherwise make every pnpm command re-run install.
$env:CI = 'true'
$env:COREPACK_ENABLE_DOWNLOAD_PROMPT = '0'

Push-Location $DshDir
try {
    if (-not $SkipInstall) { Invoke-Step 'pnpm install' { corepack pnpm install --frozen-lockfile } }
    if (-not $SkipBuild) { Invoke-Step 'pnpm run build' { corepack pnpm run build } }
    Invoke-Step 'pack executable' { corepack pnpm exec tsx scripts/build-exe-for-python-sdk.ts --skip-build "--targets=$Target" }
}
finally {
    Pop-Location
}

$DistDir = Join-Path $DshDir 'dist-exe'
$Products = @{
    (Join-Path $DistDir "$ProductBase.exe") = (Join-Path $RuntimeDir "$RuntimeBase.exe")
    (Join-Path $DistDir "$ProductBase-rg.exe") = (Join-Path $RuntimeDir "$RuntimeBase-rg.exe")
}
foreach ($Source in $Products.Keys) {
    if (-not (Test-Path $Source)) { throw "Expected product missing: $Source" }
}

Write-Host '== copy to Runtime/'
New-Item -ItemType Directory -Force $RuntimeDir | Out-Null
foreach ($Source in $Products.Keys) { Copy-Item -Force $Source $Products[$Source] }
$ResourceSource = Join-Path $DistDir 'win-x64'
$ResourceTarget = Join-Path $RuntimeDir 'win-x64'
if (Test-Path $ResourceTarget) { Remove-Item -Recurse -Force $ResourceTarget }
Copy-Item -Recurse $ResourceSource $ResourceTarget

$Revision = (& git -C $DshDir rev-parse HEAD).Trim()
$Stamp = [ordered]@{ dshRevision = $Revision; target = $Target; builtAt = (Get-Date).ToString('o') }
[IO.File]::WriteAllText((Join-Path $RuntimeDir 'runtime-version.json'), ($Stamp | ConvertTo-Json), (New-Object System.Text.UTF8Encoding $false))

$Exe = Get-Item (Join-Path $RuntimeDir "$RuntimeBase.exe")
Write-Host ("Runtime ready: {0} ({1:N1} MB), dsh {2}" -f $Exe.FullName, ($Exe.Length / 1MB), $Revision.Substring(0, 8))
exit 0
