#pragma once

#include "CoreMinimal.h"

#include "Tool/FShuttlegentToolImage.h"

/** 工具调用返回给模型的文本与图片；bError 标记未完成所请求操作的调用。 */
struct SHUTTLEGENTMCPSERVER_API FShuttlegentToolResult
{
	FString Text;
	bool bError = false;

	/** 排在文本之后的图片；运行时只在模型声明图片输入时把它们交给模型，否则替换为说明文字。 */
	TArray<FShuttlegentToolImage> Images;

	static FShuttlegentToolResult Ok(FString InText);
	static FShuttlegentToolResult Error(FString InText);
};
