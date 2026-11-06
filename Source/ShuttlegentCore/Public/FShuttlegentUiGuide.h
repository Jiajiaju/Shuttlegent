#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentUiGuideKind.h"

#include "FShuttlegentUiGuide.generated.h"

/** An editor location the assistant can open for the user, with a short note on what to do there. */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentUiGuide
{
	GENERATED_BODY()

	/** Key the model passes to show_editor_ui, for example world-settings. */
	UPROPERTY(Config, EditAnywhere, Category = "Guide")
	FString Id;

	/** Kind of location to open: an editor tab or a settings page. */
	UPROPERTY(Config, EditAnywhere, Category = "Guide")
	EShuttlegentUiGuideKind Kind = EShuttlegentUiGuideKind::Tab;

	/** Tab: tab ID. Settings: container/category/section, for example Project/Engine/Rendering. */
	UPROPERTY(Config, EditAnywhere, Category = "Guide")
	FString Target;

	/** What the user can find or do there; told to the model and returned when the location opens. */
	UPROPERTY(Config, EditAnywhere, Category = "Guide")
	FString Description;
};
