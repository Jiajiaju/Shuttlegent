#include "FShuttlegentProjectConfig.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include "FShuttlegentModelRoutes.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentProjectSettings.h"
#include "ShuttlegentSettings.h"

namespace FShuttlegentProjectConfigPrivate
{
	TArray<FString> ToExistingAbsoluteDirs(const TArray<FDirectoryPath>& Dirs)
	{
		TArray<FString> Result;
		for (const FDirectoryPath& Dir : Dirs)
		{
			if (Dir.Path.IsEmpty()) continue;

			const FString Absolute = FPaths::ConvertRelativePathToFull(FPaths::IsRelative(Dir.Path) ? FPaths::Combine(FPaths::ProjectDir(), Dir.Path) : Dir.Path);
			if (FPaths::DirectoryExists(Absolute)) Result.AddUnique(Absolute);
		}
		return Result;
	}

	/** 插件目录下的自带目录，绝对路径；项目关闭了插件自带技能与模板、找不到插件或目录不存在时为空。 */
	FString GetBuiltinDir(const TCHAR* RelativeDir)
	{
		const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ShuttlegentPath::PluginName);
		if (!IsValid(Project) || !Project->bUseBuiltinSkills || !Plugin.IsValid()) return FString();

		const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetBaseDir(), RelativeDir));
		return FPaths::DirectoryExists(Dir) ? Dir : FString();
	}

	/** 项目目录在前、插件自带目录在后；运行时与模板库都以先列出的目录为准处理同名项。 */
	TArray<FString> WithBuiltinDir(TArray<FString> Dirs, const FString& BuiltinDir)
	{
		if (!BuiltinDir.IsEmpty()) Dirs.AddUnique(BuiltinDir);
		return Dirs;
	}

	/** 默认模型所在的路由、其密钥变量与默认模型 ID；没有已连通的连接时不改动输出。 */
	bool FindDefaultRoute(FString& OutRoute, FString& OutKeyVariable, FString& OutModelName)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		FShuttlegentModelRoute Route;
		FString ModelId;
		if (!IsValid(Settings) || !FShuttlegentModelRoutes::FindDefaultModel(*Settings, Route, ModelId)) return false;

		OutRoute = Route.Route;
		OutKeyVariable = Route.KeyVariable;
		OutModelName = ModelId;
		return true;
	}
}

bool FShuttlegentProjectConfig::IsToolEnabled(const FString& ToolName)
{
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	return !IsValid(Settings) || !Settings->DisabledTools.ContainsByPredicate([&ToolName](const FString& Disabled) { return Disabled.Equals(ToolName, ESearchCase::IgnoreCase); });
}

FString FShuttlegentProjectConfig::GetDefaultRoute()
{
	FString KeyVariable;
	FString ProviderName;
	FString Route = ShuttlegentProviderConstant::BuiltinRoute;
	FShuttlegentProjectConfigPrivate::FindDefaultRoute(Route, KeyVariable, ProviderName);
	return Route;
}

void FShuttlegentProjectConfig::GetDefaultProviderKey(FString& OutKeyVariable, FString& OutProviderName)
{
	FString Route;
	OutKeyVariable = ShuttlegentEnvVar::ProviderKey;
	OutProviderName.Reset();
	FShuttlegentProjectConfigPrivate::FindDefaultRoute(Route, OutKeyVariable, OutProviderName);
}

TArray<FString> FShuttlegentProjectConfig::GetKeyVariables()
{
	TArray<FString> Variables;
	Variables.Add(ShuttlegentEnvVar::ProviderKey);

	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	if (!IsValid(Settings)) return Variables;

	for (const FShuttlegentModelRoute& Route : FShuttlegentModelRoutes::Build(*Settings))
	{
		Variables.AddUnique(Route.KeyVariable);
	}
	return Variables;
}

TArray<FString> FShuttlegentProjectConfig::GetSkillDirs()
{
	const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
	const TArray<FString> ProjectDirs = IsValid(Project) ? FShuttlegentProjectConfigPrivate::ToExistingAbsoluteDirs(Project->SkillDirs) : TArray<FString>();
	return FShuttlegentProjectConfigPrivate::WithBuiltinDir(ProjectDirs, FShuttlegentProjectConfigPrivate::GetBuiltinDir(ShuttlegentPath::BuiltinSkillsDir));
}

TArray<FString> FShuttlegentProjectConfig::GetTemplateDirs()
{
	const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
	const TArray<FString> ProjectDirs = IsValid(Project) ? FShuttlegentProjectConfigPrivate::ToExistingAbsoluteDirs(Project->TemplateDirs) : TArray<FString>();
	return FShuttlegentProjectConfigPrivate::WithBuiltinDir(ProjectDirs, GetBuiltinTemplateDir());
}

FString FShuttlegentProjectConfig::GetBuiltinTemplateDir()
{
	return FShuttlegentProjectConfigPrivate::GetBuiltinDir(ShuttlegentPath::BuiltinTemplatesDir);
}
