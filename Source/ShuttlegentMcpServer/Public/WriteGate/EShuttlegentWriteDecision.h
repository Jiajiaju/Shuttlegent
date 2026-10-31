#pragma once

#include "CoreMinimal.h"

/** 待处理的编辑器写入请求以何种方式离开写入门。每个请求恰好离开一次。 */
enum class EShuttlegentWriteDecision : uint8
{
	// 用户确认了该请求；由工具执行
	Approved,

	// 用户拒绝了该请求，或没有打开的确认 UI 可供征询
	Rejected,

	// 其预览已不再成立（关卡已切换、运行时丢失、本轮已结束）；未写入任何内容
	Expired,

	// 确认时限内无人作答；该时限低于运行时的工具调用超时；未写入任何内容
	TimedOut
};
