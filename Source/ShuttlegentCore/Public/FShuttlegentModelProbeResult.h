#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentModelProbeOutcome.h"

/** 一次检测连接的结果。 */
struct FShuttlegentModelProbeResult
{
	/** 结论。 */
	EShuttlegentModelProbeOutcome Outcome = EShuttlegentModelProbeOutcome::Unreachable;

	/** 检测对象的显示名。 */
	FString Label;

	/** 发出的模型 ID。 */
	FString ModelId;

	/** 实际请求的完整地址；未发请求时为空。 */
	FString Url;

	/** HTTP 状态码；没有收到响应时为 0。 */
	int32 StatusCode = 0;

	/** 从发出到收到响应或失败的耗时，单位毫秒。 */
	int32 ElapsedMs = 0;

	/** 没有收到响应的原因是超时，而不是解析或连接失败。 */
	bool bTimedOut = false;

	/** 响应正文的开头，用于显示服务方的错误说明；不写日志。 */
	FString Detail;
};
