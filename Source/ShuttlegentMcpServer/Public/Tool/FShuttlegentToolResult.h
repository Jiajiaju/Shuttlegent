#pragma once

#include "CoreMinimal.h"

/** 工具调用返回给模型的文本；bError 标记未完成所请求操作的调用。 */
struct SHUTTLEGENTMCPSERVER_API FShuttlegentToolResult
{
	FString Text;
	bool bError = false;

	static FShuttlegentToolResult Ok(FString InText);
	static FShuttlegentToolResult Error(FString InText);
};
