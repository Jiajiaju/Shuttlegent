#pragma once

#include "CoreMinimal.h"

/** 当前对话上一回合的结束方式。 */
enum class EShuttlegentTurnOutcome : uint8
{
	// 自对话打开或上一条提示发送以来尚无回合结束
	None,

	// 正常结束
	Completed,

	// 已取消，或因错误以外的原因停止
	Stopped,

	// 以错误结束
	Failed
};
