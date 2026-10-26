#include "FShuttlegentProjectConfig.h"

#include "Misc/Paths.h"

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
}

bool FShuttlegentProjectConfig::IsToolEnabled(const FString& ToolName)
{
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	return !IsValid(Settings) || !Settings->DisabledTools.ContainsByPredicate([&ToolName](const FString& Disabled) { return Disabled.Equals(ToolName, ESearchCase::IgnoreCase); });
}

FString FShuttlegentProjectConfig::GetDefaultRoute()
{
	const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
	return IsValid(Project) && !Project->DefaultProvider.IsEmpty() ? Project->DefaultProvider : FString(ShuttlegentProviderConstant::BuiltinRoute);
}

void FShuttlegentProjectConfig::GetDefaultProviderKey(FString& OutKeyVariable, FString& OutProviderName)
{
	OutKeyVariable = ShuttlegentEnvVar::ProviderKey;
	OutProviderName = ShuttlegentEnvVar::ProviderName;

	const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
	const FString Route = GetDefaultRoute();
	const FShuttlegentProviderEntry* Entry = IsValid(Project) ? Project->Providers.FindByPredicate([&Route](const FShuttlegentProviderEntry& Provider) { return Provider.Route == Route; }) : nullptr;
	if (Entry == nullptr) return;

	OutKeyVariable = Entry->KeyVariable;
	OutProviderName = Entry->DisplayName.IsEmpty() ? Entry->Route : Entry->DisplayName;
}

TArray<FString> FShuttlegentProjectConfig::GetKeyVariables()
{
	TArray<FString> Variables;
	Variables.Add(ShuttlegentEnvVar::ProviderKey);

	const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
	if (!IsValid(Project)) return Variables;

	for (const FShuttlegentProviderEntry& Provider : Project->Providers)
	{
		if (!Provider.KeyVariable.IsEmpty()) Variables.AddUnique(Provider.KeyVariable);
	}
	return Variables;
}

TArray<FString> FShuttlegentProjectConfig::GetSkillDirs()
{
	const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
	return IsValid(Project) ? FShuttlegentProjectConfigPrivate::ToExistingAbsoluteDirs(Project->SkillDirs) : TArray<FString>();
}

TArray<FString> FShuttlegentProjectConfig::GetTemplateDirs()
{
	const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
	return IsValid(Project) ? FShuttlegentProjectConfigPrivate::ToExistingAbsoluteDirs(Project->TemplateDirs) : TArray<FString>();
}
