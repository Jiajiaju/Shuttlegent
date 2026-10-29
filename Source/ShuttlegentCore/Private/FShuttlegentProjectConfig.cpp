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
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	return IsValid(Settings) && !Settings->DefaultProvider.IsEmpty() ? Settings->DefaultProvider : FString(ShuttlegentProviderConstant::BuiltinRoute);
}

void FShuttlegentProjectConfig::GetDefaultProviderKey(FString& OutKeyVariable, FString& OutProviderName)
{
	OutKeyVariable = ShuttlegentEnvVar::ProviderKey;
	OutProviderName = ShuttlegentEnvVar::ProviderName;

	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	const FString Route = GetDefaultRoute();
	const FShuttlegentProviderEntry* Entry = IsValid(Settings) ? Settings->Providers.FindByPredicate([&Route](const FShuttlegentProviderEntry& Provider) { return Provider.Route == Route; }) : nullptr;
	if (Entry == nullptr) return;

	OutKeyVariable = GetKeyVariable(*Entry);
	OutProviderName = Entry->DisplayName.IsEmpty() ? Entry->Route : Entry->DisplayName;
}

TArray<FString> FShuttlegentProjectConfig::GetKeyVariables()
{
	TArray<FString> Variables;
	Variables.Add(ShuttlegentEnvVar::ProviderKey);

	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	if (!IsValid(Settings)) return Variables;

	for (const FShuttlegentProviderEntry& Provider : Settings->Providers)
	{
		if (!Provider.Route.IsEmpty()) Variables.AddUnique(GetKeyVariable(Provider));
	}
	return Variables;
}

FString FShuttlegentProjectConfig::GetKeyVariable(const FShuttlegentProviderEntry& Entry)
{
	const FString Route = Entry.Route.ToUpper().Replace(ShuttlegentProviderConstant::RouteSeparator, ShuttlegentProviderConstant::VariableSeparator);
	return FString(ShuttlegentProviderConstant::KeyVariablePrefix) + Route + ShuttlegentProviderConstant::KeyVariableSuffix;
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
