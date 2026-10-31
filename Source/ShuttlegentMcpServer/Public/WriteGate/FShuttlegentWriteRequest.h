#pragma once

#include "CoreMinimal.h"

#include "WriteGate/FShuttlegentWriteObject.h"

/** 等待确认的编辑器写入（Shuttlegent UI 交互规范第 5.1 节）。由工具填写，由写入门编号。 */
struct FShuttlegentWriteRequest
{
	/** 由 FShuttlegentWriteGate 分配；即交互规范中的 requestId。 */
	int32 RequestId = 0;

	/** 发起请求的工具，名称与模型调用时一致。 */
	FString ToolName;

	/** 写入所执行的操作，以确认卡上显示的文字描述。 */
	FString Action;

	/** 对象所属的关卡。 */
	FString Level;

	/** 工具将其作为一个同步编辑器事务运行，因而可撤销；为 false 时在任何事务之外运行。 */
	bool bUndoable = false;

	/** 会删除、移动或重命名内容；确认卡会对此给出警告。 */
	bool bDestructive = false;

	TArray<FShuttlegentWriteObject> Objects;

	/** 工具参数的 JSON 形式，按需显示。 */
	FString RawArguments;

	/** 由写入门设置：请求无人作答而超时的时刻（UTC）；确认卡据此倒计时。 */
	FDateTime ExpiresAtUtc {};
};
