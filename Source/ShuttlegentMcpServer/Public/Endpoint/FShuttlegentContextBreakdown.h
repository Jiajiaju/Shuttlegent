#pragma once

#include "CoreMinimal.h"

/**
 * 会话上下文的构成，以 token 计，取自运行时 token 计量器的估算（其 contextBreakdown 投影）：系统提示词、
 * 工具定义与消息。仅为估算值，与 usage_update 中的计费总量不同。
 */
struct FShuttlegentContextBreakdown
{
	int64 SystemTokens = 0;
	int64 ToolsTokens = 0;
	int64 MessageTokens = 0;

	bool IsEmpty() const { return SystemTokens + ToolsTokens + MessageTokens <= 0; }
};
