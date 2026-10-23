#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "ShuttlegentPermissionMode.h"
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

	/** Sandbox preset of the runtime's built-in file and shell tools. */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	EShuttlegentPermissionMode PermissionMode = EShuttlegentPermissionMode::ReadOnly;

	/** Answer to runtime permission requests when no editor dialog handles them. */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	EShuttlegentPermissionPolicy PermissionPolicy = EShuttlegentPermissionPolicy::Reject;

	/** Plays the mark animations in the panel (intro, working, hover, done). When off the mark always shows its rest frame. */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	bool bAnimateMark = true;

	//~ Begin UDeveloperSettings Interface.
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End UDeveloperSettings Interface.
};
