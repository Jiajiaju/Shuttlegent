#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentWriteMode.h"
#include "FShuttlegentPromptImage.h"

#include "Chat/EShuttlegentTurnOutcome.h"
#include "Chat/FShuttlegentChatEntry.h"
#include "Chat/FShuttlegentPromptQueue.h"
#include "Chat/FShuttlegentRunLogEntry.h"
#include "Chat/FShuttlegentTodoItem.h"
#include "Endpoint/FShuttlegentContextBreakdown.h"
#include "Endpoint/FShuttlegentPlanReview.h"

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

	/** 等待运行时或会话就绪的提示，及随它发送的图片。 */
	FString PendingPrompt;
	TArray<FShuttlegentPromptImage> PendingImages;

	/** 最近一次发送的提示，保持发送时的原样（含引用块），及其图片；失败的回合会重试它。 */
	FString LastSentPrompt;
	TArray<FShuttlegentPromptImage> LastSentImages;

	/** 会话未打开期间选择的配置值，按配置 id 索引；会话打开后应用。 */
	TMap<FString, FString> PendingConfig;

	/** 运行该对话的 Agent；发出第一条消息后锁定（ShuttlegentAgentModel_Design.md 第 10 节）。 */
	FString AgentId;

	int64 UsedTokens = 0;
	int64 ContextSize = 0;

	/** 运行时插件 context tap 上报的最新上下文构成；在本次运行的第一条助手消息之前为空。 */
	FShuttlegentContextBreakdown ContextBreakdown {};

	/** Agent 最新的待办清单（todo/write 每次给出完整列表）；用户发出新提示时清空。 */
	TArray<FShuttlegentTodoItem> Todos;

	/** 进行中的上下文压缩所对应的条目 id；没有进行中的压缩时为 0。 */
	int32 CompactionEntryId = 0;

	/** 用户在面板中要求的手动压缩正在执行（/compact 经 session control 插件）。 */
	bool bCompacting = false;

	/** 本次手动压缩期间运行时报来了压缩开始；为 false 时说明没有可压缩的内容。 */
	bool bCompactionStarted = false;

	/**
	 * 计划模式：会话打开后以运行时报来的状态（plan/mode 事件、恢复时查询）为准；会话打开前是用户的选择，
	 * 会话就绪后先应用它再发送提示。
	 */
	bool bPlanMode = false;

	/** 计划模式之外编辑器写入的确认方式；新对话取偏好设置中的默认值，批准计划时可改为自动接受编辑。 */
	EShuttlegentWriteMode WriteMode = EShuttlegentWriteMode::Manual;

	/** 一轮结束（用户自己停止的除外）时用户没在看这个对话；用户看到它时清除。页签与会话列表据此显示绿点。 */
	bool bUnseenFinish = false;

	/** 本对话已显示过“已读取记忆”的提示；每个对话只在第一次发送时显示一次。 */
	bool bMemoryNoticeShown = false;

	/** 等待用户审批的计划；ReviewId 为空表示没有。 */
	FShuttlegentPlanReview PendingPlanReview;

	/** 等待审批的计划在对话中的条目 id；没有时为 0。 */
	int32 PlanEntryId = 0;

	/** 等待作答的选择卡在对话中的条目 id；没有时为 0。用户改发别的消息、或回合被停止时结束等待。 */
	int32 ChoiceEntryId = 0;
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
