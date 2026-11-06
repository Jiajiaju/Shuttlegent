<#
.SYNOPSIS
Packs the Shuttlegent plugin into a release zip that includes the agent runtime.

.DESCRIPTION
Copies the git-tracked plugin files a user needs (descriptor, license, README, Config,
Content/Localization, Resources, Source, Docs, Samples, Runtime/plugins, Runtime/profile), adds the built
runtime from Runtime/ (shuttlegent-runtime.exe, its ripgrep sidecar, runtime-version.json)
and the DeepSeek Harness license, and zips them under a top-level Shuttlegent/ folder:

    <OutDir>/Shuttlegent-<VersionName>-UE<EngineVersion>-win64.zip

Build/, ThirdParty/, Binaries/, Intermediate/, Saved/ and Runtime/win-x64/ are left out.
Users unzip it into their project's Plugins/ folder, enable the plugin and build the editor.

Stops when a runtime file is missing or the runtime was built from a different Harness
revision than the submodule's; warns when the included paths have uncommitted changes.

.PARAMETER OutDir
Output directory. Defaults to Saved/Release under the plugin directory.

.PARAMETER EngineVersion
Engine version written into the zip name.
#>
param(
    [string]$OutDir,
    [string]$EngineVersion = '5.8'
)

$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = New-Object System.Text.UTF8Encoding $false

$PluginDir = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$RuntimeDir = Join-Path $PluginDir 'Runtime'
$DshDir = Join-Path $PluginDir 'ThirdParty\deepseek-harness'
if (-not $OutDir) { $OutDir = Join-Path $PluginDir 'Saved\Release' }

# Git-tracked paths that go into the package as they are
$TrackedPaths = @('Shuttlegent.uplugin', 'LICENSE', 'README.md', 'Config', 'Content/Localization', 'Resources', 'Source', 'Docs', 'Samples', 'Runtime/plugins', 'Runtime/profile')

# Built runtime files, not in git; all of them are required
$RuntimeFiles = @('shuttlegent-runtime.exe', 'shuttlegent-runtime-rg.exe', 'runtime-version.json')

$Descriptor = Get-Content -Raw -Encoding UTF8 (Join-Path $PluginDir 'Shuttlegent.uplugin') | ConvertFrom-Json
$Version = $Descriptor.VersionName
if (-not $Version) { throw 'VersionName missing from Shuttlegent.uplugin.' }

Write-Host "== Checking the runtime"
foreach ($Name in $RuntimeFiles) {
    if (-not (Test-Path (Join-Path $RuntimeDir $Name))) { throw "Runtime file missing: Runtime/$Name. Build it with Build/BuildRuntime.ps1." }
}
$Stamp = Get-Content -Raw -Encoding UTF8 (Join-Path $RuntimeDir 'runtime-version.json') | ConvertFrom-Json
$Submodule = (& git -C $DshDir rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) { throw "Cannot read the DeepSeek Harness submodule revision: $DshDir" }
if ($Stamp.dshRevision -ne $Submodule) {
    throw "The runtime was built from Harness $($Stamp.dshRevision), but the submodule is at $Submodule. Rebuild it with Build/BuildRuntime.ps1."
}
Write-Host "   Harness $Submodule, built $($Stamp.builtAt)"

$Dirty = & git -C $PluginDir status --porcelain -- $TrackedPaths
if ($Dirty) {
    Write-Warning "Uncommitted changes in packaged paths; the package contains the working copy:"
    $Dirty | ForEach-Object { Write-Warning "   $_" }
}

Write-Host "== Staging"
$Stage = Join-Path $OutDir 'Shuttlegent'
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Item -ItemType Directory -Force $Stage | Out-Null

$Listing = & git -C $PluginDir -c core.quotepath=off ls-files -z -- $TrackedPaths
if ($LASTEXITCODE -ne 0) { throw 'git ls-files failed.' }
$Files = ($Listing -join '') -split "`0" | Where-Object { $_ }
foreach ($Relative in $Files) {
    $Source = Join-Path $PluginDir $Relative
    if (-not (Test-Path -LiteralPath $Source)) { throw "Tracked file missing from the working copy: $Relative" }
    $Target = Join-Path $Stage $Relative
    New-Item -ItemType Directory -Force (Split-Path $Target) | Out-Null
    Copy-Item -LiteralPath $Source -Destination $Target
}
foreach ($Name in $RuntimeFiles) {
    Copy-Item -LiteralPath (Join-Path $RuntimeDir $Name) -Destination (Join-Path $Stage "Runtime\$Name")
}
Copy-Item -LiteralPath (Join-Path $DshDir 'LICENSE') -Destination (Join-Path $Stage 'Runtime\LICENSE-deepseek-harness.txt')
Write-Host ("   {0} tracked files, {1} runtime files" -f $Files.Count, ($RuntimeFiles.Count + 1))

Write-Host "== Zipping"
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$Zip = Join-Path $OutDir "Shuttlegent-$Version-UE$EngineVersion-win64.zip"
if (Test-Path $Zip) { Remove-Item -Force $Zip }

# Entries are added one by one: ZipFile.CreateFromDirectory on .NET Framework writes '\' separators,
# which other unzip tools turn into file names instead of folders
$Archive = [IO.Compression.ZipFile]::Open($Zip, [IO.Compression.ZipArchiveMode]::Create)
try {
    $StageRoot = (Resolve-Path $Stage).Path.TrimEnd('\') + '\'
    foreach ($File in Get-ChildItem -LiteralPath $Stage -Recurse -File) {
        $Entry = 'Shuttlegent/' + $File.FullName.Substring($StageRoot.Length).Replace('\', '/')
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile($Archive, $File.FullName, $Entry, [IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
}
finally {
    $Archive.Dispose()
}
Remove-Item -Recurse -Force $Stage

$Hash = (Get-FileHash -Algorithm SHA256 $Zip).Hash.ToLower()
Write-Host ("   {0}" -f $Zip)
Write-Host ("   {0:N1} MB, SHA256 {1}" -f ((Get-Item $Zip).Length / 1MB), $Hash)
