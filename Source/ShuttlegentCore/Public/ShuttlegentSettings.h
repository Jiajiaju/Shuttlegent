#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "ShuttlegentConstants.h"
#include "ShuttlegentPermissionPolicy.h"

#include "ShuttlegentSettings.generated.h"

/** Per-user Shuttlegent settings (Editor Preferences > Plugins > Shuttlegent). */
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

	/** Windows Credential Manager entry per key variable, e.g. DEEPSEEK_API_KEY -> MyKeys/DeepSeek; a variable without an entry here reads Shuttlegent/<variable>. */
	UPROPERTY(Config, EditAnywhere, Category = "Credentials")
	TMap<FString, FString> CredentialTargets;

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
