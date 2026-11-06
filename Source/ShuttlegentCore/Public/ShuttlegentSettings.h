#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "EShuttlegentWriteMode.h"
#include "FShuttlegentAgentEntry.h"
#include "FShuttlegentConnection.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentPermissionPolicy.h"

#include "ShuttlegentSettings.generated.h"

/** Personal Shuttlegent settings (Editor Preferences > Shuttlegent > General). */
UCLASS(Config = EditorPerProjectUserSettings, meta = (DisplayName = "Shuttlegent"))
class SHUTTLEGENTCORE_API UShuttlegentSettings : public UDeveloperSettings
{
	GENERATED_BODY()

private:
	/** 编辑前内置连接的密钥；内置连接被误删后按它加回，密钥不丢失。 */
	FString BuiltinApiKeyBeforeEdit;

public:
	/** Runtime executable started instead of the plugin's Runtime/shuttlegent-runtime.exe; leave empty to use the default. */
	UPROPERTY(Config, EditAnywhere, Category = "Runtime", meta = (FilePathFilter = "exe"))
	FFilePath RuntimeExecutableOverride {};

	/** Start the runtime while the editor is idle after launch, so it is already connected when the panel opens. */
	UPROPERTY(Config, EditAnywhere, Category = "Runtime")
	bool bPrewarmRuntime = true;

	/**
	 * Model connections: enter only an address and key for each; the protocol and available models are detected. The first model of the first connected connection is the default model for new sessions.
	 * The built-in DeepSeek connection is preinstalled and cannot be removed. After connections are added or removed, or a test result changes, the panel restarts the runtime to apply it when no turn is running.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Models")
	TArray<FShuttlegentConnection> Connections;

	/** Agents provided by the plugin: enabled agents appear in the session's agent picker, each with its own default model. */
	UPROPERTY(Config, EditAnywhere, EditFixedSize, Category = "Agents")
	TArray<FShuttlegentAgentEntry> Agents;

	/** Agent used by default in new sessions. */
	UPROPERTY(Config, EditAnywhere, Category = "Defaults")
	FString DefaultAgent = ShuttlegentAgentConstant::DefaultAgentId;

	/** Let the runtime's file tools write to the project folder without asking; when off, every write needs elevation. Takes effect the next time the runtime starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	bool bAllowWorkspaceWrite = false;

	/** Give the runtime a PowerShell tool. Takes effect the next time the runtime starts. */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	bool bAllowShell = false;

	/** Let the runtime ask to run tools outside the sandbox; when off, such requests are rejected at once and noted in the conversation. */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	bool bAllowElevation = true;

	/** Editor tools not offered to the model, by tool name; shown as switches under the Editor Tools category. Takes effect immediately. */
	UPROPERTY(Config, EditAnywhere, Category = "Editor Tools")
	TArray<FString> DisabledTools;

	/** Engine toolsets not offered to the model, by toolset name; shown as switches under the Engine Toolsets category. Takes effect immediately. */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> DisabledToolsets;

	/** Values of the variables referenced by external MCP server headers (the variables are named in Project Settings > Shuttlegent > General), for example TEAM_KB_TOKEN -> <token>; stored only in the settings on this machine. */
	UPROPERTY(Config, EditAnywhere, Category = "Credentials")
	TMap<FString, FString> McpHeaderValues;

	/** Answer to runtime permission requests when no editor dialog handles them. */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	EShuttlegentPermissionPolicy PermissionPolicy = EShuttlegentPermissionPolicy::Reject;

	/** How editor writes are confirmed in new conversations; each conversation can switch it in the panel. Deleting, moving and renaming always need confirmation. */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	EShuttlegentWriteMode DefaultWriteMode = EShuttlegentWriteMode::Manual;

	/** Play the logo animations in the panel (intro, working, hover, done); when off, the logo always shows a still frame. */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	bool bAnimateMark = true;

	/** Font size of the conversation and the input box in the panel, in CSS pixels; the sidebar and controls keep their size. */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance", meta = (ClampMin = "11", ClampMax = "20", UIMin = "11", UIMax = "20"))
	int32 FontSize = ShuttlegentAppearanceConstant::DefaultFontSize;

	/** Let the conversation and the input box fill the whole width between the side panes instead of a centered reading column. */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	bool bWideLayout = false;

	/** Font for the sidebar, menus, buttons and cards; several can be listed, separated by commas. Leave empty to use Segoe UI and Microsoft YaHei UI. Takes effect immediately. */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	FString UiFontFamily;

	/** Font for conversation messages and the input box; leave empty to follow the UI font. For replies that read more like a printed page, try a serif font such as Source Han Serif. Takes effect immediately. */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	FString ChatFontFamily;

	/** Monospace font for code, arguments and logs; leave empty to use Cascadia Mono and Consolas. Takes effect immediately. */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	FString CodeFontFamily;

	/**
	 * 补齐内置 DeepSeek 连接：缺少时插到最前，地址、协议与模型恢复为预装值，密钥保持。
	 * Agent 列表与 FShuttlegentAgentCatalog 对齐，默认 Agent 不可用时改回默认循环。
	 */
	void EnsureBuiltins();

	//~ Begin UObject Interface.
#if WITH_EDITOR
	virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
#endif // WITH_EDITOR
	//~ End UObject Interface.

	//~ Begin UDeveloperSettings Interface.
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End UDeveloperSettings Interface.
};
