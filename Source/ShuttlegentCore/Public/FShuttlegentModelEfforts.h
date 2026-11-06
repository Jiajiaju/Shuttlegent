#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentModelEfforts.generated.h"

/**
 * Reasoning effort levels available for a model, looked up in the models.opencode.ai catalog when the connection is tested (FShuttlegentEffortCatalog).
 * Only models with known levels are recorded; other models declare no levels, and their requests carry no reasoning parameter and keep the provider's default behavior.
 */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentModelEfforts
{
	GENERATED_BODY()

	/** Model ID. */
	UPROPERTY(Config)
	FString ModelId;

	/** Available levels, using the runtime's level names (minimal, low, medium, high, xhigh, max), from low to high. */
	UPROPERTY(Config)
	TArray<FString> Efforts;
};
