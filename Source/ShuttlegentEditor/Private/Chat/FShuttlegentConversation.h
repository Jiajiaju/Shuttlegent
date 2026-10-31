#pragma once

#include "CoreMinimal.h"

#include "Chat/EShuttlegentTurnOutcome.h"
#include "Chat/FShuttlegentChatEntry.h"
#include "Chat/FShuttlegentPromptQueue.h"
#include "Chat/FShuttlegentRunLogEntry.h"
#include "Endpoint/FShuttlegentContextBreakdown.h"

/**
 * 面板持有的一个对话：某会话的条目、运行日志、队列与回合状态。
 * 多个对话可同时运行；面板只显示其中一个（Shuttlegent.md 第 14.3.7 节）。
 */
struct FShuttlegentConversation : public TSharedFromThis<FShuttlegentConversation>
{
	/** 运行时会话；新对话在 session/new 答复前为空。 */
	FString SessionId;

	TArray<TSharedPtr<FShuttlegentChatEntry>> Entries;

	/** 本对话中仍在等待答复的权限请求，按条目 id 索引。 */
	TMap<int32, TFunction<void(bool)>> PendingPermissions;

	TArray<FShuttlegentRunLogEntry> RunLog;
	FShuttlegentPromptQueue Queue;

	/** 等待运行时或会话就绪的提示。 */
	FString PendingPrompt;

	/** 最近一次发送的提示，保持发送时的原样（含引用块）；失败的回合会重试它。 */
	FString LastSentPrompt;

	/** 会话未打开期间选择的配置值，按配置 id 索引；会话打开后应用。 */
	TMap<FString, FString> PendingConfig;

	int64 UsedTokens = 0;
	int64 ContextSize = 0;

	/** 运行时插件 context tap 上报的最新上下文构成；在本次运行的第一条助手消息之前为空。 */
	FShuttlegentContextBreakdown ContextBreakdown {};
	EShuttlegentTurnOutcome LastOutcome = EShuttlegentTurnOutcome::None;

	/** 已为正在运行的回合发送停止；回合结束时清除。 */
	bool bStopping = false;

	/** session/resume 正在进行，或正在等待运行时启动。 */
	bool bResuming = false;

	/** 运行时就绪后需要执行 session/new 或 session/resume。 */
	bool bWantsSession = false;

	/** 上一次 session/resume 失败；本地记录予以保留，新会话不会继承其上下文。 */
	bool bResumeFailed = false;

	/** 某次写入结果未知且其对象尚未核对；核对完成前禁止发送。 */
	bool bNeedsVerify = false;
};
