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
	// dsh-permission-presets 识别的 DSH_PERMISSION_MODE 取值
	inline const TCHAR* PermissionModeReadOnly = TEXT("read-only");
	inline const TCHAR* PermissionModeWorkspaceWrite = TEXT("workspace-write");

	// dsh 启动器的命令行开关
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

	// 用户的供应商、Shell 开关与项目技能写入第二个补丁，叠加在插件补丁之上
	const bool bWorkspaceWrite = IsValid(Settings) && Settings->bAllowWorkspaceWrite;
	const bool bShell = IsValid(Settings) && Settings->bAllowShell;
	FString GeneratedPatch;
	if (!FShuttlegentGeneratedPatch::Write(DshHome, PluginDir, bShell, GeneratedPatch, OutError)) return false;

	OutSpec = FShuttlegentRuntimeLaunchSpec {};
	OutSpec.Executable = Executable;
	OutSpec.Arguments = { ShuttlegentRuntimeLaunchSpecPrivate::ProfileSwitch, ShuttlegentPath::AcpProfile, ShuttlegentRuntimeLaunchSpecPrivate::PatchSwitch, PatchFile, ShuttlegentRuntimeLaunchSpecPrivate::PatchSwitch, GeneratedPatch };
	OutSpec.WorkingDirectory = DshHome;

	for (const TCHAR* Name : ShuttlegentEnvVar::InheritedNames)
	{
		const FString Value = FPlatformMisc::GetEnvironmentVariable(Name);
		if (!Value.IsEmpty()) OutSpec.Environment.Add(Name, Value);
	}

	// 只传个人设置中已有的密钥，各自放在对应路由的变量中
	for (const FString& Variable : FShuttlegentProjectConfig::GetKeyVariables())
	{
		FString Value;
		if (FShuttlegentSecrets::Resolve(Variable, Value)) OutSpec.Environment.Add(Variable, Value);
	}

	FString KeyVariable;
	FString ProviderName;
	FShuttlegentProjectConfig::GetDefaultProviderKey(KeyVariable, ProviderName);
	if (!OutSpec.Environment.Contains(KeyVariable))
	{
		OutIssue = EShuttlegentStartIssue::MissingKey;
		OutError = FString::Printf(TEXT("%s has no API key in Editor Preferences > Shuttlegent > General > Providers."), *ProviderName);
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
