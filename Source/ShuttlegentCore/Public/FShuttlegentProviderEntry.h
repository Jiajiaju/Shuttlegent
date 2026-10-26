#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentProviderApi.h"
#include "FShuttlegentProviderModel.h"

#include "FShuttlegentProviderEntry.generated.h"

/** A model provider route the project adds through llm-pi-ai (Shuttlegent.md section 8.2). DeepSeek is built in and needs no entry. */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentProviderEntry
{
	GENERATED_BODY()

	/** Route id, unique among providers, e.g. team-gateway; lowercase letters, digits and dashes. */
	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	FString Route;

	/** Name shown in the panel. */
	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	FString DisplayName;

	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	EShuttlegentProviderApi Api = EShuttlegentProviderApi::OpenAiCompletions;

	/** Endpoint base URL, e.g. https://gateway.example/v1. */
	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	FString BaseUrl;

	/** Environment variable the runtime reads the key from; Shuttlegent fills it from the credential manager, the editor environment or the development .env file. */
	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	FString KeyVariable;

	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	TArray<FShuttlegentProviderModel> Models;
};
