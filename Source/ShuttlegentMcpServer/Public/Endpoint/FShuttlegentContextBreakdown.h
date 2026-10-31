#pragma once

#include "CoreMinimal.h"

/**
 * What a session's context holds, in tokens, as the runtime's token meter estimates it (its contextBreakdown
 * projection): system prompts, tool definitions and messages. An estimate; the billed total in usage_update differs.
 */
struct FShuttlegentContextBreakdown
{
	int64 SystemTokens = 0;
	int64 ToolsTokens = 0;
	int64 MessageTokens = 0;

	bool IsEmpty() const { return SystemTokens + ToolsTokens + MessageTokens <= 0; }
};
