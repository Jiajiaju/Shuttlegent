#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentProviderModel.generated.h"

/** One model a provider route serves. */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentProviderModel
{
	GENERATED_BODY()

	/** Model id the endpoint expects, e.g. gpt-4.1 or qwen-max. */
	UPROPERTY(Config, EditAnywhere, Category = "Model")
	FString Id;

	/** Name shown in the panel's model picker; the id when empty. */
	UPROPERTY(Config, EditAnywhere, Category = "Model")
	FString Name;
};
