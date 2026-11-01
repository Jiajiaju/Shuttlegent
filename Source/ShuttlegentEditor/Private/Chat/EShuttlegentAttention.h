#pragma once

#include "CoreMinimal.h"

/** 对话需要用户留意的程度，显示为页签与会话列表上的圆点。 */
enum class EShuttlegentAttention : uint8
{
	// 无需提醒
	None,

	// 一轮已结束，用户还没看过（绿点）
	Done,

	// 等待用户输入：写入确认、Agent 提问、计划审批或运行时权限请求（黄点）
	Input
};
