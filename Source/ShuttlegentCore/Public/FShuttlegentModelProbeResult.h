#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentProviderApi.h"

/** 一次检测的结果；详细过程写在 Output Log（LogShuttlegent），这里只留结论。 */
struct FShuttlegentModelProbeResult
{
	/** 已连通：协议与模型列表已识别。 */
	bool bConnected = false;

	/** 识别出的协议。 */
	EShuttlegentProviderApi Api = EShuttlegentProviderApi::OpenAiCompletions;

	/** 可用于对话的模型 ID，最新的在前。 */
	TArray<FString> ModelIds;
};
