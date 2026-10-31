#pragma once

#include "CoreMinimal.h"

/** 编辑器写入中单个对象的处理结果（Shuttlegent UI 交互规范第 5.2 节）。 */
enum class EShuttlegentWriteObjectStatus : uint8
{
	// 尚未写入：请求正在等待确认，或批次在该对象之前已停止
	NotRun,

	// 已写入
	Completed,

	// 该对象写入失败；原因见消息
	Failed,

	// 已写入，但因同批次另一对象失败而随整个事务回滚
	RolledBack
};
