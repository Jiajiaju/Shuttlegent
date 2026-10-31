#pragma once

#include "CoreMinimal.h"

/** agent 向用户提出的问题以何种方式离开提问通道。每个问题恰好离开一次。 */
enum class EShuttlegentQuestionOutcome : uint8
{
	// 用户选择了某个选项或填写了答案
	Answered,

	// 用户未作答即关闭了问题
	Skipped,

	// 时限内无人作答；该时限低于运行时的工具调用超时
	TimedOut,

	// 问题已不再适用（本轮已结束或运行时已丢失）
	Expired,

	// 没有打开的面板可显示该问题
	NoPanel
};
