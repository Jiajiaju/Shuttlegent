#pragma once

#include "CoreMinimal.h"

/**
 * 解析机密变量（供应商密钥、MCP 请求头的值），不写到任何地方（Shuttlegent.md 第 9.2 节）。
 * 唯一来源是个人设置（编辑器偏好设置 > Shuttlegent > General）。值绝不写入日志。
 */
struct SHUTTLEGENTCORE_API FShuttlegentSecrets
{
	/** 变量的值：DeepSeek 密钥、供应商条目的密钥或 MCP 请求头的值；设置中没有时返回 false。 */
	static bool Resolve(const FString& Variable, FString& OutValue);
};
