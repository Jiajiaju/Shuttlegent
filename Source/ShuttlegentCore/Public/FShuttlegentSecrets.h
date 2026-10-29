#pragma once

#include "CoreMinimal.h"

/**
 * Resolves secret variables (provider keys, MCP header values) without writing them anywhere (Shuttlegent.md section 9.2).
 * The only source is the user's settings (Editor Preferences > Shuttlegent > General). Values are never logged.
 */
struct SHUTTLEGENTCORE_API FShuttlegentSecrets
{
	/** The value of a variable: the DeepSeek key, a provider entry's key or an MCP header value; false when the settings have none. */
	static bool Resolve(const FString& Variable, FString& OutValue);
};
