<#
.SYNOPSIS
Gathers, imports, exports and compiles the Shuttlegent localization targets with UAT Localise.

.DESCRIPTION
Runs the engine's standard plugin localization pipeline for the three targets declared in
Shuttlegent.uplugin (Shuttlegent, ShuttlegentToolTips, ShuttlegentPropertyNames), using the
config files in Config/Localization:

    RunUAT Localise -UEProjectDirectory=<project> -UEProjectName=<project> -IncludePlugins=Shuttlegent

Translations are edited in Content/Localization/<Target>/zh-Hans/<Target>.po; running this
script again imports them and rebuilds the .locres files. Word count and conflict reports
go to <project>/Saved/Localization/Shuttlegent.

The plugin must sit in <project>/Plugins/Shuttlegent, the editor modules must be built, and
the project's editor should be closed so the commandlet can load the plugin.

By default the source gather skips the nested macro prepass (-SkipNestedMacroPrepass), which
otherwise scans all engine and project source for macros that wrap LOCTEXT and takes tens of
minutes. Shuttlegent defines no such macros; use -FullPrepass once it does.

.PARAMETER Steps
Localization steps passed to UAT. Defaults to Gather,Import,Export,Compile,GenerateReports.

.PARAMETER FullPrepass
Run the nested macro prepass over all engine and project source.
#>
param(
    [string]$Steps = 'Gather,Import,Export,Compile,GenerateReports',
    [switch]$FullPrepass
)

$ErrorActionPreference = 'Stop'

$PluginDir = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$ProjectDir = (Resolve-Path (Join-Path $PluginDir '..\..')).Path
$Uproject = Get-ChildItem -LiteralPath $ProjectDir -Filter '*.uproject' | Select-Object -First 1
if (-not $Uproject) { throw "No .uproject found in $ProjectDir; the plugin must be in <project>/Plugins/Shuttlegent." }
$ProjectName = $Uproject.BaseName

$Root = $ProjectDir
while ($Root -and -not (Test-Path (Join-Path $Root 'Engine\Build\BatchFiles\RunUAT.bat'))) {
    $Parent = Split-Path $Root -Parent
    if ($Parent -eq $Root) { $Root = $null } else { $Root = $Parent }
}
if (-not $Root) { throw "No engine with Engine/Build/BatchFiles/RunUAT.bat found above $ProjectDir." }

$ProjectRelative = $ProjectDir.Substring($Root.Length).TrimStart('\').Replace('\', '/')
$RunUat = Join-Path $Root 'Engine\Build\BatchFiles\RunUAT.bat'

$UatArgs = @('Localise', "-UEProjectRoot=$Root", "-UEProjectDirectory=$ProjectRelative", "-UEProjectName=$ProjectName", '-IncludePlugins=Shuttlegent', "-LocalizationSteps=$Steps")
if (-not $FullPrepass) { $UatArgs += '-AdditionalCommandletArguments=-SkipNestedMacroPrepass' }

Write-Host "== Localise $ProjectName ($ProjectRelative), plugin Shuttlegent, steps $Steps, full prepass $([bool]$FullPrepass)"
& $RunUat @UatArgs
if ($LASTEXITCODE -ne 0) { throw "UAT Localise failed with exit code $LASTEXITCODE." }

Write-Host "== Done. Reports: $ProjectDir\Saved\Localization\Shuttlegent"
