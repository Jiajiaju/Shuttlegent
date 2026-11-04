#pragma once

#include "CoreMinimal.h"

/**
 * 检测个人设置中的一条模型连接，并把识别出的协议、模型与结论写回设置、存盘。
 * 地址或密钥改动后由 UShuttlegentSettings 自动调用，设置页的检测按钮也调用它。仅限游戏线程。
 */
struct SHUTTLEGENTCORE_API FShuttlegentConnectionTester
{
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnTested, int32 /*ConnectionIndex*/);

	/** 检测 UShuttlegentSettings::Connections 中的第 Index 条；正在检测时忽略。 */
	static void Test(int32 Index);

	/** 一条连接检测完、结果已写回设置时广播。 */
	static FOnTested& OnTested();
};
