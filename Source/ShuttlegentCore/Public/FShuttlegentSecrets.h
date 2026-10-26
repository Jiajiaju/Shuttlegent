#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentSecretSource.h"

/**
 * Resolves secret variables (provider keys, MCP header values) without writing them anywhere (Shuttlegent.md section 9.2).
 * Order: the Windows Credential Manager entry of the variable, the editor's environment, the plugin's development .env file.
 * Values are never logged.
 */
struct SHUTTLEGENTCORE_API FShuttlegentSecrets
{
	/** The value of a variable; false when no source has it. */
	static bool Resolve(const FString& Variable, FString& OutValue, EShuttlegentSecretSource& OutSource);

	/** Credential Manager entry a variable is read from: the personal setting's entry, else Shuttlegent/<variable>. */
	static FString GetCredentialTarget(const FString& Variable);

	/** Full path of the plugin's development .env file. */
	static FString GetDevEnvFilePath();
};
