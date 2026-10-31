#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentMcpHeader.generated.h"

/** 发给外部 MCP 服务的 HTTP 请求头；值取自变量，密钥不写进配置。 */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentMcpHeader
{
	GENERATED_BODY()

	/** 请求头名称，例如 Authorization。 */
	UPROPERTY(Config, EditAnywhere, Category = "Header")
	FString Name;

	/** 加在变量值之前的文字，例如 "Bearer "。 */
	UPROPERTY(Config, EditAnywhere, Category = "Header")
	FString Prefix;

	/** 存放值的变量名；每位用户在 编辑器偏好设置 > Shuttlegent > General > Mcp Header Values 中填写。 */
	UPROPERTY(Config, EditAnywhere, Category = "Header")
	FString ValueVariable;
};
