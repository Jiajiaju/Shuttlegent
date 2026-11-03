#pragma once

#include "CoreMinimal.h"

#include "QuestionGate/FShuttlegentChoiceOption.h"

/**
 * Agent 交给用户的一张选择卡（ask_choices）。与 ask_user 不同，它不等待作答：卡片留在对话中，
 * 用户提交后，选择作为下一条用户消息发给 Agent，因此不受运行时工具调用超时的限制。
 */
struct FShuttlegentChoiceRequest
{
	/** 由 FShuttlegentQuestionGate 分配的全局唯一键；会话记录据此把后来的提交结果对应到这张卡片。 */
	FString ChoiceKey;

	/** 由提问通道设置：交出卡片的对话（工具调用带回的 X-Shuttlegent-Conversation）；未带时为空。 */
	FString ConversationKey;

	FString Question;

	/** 按显示顺序排列的选项。 */
	TArray<FShuttlegentChoiceOption> Options;

	/** 可选多项；否则只能选一项。 */
	bool bMultiSelect = false;

	/** 每一项可写评语。 */
	bool bItemNotes = false;

	/** 卡片底部可写整体意见。 */
	bool bOverallNote = false;
};
