#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentModelProbeResult.h"
#include "FShuttlegentModelProbeTarget.h"

class UShuttlegentSettings;

/**
 * 检测连接：编辑器直接向模型服务发一次最小请求并分类结果，不依赖运行时（ShuttlegentAgentModel_Design.md 第 8 节）。
 * 请求路径与运行时一致；日志只记录地址、状态码与耗时，绝不记录密钥与响应正文。仅限游戏线程。
 */
struct SHUTTLEGENTCORE_API FShuttlegentModelProbe
{
	DECLARE_DELEGATE_OneParam(FOnProbeDone, const FShuttlegentModelProbeResult& /*Result*/);

	/** 模型条目的检测对象：地址、协议、模型 ID 取自条目，密钥按运行时规则取（留空时沿用同地址同协议的模型的密钥）。 */
	static FShuttlegentModelProbeTarget MakeTarget(const UShuttlegentSettings& Settings, int32 ModelIndex);

	/** 运行时对该对象实际请求的完整地址；地址为空时返回空。 */
	static FString GetRequestUrl(const FShuttlegentModelProbeTarget& Target);

	/** 发出检测请求；缺少地址、密钥或模型 ID 时不发请求。OnDone 在游戏线程上恰好调用一次。 */
	static void Probe(const FShuttlegentModelProbeTarget& Target, FOnProbeDone OnDone);
};
