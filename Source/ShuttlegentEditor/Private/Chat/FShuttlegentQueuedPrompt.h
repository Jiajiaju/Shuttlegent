#pragma once

#include "CoreMinimal.h"

#include "Reference/FShuttlegentReference.h"

/** 在会话队列中等待的一条提示。文本、引用与发送选项在入队时固定（Shuttlegent UI 交互规范第 4.3 节）。 */
struct FShuttlegentQueuedPrompt
{
	/** 用于移除或取回该条目的稳定 id。 */
	int32 Id = 0;

	/** 草稿文本，不含引用块。 */
	FString Text;

	TArray<FShuttlegentReference> References;

	/** 入队时选择的模型与推理强度；未提供该选项时为空。 */
	FString ModelValue;
	FString ReasoningValue;
};
