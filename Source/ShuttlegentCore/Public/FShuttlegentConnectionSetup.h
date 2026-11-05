#pragma once

#include "CoreMinimal.h"

/**
 * 面板里“连接一个模型”的入口：把地址与密钥写进个人设置的模型连接、存盘并开始检测（FShuttlegentConnectionTester）。
 * 检测结论经 FShuttlegentConnectionTester::OnTested 通知。仅限游戏线程。
 */
struct SHUTTLEGENTCORE_API FShuttlegentConnectionSetup
{
	/**
	 * bBuiltin 为 true 时把密钥填进内置 DeepSeek 连接，忽略地址；否则写进地址相同的连接，没有就新增一条。
	 * 返回开始检测的连接下标；密钥为空、或非内置时地址为空则不改设置，返回 INDEX_NONE。
	 */
	static int32 Connect(bool bBuiltin, const FString& BaseUrl, const FString& ApiKey);
};
