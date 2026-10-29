#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "FShuttlegentProviderEntry.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentPermissionPolicy.h"

#include "ShuttlegentSettings.generated.h"

/** Per-user Shuttlegent settings (Editor Preferences > Shuttlegent > General). */
UCLASS(Config = EditorPerProjectUserSettings, meta = (DisplayName = "Shuttlegent"))
class SHUTTLEGENTCORE_API UShuttlegentSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** Runtime executable to launch instead of the plugin's Runtime/shuttlegent-runtime.exe. Leave empty for the default. */
	UPROPERTY(Config, EditAnywhere, Category = "Runtime", meta = (FilePathFilter = "exe"))
	FFilePath RuntimeExecutableOverride {};

	/** Starts the runtime once the editor is idle after startup, so the panel is connected when it opens. */
	UPROPERTY(Config, EditAnywhere, Category = "Runtime")
	bool bPrewarmRuntime = true;

	/** Endpoint of the built-in DeepSeek route (Anthropic Messages protocol); empty uses DeepSeek's official endpoint. Takes effect when the runtime next starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	FString DeepSeekBaseUrl = ShuttlegentProviderConstant::DeepSeekBaseUrl;

	/** Key of the built-in DeepSeek route, kept in this machine's settings only; the runtime does not start on this route without it. */
	UPROPERTY(Config, EditAnywhere, Category = "Providers", meta = (PasswordField = true))
	FString DeepSeekApiKey;

	/** Provider routes besides the built-in DeepSeek one, e.g. a team gateway. Takes effect when the runtime next starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	TArray<FShuttlegentProviderEntry> Providers;

	/** Route new sessions start on; empty uses the built-in DeepSeek route. */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	FString DefaultProvider;

	/** Model new sessions start on, among the default route's models; empty uses the route's first. */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	FString DefaultModel;

	/** The runtime's file tools may write inside the project without asking; off, every write is an escalation. Takes effect when the runtime next starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	bool bAllowWorkspaceWrite = false;

	/** The runtime gets its PowerShell tool. Takes effect when the runtime next starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	bool bAllowShell = false;

	/** The runtime may ask to escalate a tool beyond its sandbox; off, such requests are rejected at once with a note in the conversation. */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	bool bAllowElevation = true;

	/** Editor tools withheld from the model, by name; shown as switches under Editor Tools. Takes effect at once. */
	UPROPERTY(Config, EditAnywhere, Category = "Editor Tools")
	TArray<FString> DisabledTools;

	/** Values of the variables external MCP server headers name (Project Settings > Shuttlegent > General), e.g. TEAM_KB_TOKEN -> <token>; kept in this machine's settings only. */
	UPROPERTY(Config, EditAnywhere, Category = "Credentials")
	TMap<FString, FString> McpHeaderValues;

	/** Answer to runtime permission requests when no editor dialog handles them. */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	EShuttlegentPermissionPolicy PermissionPolicy = EShuttlegentPermissionPolicy::Reject;

	/** Plays the mark animations in the panel (intro, working, hover, done). When off the mark always shows its rest frame. */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	bool bAnimateMark = true;

	/** Font size of the conversation and the message input in the panel, in CSS pixels; the panel's sidebar and controls keep their size. */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance", meta = (ClampMin = "11", ClampMax = "20", UIMin = "11", UIMax = "20"))
	int32 FontSize = ShuttlegentAppearanceConstant::DefaultFontSize;

	/** The conversation and the message input span the whole width between the side panes instead of a centered reading column. */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	bool bWideLayout = false;

	//~ Begin UDeveloperSettings Interface.
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End UDeveloperSettings Interface.
};
