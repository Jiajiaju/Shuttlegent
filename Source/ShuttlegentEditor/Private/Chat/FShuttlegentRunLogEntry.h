#pragma once

#include "CoreMinimal.h"

/** 检视器运行日志中显示的当前对话的一个事件。从不包含运行时 stderr、密钥或环境变量。 */
struct FShuttlegentRunLogEntry
{
	/** 事件发生的时间（UTC）。 */
	FDateTime Time {};

	FText Text;

	/** 取消、失败或断开连接；以警告色显示。 */
	bool bWarning = false;
};
