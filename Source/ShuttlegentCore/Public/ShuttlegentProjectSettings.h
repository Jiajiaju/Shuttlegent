#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "FShuttlegentExternalMcpServer.h"
#include "FShuttlegentProviderEntry.h"
#include "FShuttlegentUiGuide.h"

#include "ShuttlegentProjectSettings.generated.h"

/**
 * Settings the project shares through version control (Project Settings > Plugins > Shuttlegent, Shuttlegent.md section 11).
 * Plugin defaults come from the plugin's Config/DefaultShuttlegent.ini; the project overrides them in its own
 * Config/DefaultShuttlegent.ini. Personal choices live in UShuttlegentSettings.
 */
UCLASS(Config = Shuttlegent, DefaultConfig, meta = (DisplayName = "Shuttlegent"))
class SHUTTLEGENTCORE_API UShuttlegentProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Provider routes besides the built-in DeepSeek one, e.g. a team gateway. */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	TArray<FShuttlegentProviderEntry> Providers;

	/** Route new sessions start on; empty uses the built-in DeepSeek route. */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	FString DefaultProvider;

	/** Model new sessions start on, among the default route's models; empty uses the route's first. */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	FString DefaultModel;

	/** Skill folders the runtime loads, relative to the project directory. */
	UPROPERTY(Config, EditAnywhere, Category = "Integration", meta = (RelativeToGameDir))
	TArray<FDirectoryPath> SkillDirs;

	/** Folders of slash templates (*.md), relative to the project directory. */
	UPROPERTY(Config, EditAnywhere, Category = "Integration", meta = (RelativeToGameDir))
	TArray<FDirectoryPath> TemplateDirs;

	/** Knowledge or tool services declared to the runtime with every session. */
	UPROPERTY(Config, EditAnywhere, Category = "Integration")
	TArray<FShuttlegentExternalMcpServer> ExternalMcpServers;

	/** Places in the editor the assistant can open for the user. */
	UPROPERTY(Config, EditAnywhere, Category = "Integration")
	TArray<FShuttlegentUiGuide> UiGuides;

	//~ Begin UDeveloperSettings Interface.
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End UDeveloperSettings Interface.
};
