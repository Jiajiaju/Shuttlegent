#pragma once

#include "CoreMinimal.h"

/** agent 向用户提出的带候选答案的问题（ask_user）。由工具填写，由提问通道编号。 */
struct FShuttlegentQuestion
{
	/** 由 FShuttlegentQuestionGate 分配。 */
	int32 QuestionId = 0;

	/** 由提问通道设置：提问的对话（工具调用带回的 X-Shuttlegent-Conversation）；未带时为空。 */
	FString ConversationKey;

	FString Question;

	/** 提供的候选答案，按显示顺序排列。 */
	TArray<FString> Options;

	/** 用户可不选择选项，改为自行填写答案。 */
	bool bAllowOther = true;

	/** 由提问通道设置：问题无人作答而超时的时刻（UTC）；卡片据此倒计时。 */
	FDateTime ExpiresAtUtc {};
};
