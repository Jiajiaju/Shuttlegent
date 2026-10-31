#pragma once

#include "CoreMinimal.h"

#include "Chat/FShuttlegentQueuedPrompt.h"

/** 某会话在回合运行期间所发送提示的 FIFO 队列。暂停的队列等待“继续队列”（Shuttlegent UI 交互规范第 4.3 节）。 */
struct FShuttlegentPromptQueue
{
	TArray<FShuttlegentQueuedPrompt> Items;

	/** 由停止、被拒绝的权限、失败的回合或丢失的运行时置位；只能由用户清除。 */
	bool bPaused = false;
};
