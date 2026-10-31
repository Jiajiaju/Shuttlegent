#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

#include "WriteGate/FShuttlegentWriteObject.h"

/** 已批准的编辑器写入的实际执行结果，按对象逐一列出，由工具上报（Shuttlegent UI 交互规范第 5.2、5.4 节）。 */
struct FShuttlegentWriteResult
{
	int32 RequestId = 0;

	FString Action;

	/** 每个对象及其真实状态；面板从不自行推断。 */
	TArray<FShuttlegentWriteObject> Objects;

	/** 写入作为一个同步编辑器事务运行。 */
	bool bUndoable = false;

	/** 该事务在编辑器撤销缓冲区中的标识；未写入任何内容或写入不可撤销时无效。 */
	FGuid TransactionId;
};
