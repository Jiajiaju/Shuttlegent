#pragma once

#include "CoreMinimal.h"

#include "WriteGate/FShuttlegentWriteObject.h"

/** 等待确认的编辑器写入。由工具填写，由写入门编号。 */
struct FShuttlegentWriteRequest
{
	/** 由 FShuttlegentWriteGate 分配；即确认与结果消息中的 requestId。 */
	int32 RequestId = 0;

	/** 由写入门设置：发起请求的对话（工具调用带回的 X-Shuttlegent-Conversation）；未带时为空，由对话模型按旧规则归属。 */
	FString ConversationKey;

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

	/** 由写入门设置：按当前写入模式未弹确认卡、直接批准执行。 */
	bool bAutoApproved = false;
};
