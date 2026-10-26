#include "ShuttlegentRuntimeLaunchSpec.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include "FShuttlegentGeneratedPatch.h"
#include "FShuttlegentProjectConfig.h"
#include "FShuttlegentSecrets.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentSettings.h"

namespace ShuttlegentRuntimeLaunchSpecPrivate
{
	// DSH_PERMISSION_MODE values understood by dsh-permission-presets
	inline const TCHAR* PermissionModeReadOnly = TEXT("read-only");
	inline const TCHAR* PermissionModeWorkspaceWrite = TEXT("workspace-write");

	// Command-line switches of the dsh launcher
	inline const TCHAR* ProfileSwitch = TEXT("--profile");
	inline const TCHAR* PatchSwitch = TEXT("--patch");

}

bool FShuttlegentRuntimeLaunchSpec::Build(FShuttlegentRuntimeLaunchSpec& OutSpec, FString& OutError, EShuttlegentStartIssue& OutIssue)
{
	OutIssue = EShuttlegentStartIssue::Other;
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ShuttlegentPath::PluginName);
	if (!Plugin.IsValid())
	{
		OutError = TEXT("Shuttlegent plugin descriptor not found.");
		return false;
	}

	const FString PluginDir = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir());
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();

	FString Executable = FPaths::Combine(PluginDir, ShuttlegentPath::RuntimeExecutable);
	if (IsValid(Settings) && !Settings->RuntimeExecutableOverride.FilePath.IsEmpty())
	{
		Executable = FPaths::ConvertRelativePathToFull(Settings->RuntimeExecutableOverride.FilePath);
	}

	const FString PatchFile = FPaths::Combine(PluginDir, ShuttlegentPath::ProfilePatch);
	if (!FPaths::FileExists(Executable))
	{
		OutIssue = EShuttlegentStartIssue::MissingRuntime;
		OutError = FString::Printf(TEXT("Runtime executable not found: %s. Build it with Build/BuildRuntime.ps1."), *Executable);
		return false;
	}
	if (!FPaths::FileExists(PatchFile))
	{
		OutIssue = EShuttlegentStartIssue::MissingRuntime;
		OutError = FString::Printf(TEXT("Profile patch not found: %s."), *PatchFile);
		return false;
	}

	const FString LocalAppData = FPlatformMisc::GetEnvironmentVariable(ShuttlegentEnvVar::LocalAppData);
	if (LocalAppData.IsEmpty())
	{
		OutError = TEXT("LOCALAPPDATA is not set; cannot place DSH_HOME.");
		return false;
	}

	const FString DshHome = FPaths::ConvertRelativePathToFull(FPaths::Combine(LocalAppData, ShuttlegentPath::DshHome));
	const FString DshAgentsHome = FPaths::Combine(DshHome, ShuttlegentPath::DshAgentsHome);
	if (!IFileManager::Get().MakeDirectory(*DshAgentsHome, true))
	{
		OutError = FString::Printf(TEXT("Cannot create DSH_HOME: %s."), *DshHome);
		return false;
	}

	// The project's providers and skills and the user's Shell switch go into a second patch on top of the plugin's
	const bool bWorkspaceWrite = IsValid(Settings) && Settings->bAllowWorkspaceWrite;
	const bool bShell = IsValid(Settings) && Settings->bAllowShell;
	FString GeneratedPatch;
	if (!FShuttlegentGeneratedPatch::Write(DshHome, bShell, GeneratedPatch, OutError)) return false;

	OutSpec = FShuttlegentRuntimeLaunchSpec {};
	OutSpec.Executable = Executable;
	OutSpec.Arguments = { ShuttlegentRuntimeLaunchSpecPrivate::ProfileSwitch, ShuttlegentPath::AcpProfile, ShuttlegentRuntimeLaunchSpecPrivate::PatchSwitch, PatchFile, ShuttlegentRuntimeLaunchSpecPrivate::PatchSwitch, GeneratedPatch };
	OutSpec.WorkingDirectory = DshHome;

	for (const TCHAR* Name : ShuttlegentEnvVar::InheritedNames)
	{
		const FString Value = FPlatformMisc::GetEnvironmentVariable(Name);
		if (!Value.IsEmpty()) OutSpec.Environment.Add(Name, Value);
	}

	// Only the key variables the configuration names are passed on, each from its first source
	for (const FString& Variable : FShuttlegentProjectConfig::GetKeyVariables())
	{
		FString Value;
		EShuttlegentSecretSource Source = EShuttlegentSecretSource::None;
		if (FShuttlegentSecrets::Resolve(Variable, Value, Source)) OutSpec.Environment.Add(Variable, Value);
	}

	FString KeyVariable;
	FString ProviderName;
	FShuttlegentProjectConfig::GetDefaultProviderKey(KeyVariable, ProviderName);
	if (!OutSpec.Environment.Contains(KeyVariable))
	{
		OutIssue = EShuttlegentStartIssue::MissingKey;
		OutError = FString::Printf(TEXT("%s (%s) is set neither in the Credential Manager entry %s, the environment nor %s."), *KeyVariable, *ProviderName, *FShuttlegentSecrets::GetCredentialTarget(KeyVariable), *FShuttlegentSecrets::GetDevEnvFilePath());
		return false;
	}

	OutSpec.Environment.Add(ShuttlegentEnvVar::DshHome, DshHome);
	OutSpec.Environment.Add(ShuttlegentEnvVar::DshAgentsHome, DshAgentsHome);
	OutSpec.Environment.Add(ShuttlegentEnvVar::DshPermissionMode, bWorkspaceWrite ? ShuttlegentRuntimeLaunchSpecPrivate::PermissionModeWorkspaceWrite : ShuttlegentRuntimeLaunchSpecPrivate::PermissionModeReadOnly);
	UE_LOG(LogShuttlegent, Log, TEXT("Runtime launch: workspace write %s, shell %s, default route %s."), bWorkspaceWrite ? TEXT("on") : TEXT("off"), bShell ? TEXT("on") : TEXT("off"), *FShuttlegentProjectConfig::GetDefaultRoute());
	OutSpec.Environment.Add(ShuttlegentEnvVar::DshTelemetryDisabled, ShuttlegentEnvVar::TelemetryDisabledValue);
	OutIssue = EShuttlegentStartIssue::None;
	return true;
}
