#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "FShuttlegentExternalMcpServer.h"
#include "FShuttlegentUiGuide.h"

#include "ShuttlegentProjectSettings.generated.h"

/**
 * Project settings shared through version control (Project Settings > Shuttlegent > General).
 * Plugin defaults come from the plugin's Config/DefaultShuttlegent.ini; the project overrides them in its own Config/DefaultShuttlegent.ini.
 * Personal options, including model providers and their keys, are in UShuttlegentSettings.
 */
UCLASS(Config = Shuttlegent, DefaultConfig, meta = (DisplayName = "Shuttlegent"))
class SHUTTLEGENTCORE_API UShuttlegentProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Skill folders loaded by the runtime, relative to the project folder. */
	UPROPERTY(Config, EditAnywhere, Category = "Integration", meta = (RelativeToGameDir))
	TArray<FDirectoryPath> SkillDirs;

	/** Folders that hold slash templates (*.md), relative to the project folder. */
	UPROPERTY(Config, EditAnywhere, Category = "Integration", meta = (RelativeToGameDir))
	TArray<FDirectoryPath> TemplateDirs;

	/** Also load the skills and slash templates that ship with the plugin; they come after the two folder lists above, and the project's version wins on a name clash. */
	UPROPERTY(Config, EditAnywhere, Category = "Integration", meta = (DisplayName = "Use Built-in Skills and Templates"))
	bool bUseBuiltinSkills = true;

	/** Knowledge or tool servers declared to the runtime in every session. */
	UPROPERTY(Config, EditAnywhere, Category = "Integration")
	TArray<FShuttlegentExternalMcpServer> ExternalMcpServers;

	/** Editor locations the assistant can open for the user. */
	UPROPERTY(Config, EditAnywhere, Category = "Integration")
	TArray<FShuttlegentUiGuide> UiGuides;

	/**
	 * Engine toolsets (Toolset Registry) and tools hidden from the assistant for the whole team. Each rule is a case-insensitive substring of
	 * the toolset name or the full tool name (Toolset.Tool); write /pattern/ for a regular expression.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> BlockedToolsetNames;

	/**
	 * Engine toolset tools whose names start with these words are read-only and run without confirmation; for example, Get matches GetSelectedActors and get_actors.
	 * All other engine toolset tools need the user's confirmation before they run.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> ReadOnlyToolPrefixes;

	/** Additional engine toolset tools treated as read-only, by tool name (Snapshot) or full name (Toolset.Tool). */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> ReadOnlyTools;

	/** Engine toolset tools that look like queries but write; they always need confirmation, overriding the read-only rules. */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> WriteTools;

	/**
	 * Engine toolset tools that run outside an editor transaction and are marked as not undoable, by tool name or full name.
	 * Use this for tools whose undo would corrupt the edited object, such as DataTable tools that rebuild the whole table in place.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> NonUndoableTools;

	/**
	 * Engine toolset tools whose names start with these words delete, move or rename, and their confirmation cards are marked as destructive. Matching works like Read Only Tool Prefixes.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> DestructiveToolPrefixes;

	//~ Begin UDeveloperSettings Interface.
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End UDeveloperSettings Interface.
};
