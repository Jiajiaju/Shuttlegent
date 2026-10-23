#pragma once

#include "CoreMinimal.h"

/** How a runtime tool call reads on a chat row: readable name, one-line object summary, failure reason. Pure functions. */
struct FShuttlegentToolPresentation
{
	/** Readable name of a tool ("read" -> "读取文件"); MCP tools show their tool part; unknown names are shown as is. */
	static FText GetDisplayName(const FString& ToolName);

	/** Whether consecutive calls of this tool may be folded into a "queried N items" group. */
	static bool IsReadOnly(const FString& ToolName);

	/** One-line object summary from rawInput JSON: the first known key, paths relative to the project, truncated. */
	static FString GetSummary(const FString& InputJson);

	/** First line of a failed call's result without the runtime's "Error:" prefix, truncated. */
	static FString GetFailureReason(const FString& Result);
};
