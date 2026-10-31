#pragma once

#include "CoreMinimal.h"

#include "Chat/EShuttlegentChatEntryKind.h"
#include "WriteGate/FShuttlegentWriteResult.h"

/** 聊天面板中的一行。控件通过属性读取它，因此流式文本无需重建。 */
struct FShuttlegentChatEntry
{
	EShuttlegentChatEntryKind Kind = EShuttlegentChatEntryKind::Notice;

	/** 消息、提示、通知或错误文本。 */
	FString Text;

	/** 助手消息的推理文本；折叠显示。 */
	FString Thought;

	/** 助手条目的 ACP messageId。 */
	FString MessageId;

	/** 工具调用或权限条目的 ACP toolCallId。 */
	FString ToolCallId;

	/** 工具标题，或权限请求所涉及的工具。 */
	FString Title;

	/** 工具调用状态：pending、in_progress、completed 或 failed。 */
	FString Status;

	/** 附加详情：权限请求的工具理由。 */
	FString Detail;

	/** 工具调用：紧凑 JSON 形式的 rawInput。 */
	FString Input;

	/** 工具调用：最新结果内容的文本，包括失败调用的错误文本。 */
	FString Result;

	/** 助手条目：第一个与最新一个推理分块的时间（UTC）；无推理时不设置。 */
	FDateTime ThoughtStartUtc {};
	FDateTime ThoughtEndUtc {};

	/** 助手条目：推理已展开。 */
	bool bThoughtExpanded = false;

	/** 权限条目：是否已答复及答复内容。写入未知条目：用户核对对象后置 bAnswered。 */
	bool bAnswered = false;
	bool bAllowed = false;

	/** 报告失败的通知或回合结束。 */
	bool bError = false;

	/** 写入结果条目：写入的执行内容，以及“撤销本次修改”是否已将其还原。 */
	FShuttlegentWriteResult WriteResult;
	bool bUndone = false;

	/** 用于答复待处理权限请求的稳定 id。 */
	int32 Id = 0;
};
