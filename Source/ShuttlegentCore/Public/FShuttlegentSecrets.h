#pragma once

#include "CoreMinimal.h"

/**
 * 解析机密变量（供应商密钥、MCP 请求头的值），不写到任何地方。
 * 唯一来源是个人设置（编辑器偏好设置 > Shuttlegent > General）。值绝不写入日志。
 */
struct SHUTTLEGENTCORE_API FShuttlegentSecrets
{
	/** 变量的值：模型路由的密钥（FShuttlegentModelRoutes）或 MCP 请求头的值；设置中没有时返回 false。 */
	static bool Resolve(const FString& Variable, FString& OutValue);
};
