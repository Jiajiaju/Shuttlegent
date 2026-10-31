#pragma once

#include "CoreMinimal.h"

/** 用户所发送草稿的处理结果。 */
enum class EShuttlegentSubmitResult : uint8
{
	// 已作为提示发送（或暂存至运行时与会话就绪）
	Sent,

	// 有回合正在运行：已追加到该会话的队列
	Queued,

	// 未被接受：文本为空、引用已不存在，或运行时已丢失
	Rejected
};
