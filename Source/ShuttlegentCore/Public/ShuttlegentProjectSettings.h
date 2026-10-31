#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "FShuttlegentExternalMcpServer.h"
#include "FShuttlegentUiGuide.h"

#include "ShuttlegentProjectSettings.generated.h"

/**
 * Settings the project shares through version control (Project Settings > Shuttlegent > General, Shuttlegent.md section 11).
 * Plugin defaults come from the plugin's Config/DefaultShuttlegent.ini; the project overrides them in its own
 * Config/DefaultShuttlegent.ini. Personal choices, model providers and their keys included, live in UShuttlegentSettings.
 */
UCLASS(Config = Shuttlegent, DefaultConfig, meta = (DisplayName = "Shuttlegent"))
class SHUTTLEGENTCORE_API UShuttlegentProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
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

	/**
	 * Engine toolsets (Toolset Registry) and engine toolset tools the assistant never sees, for the whole team. A pattern
	 * matches a toolset name or a full tool name (Toolset.Tool) as a case-insensitive substring; /pattern/ is a regular expression.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> BlockedToolsetNames;

	/**
	 * Engine toolset tools whose name starts with one of these words only read and run without confirmation, e.g. Get
	 * matches GetSelectedActors and get_actors. Every other engine toolset tool asks the user first.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> ReadOnlyToolPrefixes;

	/** Further engine toolset tools that only read, by tool name (Snapshot) or full name (Toolset.Tool). */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> ReadOnlyTools;

	/** Engine toolset tools that write although their name reads like a query; they always ask first. Wins over the read lists. */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> WriteTools;

	/**
	 * Engine toolset tools that run outside an editor transaction and are shown as not undoable, by tool name or full
	 * name. For tools whose undo breaks the edited object, e.g. DataTable tools that rebuild the table in place.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> NonUndoableTools;

	/**
	 * Engine toolset tools whose name starts with one of these words delete, move or rename things; their confirmation
	 * card is marked as destructive. Matched like Read Only Tool Prefixes.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> DestructiveToolPrefixes;

	//~ Begin UDeveloperSettings Interface.
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End UDeveloperSettings Interface.
};
