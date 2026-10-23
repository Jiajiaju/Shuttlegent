#include "ShuttlegentRuntimeLaunchSpec.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

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

	// .env syntax
	inline const TCHAR* CommentPrefix = TEXT("#");
	inline const TCHAR* Assignment = TEXT("=");
	inline const TCHAR* DoubleQuote = TEXT("\"");
	inline const TCHAR* SingleQuote = TEXT("'");

	FString StripQuotes(const FString& Value)
	{
		const bool bDoubleQuoted = Value.Len() >= 2 && Value.StartsWith(DoubleQuote) && Value.EndsWith(DoubleQuote);
		const bool bSingleQuoted = Value.Len() >= 2 && Value.StartsWith(SingleQuote) && Value.EndsWith(SingleQuote);
		if (bDoubleQuoted || bSingleQuoted) return Value.Mid(1, Value.Len() - 2);
		return Value;
	}

	/** Reads KEY=VALUE lines from the development .env file. Values are never logged. */
	void LoadDevEnvFile(const FString& Path, TMap<FString, FString>& OutEnvironment)
	{
		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *Path)) return;

		for (const FString& RawLine : Lines)
		{
			const FString Line = RawLine.TrimStartAndEnd();
			if (Line.IsEmpty() || Line.StartsWith(CommentPrefix)) continue;

			FString Name;
			FString Value;
			if (!Line.Split(Assignment, &Name, &Value)) continue;

			Name = Name.TrimStartAndEnd();
			Value = StripQuotes(Value.TrimStartAndEnd());
			if (Name.IsEmpty() || Value.IsEmpty()) continue;

			OutEnvironment.Add(Name, Value);
			UE_LOG(LogShuttlegent, Verbose, TEXT("Loaded %s from the development .env file."), *Name);
		}
	}

	const TCHAR* ToDshPermissionMode(EShuttlegentPermissionMode Mode)
	{
		switch (Mode)
		{
		case EShuttlegentPermissionMode::WorkspaceWrite:
			{
				return PermissionModeWorkspaceWrite;
			}
		case EShuttlegentPermissionMode::ReadOnly:
		default:
			{
				return PermissionModeReadOnly;
			}
		}
	}
}

bool FShuttlegentRuntimeLaunchSpec::Build(FShuttlegentRuntimeLaunchSpec& OutSpec, FString& OutError)
{
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
		OutError = FString::Printf(TEXT("Runtime executable not found: %s. Build it with Build/BuildRuntime.ps1."), *Executable);
		return false;
	}
	if (!FPaths::FileExists(PatchFile))
	{
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

	OutSpec = FShuttlegentRuntimeLaunchSpec {};
	OutSpec.Executable = Executable;
	OutSpec.Arguments = { ShuttlegentRuntimeLaunchSpecPrivate::ProfileSwitch, ShuttlegentPath::AcpProfile, ShuttlegentRuntimeLaunchSpecPrivate::PatchSwitch, PatchFile };
	OutSpec.WorkingDirectory = DshHome;

	for (const TCHAR* Name : ShuttlegentEnvVar::InheritedNames)
	{
		const FString Value = FPlatformMisc::GetEnvironmentVariable(Name);
		if (!Value.IsEmpty()) OutSpec.Environment.Add(Name, Value);
	}

	ShuttlegentRuntimeLaunchSpecPrivate::LoadDevEnvFile(FPaths::Combine(PluginDir, ShuttlegentPath::DevEnvFile), OutSpec.Environment);

	const EShuttlegentPermissionMode PermissionMode = IsValid(Settings) ? Settings->PermissionMode : EShuttlegentPermissionMode::ReadOnly;
	OutSpec.Environment.Add(ShuttlegentEnvVar::DshHome, DshHome);
	OutSpec.Environment.Add(ShuttlegentEnvVar::DshAgentsHome, DshAgentsHome);
	OutSpec.Environment.Add(ShuttlegentEnvVar::DshPermissionMode, ShuttlegentRuntimeLaunchSpecPrivate::ToDshPermissionMode(PermissionMode));
	OutSpec.Environment.Add(ShuttlegentEnvVar::DshTelemetryDisabled, ShuttlegentEnvVar::TelemetryDisabledValue);
	return true;
}
