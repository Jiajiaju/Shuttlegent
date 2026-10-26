#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentUiGuideKind.h"

#include "FShuttlegentUiGuide.generated.h"

/** One place in the editor the assistant can open for the user, with a sentence on what to do there (Shuttlegent.md section 11). */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentUiGuide
{
	GENERATED_BODY()

	/** Key the model passes to show_editor_ui, e.g. world-settings. */
	UPROPERTY(Config, EditAnywhere, Category = "Guide")
	FString Id;

	UPROPERTY(Config, EditAnywhere, Category = "Guide")
	EShuttlegentUiGuideKind Kind = EShuttlegentUiGuideKind::Tab;

	/** Tab: the tab id. Settings: Container/Category/Section, e.g. Project/Engine/Rendering. */
	UPROPERTY(Config, EditAnywhere, Category = "Guide")
	FString Target;

	/** What the user finds or does there; told to the model and returned when opened. */
	UPROPERTY(Config, EditAnywhere, Category = "Guide")
	FString Description;
};
