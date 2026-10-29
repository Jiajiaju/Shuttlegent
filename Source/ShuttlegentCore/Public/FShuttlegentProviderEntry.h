#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentProviderApi.h"
#include "FShuttlegentProviderModel.h"
#include "ShuttlegentConstants.h"

#include "FShuttlegentProviderEntry.generated.h"

/** A model provider route the user adds through llm-pi-ai (Shuttlegent.md section 8.2). DeepSeek is built in and needs no entry. */
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

	/** Endpoint base URL, e.g. https://gateway.example/v1; a new entry starts on DeepSeek's OpenAI-compatible endpoint. */
	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	FString BaseUrl = ShuttlegentProviderConstant::DeepSeekOpenAiBaseUrl;

	/** Key of this route, kept in this machine's settings only; the runtime receives it as SHUTTLEGENT_<ROUTE>_API_KEY. */
	UPROPERTY(Config, EditAnywhere, Category = "Provider", meta = (PasswordField = true))
	FString ApiKey;

	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	TArray<FShuttlegentProviderModel> Models;
};
