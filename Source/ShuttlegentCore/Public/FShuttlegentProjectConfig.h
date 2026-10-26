#pragma once

#include "CoreMinimal.h"

/** Reads the effective configuration out of the project and personal settings (Shuttlegent.md section 11). Game thread only. */
struct SHUTTLEGENTCORE_API FShuttlegentProjectConfig
{
	/** Whether the user's settings leave an editor tool switched on. */
	static bool IsToolEnabled(const FString& ToolName);

	/** Route new sessions start on: the project's default provider, else the built-in DeepSeek route. */
	static FString GetDefaultRoute();

	/** Key variable and display name of the default route; for the start check and the missing-key guide card. */
	static void GetDefaultProviderKey(FString& OutKeyVariable, FString& OutProviderName);

	/** Every variable the runtime may need: the built-in DeepSeek key (also used by web search) and each provider's key. */
	static TArray<FString> GetKeyVariables();

	/** Absolute folders of the project's skill directories that exist. */
	static TArray<FString> GetSkillDirs();

	/** Absolute folders of the project's template directories that exist. */
	static TArray<FString> GetTemplateDirs();
};
