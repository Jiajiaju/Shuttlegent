#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentConnectionStatus.h"
#include "EShuttlegentProviderApi.h"
#include "FShuttlegentModelEfforts.h"

#include "FShuttlegentConnection.generated.h"

/**
 * A model connection in the personal settings: enter only the address and key; the protocol and available models are detected (FShuttlegentConnectionTester).
 * Each connection is one runtime route, and every detected model appears in the session's model menu.
 */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentConnection
{
	GENERATED_BODY()

	/** Service address, for example https://api.openai.com/v1. The built-in DeepSeek connection has a fixed address. */
	UPROPERTY(Config, EditAnywhere, Category = "Connection")
	FString BaseUrl;

	/** API key, stored only in the settings on this machine. */
	UPROPERTY(Config, EditAnywhere, Category = "Connection", meta = (PasswordField = true))
	FString ApiKey;

	/** Built-in DeepSeek connection: served by the runtime's llm-deepseek with a fixed address, protocol and models; only the key is needed, and it cannot be removed. */
	UPROPERTY(Config)
	bool bBuiltin = false;

	/** Detected API protocol. */
	UPROPERTY(Config)
	EShuttlegentProviderApi Api = EShuttlegentProviderApi::OpenAiCompletions;

	/** Detected chat model IDs, newest first; the first one is this connection's default model. */
	UPROPERTY(Config)
	TArray<FString> ModelIds;

	/** Models whose reasoning effort levels were found during the test; requests to other models carry no reasoning parameter and keep the provider's default behavior. */
	UPROPERTY(Config)
	TArray<FShuttlegentModelEfforts> ModelEfforts;

	/** Result of the latest test. */
	UPROPERTY(Config)
	EShuttlegentConnectionStatus Status = EShuttlegentConnectionStatus::Untested;

	/** Fingerprint of the address and key at the latest test; a mismatch with the current values means they changed and need a new test. Does not contain the key itself. */
	UPROPERTY(Config)
	FString TestedFingerprint;
};
