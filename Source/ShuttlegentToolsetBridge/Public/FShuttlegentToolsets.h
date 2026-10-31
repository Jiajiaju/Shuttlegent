#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Dom/JsonObject.h"
#include "Templates/ValueOrError.h"

/** One engine toolset as the settings and list_toolsets show it. */
struct FShuttlegentToolsetInfo
{
	/** Registry name, e.g. EditorToolset.EditorActorTools; what the model passes as "toolset". */
	FString Name;

	FString Description;

	/** Tools the assistant may use; 0 when the toolset is blocked or switched off. */
	int32 ToolCount = 0;

	/** Hidden for the whole team by Project Settings > Shuttlegent > General > Blocked Toolset Names. */
	bool bBlockedByProject = false;

	/** Switched off by this user (Editor Preferences > Shuttlegent > General > Engine Toolsets). */
	bool bDisabledByUser = false;
};

/** One tool of an engine toolset. */
struct FShuttlegentToolsetTool
{
	/** Tool name within its toolset, e.g. GetSelectedActors; what the model passes as "tool". */
	FString Name;

	/** Toolset.Tool */
	FString FullName;

	FString Description;
	TSharedPtr<FJsonObject> InputSchema;

	/** Runs without confirmation; every other tool asks the user first. */
	bool bReadOnly = false;

	/** Runs inside an editor transaction; false for the project's Non Undoable Tools. */
	bool bUndoable = true;

	/** Deletes, moves or renames things (the project's Destructive Tool Prefixes); its confirmation card says so. */
	bool bDestructive = false;
};

/**
 * The engine's Toolset Registry as Shuttlegent sees it: which toolsets and tools the assistant may use (the registry's
 * own filters, the project's block list, the user's switches) and which of them only read (the project's read lists).
 * Game thread only.
 */
class SHUTTLEGENTTOOLSETBRIDGE_API FShuttlegentToolsets
{
public:
	/** The registry exists (the editor is up and the Toolset Registry plugin is loaded). */
	static bool IsAvailable();

	/** Every toolset the registry offers, blocked and switched-off ones included, sorted by name. */
	static TArray<FShuttlegentToolsetInfo> GetToolsets();

	/** The toolset's tools the assistant may use; false with OutError when the toolset is unknown or not usable. */
	static bool GetTools(const FString& ToolsetName, TArray<FShuttlegentToolsetTool>& OutTools, FString& OutError);

	/** The tool answered {"returnValue": false}; for a write this means it did not do what was asked (a failed delete or rename). */
	static bool ReturnedFalse(const FString& ResultText);

	/** The usable tool of that name; false with OutError otherwise. */
	static bool FindTool(const FString& ToolsetName, const FString& ToolName, FShuttlegentToolsetTool& OutTool, FString& OutError);

	/**
	 * Runs a tool with JSON arguments; the future holds the tool's JSON result or an error. The tool runs unattended:
	 * engine message dialogs it raises are not shown, and the warnings, errors and dialog texts the engine logs while it
	 * runs on the game thread are appended to the result.
	 */
	static TFuture<TValueOrError<FString, FString>> Execute(const FString& ToolsetName, const FString& ToolName, const FString& JsonArguments);
};
