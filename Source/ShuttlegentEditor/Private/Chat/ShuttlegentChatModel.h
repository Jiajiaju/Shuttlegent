#pragma once

#include "CoreMinimal.h"

#include "Chat/EShuttlegentAttention.h"
#include "Chat/EShuttlegentChatStatus.h"
#include "Chat/EShuttlegentSubmitResult.h"
#include "Chat/EShuttlegentTurnOutcome.h"
#include "Chat/FShuttlegentChatEntry.h"
#include "Chat/FShuttlegentConfigChoice.h"
#include "Chat/FShuttlegentConversation.h"
#include "Chat/FShuttlegentPromptQueue.h"
#include "Chat/FShuttlegentRunLogEntry.h"
#include "ShuttlegentAgentState.h"
#include "ShuttlegentSessionRecord.h"
#include "ShuttlegentStartIssue.h"
#include "QuestionGate/FShuttlegentQuestion.h"
#include "WriteGate/FShuttlegentWriteRequest.h"

class FJsonObject;
class FJsonValue;
class FShuttlegentAgentService;
class FShuttlegentQuestionGate;
class FShuttlegentWriteGate;
struct FShuttlegentSessionEvent;
enum class EShuttlegentQuestionOutcome : uint8;
enum class EShuttlegentWriteDecision : uint8;
enum class EShuttlegentWritePolicy : uint8;

DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatStructureChanged);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatContentChanged);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatStatusChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentChatTurnFinished, bool /*bCompleted*/);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatSessionsChanged);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatRunLogChanged);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatQueueChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentChatDraftRestored, const FString& /*Text*/);

/**
 * 介于 FShuttlegentAgentService 与聊天面板之间的编辑器侧对话状态。
 *
 * 位于编辑器模块中，因此对话在面板关闭并重新打开后依然保留。
 * 用户每个正在运行、排队或打开的会话对应一个 FShuttlegentConversation；
 * 面板显示当前对话，其余对话在后台继续运行并接收更新
 * （Shuttlegent.md 第 14.3.7 节）。离开空闲的对话会关闭其会话并将其丢弃；
 * 再次打开时重放其对话记录。
 *
 * 将 session/update 分块合并为条目（每个助手消息 id 一条，每次工具调用一条），
 * 并将对话的第一条提示暂存至运行时及其会话就绪。回合运行期间发送的草稿进入该对话的
 * FIFO 队列，在回合正常结束时逐条发送；停止、被拒绝的权限、失败的回合或丢失的运行时
 * 会暂停队列，直至用户继续（Shuttlegent UI 交互规范第 4.3 节）。事件均针对当前对话；
 * 后台对话仅在其状态或队列变化时触发 OnSessionsChanged。仅限游戏线程。
 */
class FShuttlegentChatModel : public TSharedFromThis<FShuttlegentChatModel>
{
public:
	/** 当前对话新增了条目或替换了列表；需要重建行。 */
	FOnShuttlegentChatStructureChanged OnStructureChanged;

	/** 当前对话条目的文本或状态已变化；行通过属性更新。 */
	FOnShuttlegentChatContentChanged OnContentChanged;

	/** 当前对话的运行时状态、配置选项、用量或运行时丢失横幅已变化。 */
	FOnShuttlegentChatStatusChanged OnStatusChanged;

	/** 当前对话的一个回合已结束；若被取消、失败或因其他原因停止，bCompleted 为 false。 */
	FOnShuttlegentChatTurnFinished OnTurnFinished;

	/** 会话索引、当前会话或任一持有对话的状态或队列已变化；需要刷新会话列表与标题。 */
	FOnShuttlegentChatSessionsChanged OnSessionsChanged;

	/** 当前对话的运行日志新增了事件，或另一个对话成为当前对话。 */
	FOnShuttlegentChatRunLogChanged OnRunLogChanged;

	/** 当前对话的队列已变化：条目被添加、发送、移除或取回，或队列被暂停或继续。 */
	FOnShuttlegentChatQueueChanged OnQueueChanged;

	/** 回到某条消息后，其原文应放回输入框以便修改重发。 */
	FOnShuttlegentChatDraftRestored OnDraftRestored;

private:
	TWeakPtr<FShuttlegentAgentService> Service;

	/** 内存中持有的对话；始终包含 Current。 */
	TArray<TSharedPtr<FShuttlegentConversation>> Conversations;
	TSharedPtr<FShuttlegentConversation> Current;

	/** session/new 正在进行中的对话，按从早到晚排列；每个答复归属最早的一个。 */
	TArray<TWeakPtr<FShuttlegentConversation>> AwaitingNewSession;

	/** 最近一个上报配置选项的会话所给出的选项；用于显示会话尚未打开的对话。 */
	TArray<TSharedPtr<FJsonValue>> LastConfigOptions;

	/** 各模型路由（"model" 配置项的取值）是否接受图片；运行时只按默认模型声明，沿用旧模型的会话逐个向运行时确认。 */
	TMap<FString, bool> ModelImageInput;

	/** 正在向运行时确认的模型路由。 */
	TSet<FString> PendingImageQueries;

	int32 NextEntryId = 1;
	int32 NextQueueId = 1;
	bool bRuntimeLost = false;

	/** Shuttlegent 页签在前台、用户看得见当前对话；此时当前对话的回合结束不留“完成未看”。 */
	bool bPanelVisible = false;

	/** 运行时无法启动或无法处理提示的原因；以启动引导卡片显示，直至重试或运行时就绪将其清除。 */
	EShuttlegentStartIssue StartIssue = EShuttlegentStartIssue::None;

	/** 因其他原因启动失败时的原因说明（英文，来自运行时或启动器）；不含密钥值。 */
	FString StartIssueDetail;

	/** 重试已停止正在运行的运行时；旧进程退出后再次启动。 */
	bool bRestartPending = false;
	bool bPermissionHandlerAttached = false;

	/** 编辑器写入门（ShuttlegentMcpServer）；面板确认其请求并显示其结果。 */
	TWeakPtr<FShuttlegentWriteGate> WriteGate;

	/** Agent 通过 ask_user 提出的问题（ShuttlegentMcpServer）；由面板答复。 */
	TWeakPtr<FShuttlegentQuestionGate> QuestionGate;

	/** 打开中的写入请求及其结果所属的对话：即其中第一个请求到达时的当前对话。 */
	TWeakPtr<FShuttlegentConversation> WriteOwner;

	/** 已确认的写入正在执行；结果到达或运行时丢失时清除。 */
	bool bWriting = false;

	/** 等待确认的写入请求（按请求 id 索引），以及最近一次确认的请求：即写入未知条目所列出的内容。 */
	TMap<int32, FShuttlegentWriteRequest> OpenWriteRequests;
	FShuttlegentWriteRequest ApprovedWrite;

	/** 撤销缓冲区监听器，在编辑器存在后添加，使写入结果能判断其是否仍可撤销。 */
	FDelegateHandle UndoBufferHandle;
	FDelegateHandle PostUndoRedoHandle;

public:
	void Initialize(const TSharedRef<FShuttlegentAgentService>& InService);
	void Shutdown();

	const TArray<TSharedPtr<FShuttlegentChatEntry>>& GetEntries() const { return Current->Entries; }
	EShuttlegentAgentState GetState() const;
	bool IsRuntimeLost() const { return bRuntimeLost; }
	bool IsStopping() const { return Current->bStopping; }
	EShuttlegentTurnOutcome GetLastOutcome() const { return Current->LastOutcome; }
	const TArray<FShuttlegentRunLogEntry>& GetRunLog() const { return Current->RunLog; }

	/** 面板中显示的会话；新对话时为空。 */
	const FString& GetCurrentSessionId() const { return Current->SessionId; }

	/** 当前对话的会话正在执行提示。 */
	bool IsPrompting() const;

	/** 当前对话供标题栏使用的状态。 */
	EShuttlegentChatStatus GetStatus() const;

	/** 内存中持有的会话的状态与排队提示数量，供会话列表使用；会话未被持有时返回 false。 */
	bool GetSessionActivity(const FString& SessionId, EShuttlegentChatStatus& OutStatus, int32& OutQueued) const;

	/** 会话列表上该会话的提醒圆点；会话未被持有时为 None。 */
	EShuttlegentAttention GetSessionAttention(const FString& SessionId) const;

	/** 页签上的提醒圆点：有对话等待输入时为 Input，否则有完成未看的对话时为 Done。 */
	EShuttlegentAttention GetOverallAttention() const;

	/** 由面板页签报告是否在前台；转到前台时当前对话的“完成未看”随之清除。 */
	void SetPanelVisible(bool bVisible);

	/** 当前对话中仍在等待答复的最早一个运行时权限请求，或 nullptr。 */
	TSharedPtr<FShuttlegentChatEntry> GetPendingPermission() const;

	/** 当前对话的某个权限请求所涉及的工具调用条目，或 nullptr。 */
	TSharedPtr<FShuttlegentChatEntry> FindToolCallOf(const TSharedPtr<FShuttlegentChatEntry>& Permission) const;
	int64 GetUsedTokens() const { return Current->UsedTokens; }
	int64 GetContextSize() const { return Current->ContextSize; }
	const FShuttlegentContextBreakdown& GetContextBreakdown() const { return Current->ContextBreakdown; }
	const TArray<FShuttlegentTodoItem>& GetTodos() const { return Current->Todos; }
	bool IsCompacting() const { return Current->bCompacting; }

	/** 当前对话现在能否手动压缩上下文；不能时给出原因（未连接、回合进行中、插件不可用、正在压缩）。 */
	bool CanCompact(FText& OutReason) const;

	/** 手动压缩当前对话的上下文（运行时的 /compact）；进度与结果以压缩条目显示在对话中。 */
	void CompactContext();

	bool IsPlanMode() const { return Current->bPlanMode; }

	/** 当前对话现在能否切换计划模式；不能时给出原因（运行时未就绪、插件不可用、恢复中）。会话尚未创建时可以，就绪后应用。 */
	bool CanSetPlanMode(FText& OutReason) const;

	/** 打开或关闭当前对话的计划模式；回合进行中时在下一步生效。 */
	void SetPlanMode(bool bActive);

	/** 当前对话等待审批的计划；ReviewId 为空表示没有。 */
	const FShuttlegentPlanReview& GetPendingPlanReview() const { return Current->PendingPlanReview; }

	/**
	 * 答复当前对话等待审批的计划：批准后 Agent 退出计划模式并执行，bAcceptEdits 时写入模式改为自动接受编辑，
	 * 否则改为手动确认；退回时 Feedback 交给 Agent 继续规划。
	 */
	void AnswerPlanReview(bool bApprove, const FString& Feedback, bool bAcceptEdits);

	/** 当前对话计划模式之外的写入确认方式。 */
	EShuttlegentWriteMode GetWriteMode() const { return Current->WriteMode; }

	/** 切换当前对话的写入确认方式；处于计划模式时同时退出计划模式。回合进行中时从下一次写入起生效。 */
	void SetWriteMode(EShuttlegentWriteMode Mode);

	/** 若运行时未在运行则启动它；失败时记为启动问题。 */
	void EnsureRuntime();

	/** 在 UShuttlegentSettings::bPrewarmRuntime 开启且无阻碍时，于编辑器启动后启动运行时。 */
	void Prewarm();

	EShuttlegentStartIssue GetStartIssue() const { return StartIssue; }
	const FString& GetStartIssueDetail() const { return StartIssueDetail; }

	/** 清除启动问题并再次启动运行时（若正在运行则重启，以便读取修正后的密钥）。 */
	void RetryStart();

	/** 当前对话上一次 session/resume 失败。 */
	bool IsResumeFailed() const { return Current->bResumeFailed; }

	/** 恢复失败后再次恢复当前对话的会话。 */
	void RetryResume();

	/** 当前对话存在用户尚未核对的写入结果；禁止发送。 */
	bool NeedsVerify() const { return Current->bNeedsVerify; }

	/** 将当前对话的某个未知写入结果标记为已由用户核对。 */
	void AcknowledgeUnknownWrite(int32 EntryId);

	/**
	 * 在当前对话中发送草稿及其引用，若该对话有回合正在运行则将其入队。每个引用都必须
	 * 仍然存在；不存在的引用会被返回，且不发送也不入队。bEnterPlanMode 时先进入计划模式（/plan <消息>）：
	 * 会话已打开时等运行时确认计划模式已开再发出，计划模式不可切换时拒绝。
	 */
	EShuttlegentSubmitResult SubmitDraft(const FString& Text, const TArray<FShuttlegentReference>& References, bool bEnterPlanMode, const TArray<FShuttlegentPromptImage>& Images, TArray<FString>& OutInvalidPaths);

	/** 草稿现在能否附带图片：运行时已就绪且其模型接受图片，当前对话也没有进行中的回合；否则 OutReason 说明原因。 */
	bool CanAttachImages(FText& OutReason) const;

	/** 当前对话中有回合正在运行、停止待完成，或运行时或会话仍在就绪过程中：新草稿将入队。 */
	bool IsBusy() const;

	/** 当前对话的队列。 */
	FShuttlegentPromptQueue GetCurrentQueue() const;

	void RemoveQueued(int32 ItemId);

	/**
	 * 将排队的条目取回草稿。草稿非空时，草稿占据该条目在队列中的位置，
	 * 因此用户所写内容不会丢失。条目已不存在时返回 false。
	 */
	bool TakeQueued(int32 ItemId, const FString& DraftText, const TArray<FShuttlegentReference>& DraftReferences, FShuttlegentQueuedPrompt& OutItem);

	/** 继续当前对话已暂停的队列，并在该对话无运行内容时发送下一个条目。 */
	void ResumeQueue();

	/** 当前对话失败的回合可以重新发送。 */
	bool CanRetry() const;

	/** 重新发送失败回合的提示；队列保持暂停。 */
	void Retry();

	/** 属于当前对话的最早一个等待确认的编辑器写入，或 nullptr。 */
	const FShuttlegentWriteRequest* GetPendingWrite() const;

	/** 确认或拒绝编辑器写入；对同一请求的再次答复不产生任何作用。 */
	void AnswerWrite(int32 RequestId, bool bApprove);

	/** 写入结果是否仍可直接撤销，以及不可撤销时的原因（Shuttlegent UI 交互规范第 5.4 节）。 */
	bool CanUndoWrite(const FShuttlegentChatEntry& Entry, FText& OutReason) const;

	/** 当写入结果的事务仍位于编辑器撤销缓冲区顶部时将其撤销。 */
	void UndoWrite(int32 EntryId);

	/** 当前对话能否回到某条消息或从它分支：会话已在运行时打开、空闲，且运行时的 session control 插件可用；不能时给出原因。 */
	bool CanEditHistory(FText& OutReason) const;

	/** 从当前对话的这条用户消息分支：新会话含到该消息这一轮结束为止的对话，原会话与编辑器状态不变。 */
	void BranchFrom(int32 EntryId);

	/**
	 * 回到当前对话的这条用户消息之前：按倒序撤销其后（含这一轮）的编辑器写入，对话截到该消息之前（原会话归档保留），
	 * 原文放回输入框。中间夹着用户自己的编辑、事务不在撤销栈顶时整体拒绝，不做任何改动。
	 */
	void RewindTo(int32 EntryId);

	/** Agent 提出的、仍在等待答复的最早一个问题，或 nullptr。 */
	const FShuttlegentQuestion* GetPendingQuestion() const;

	/** 答复或跳过一个问题；对同一问题的再次答复不产生任何作用。 */
	void AnswerQuestion(int32 QuestionId, const FString& Answer, bool bSkip);

	/** 为当前对话正在运行的回合发送一次取消；之后的调用等待该回合结束。 */
	void Cancel();

	/** 显示一个空对话；下一条提示将创建新会话。被离开的对话若处于忙碌状态则继续运行。 */
	void NewConversation();

	/** 显示一个会话：已持有的对话按原样显示，否则显示其已存储的对话记录并在运行时中恢复。 */
	void OpenSession(const FString& SessionId);

	/** 重启已丢失的运行时并恢复当前会话。 */
	void Reconnect();

	/** 处于挂接状态（面板打开）时，运行时权限请求会成为其所指对话中的面板条目。 */
	void SetPermissionHandlerAttached(bool bAttached);

	void AnswerPermission(int32 EntryId, bool bAllow);

	/** 当前对话某个配置选项（"model"、"reasoning_effort"）的可选值与当前值。 */
	bool GetConfigChoices(const FString& ConfigId, TArray<FShuttlegentConfigChoice>& OutChoices, FString& OutCurrentValue) const;

	/** 设置当前对话的配置选项；其会话打开前，该值处于等待状态。 */
	void SetConfigValue(const FString& ConfigId, const FString& Value);

	/** 索引中的会话，按最近更新时间从新到旧排列：活动列表或已归档列表。 */
	TArray<FShuttlegentSessionRecord> GetSessions(bool bArchived) const;

	/** 当前会话的标题；新对话或未命名会话时为空。 */
	FString GetCurrentTitle() const;

	/** 正在运行、等待权限或等待写入确认的会话不能归档。 */
	bool CanArchive(const FString& SessionId) const;

	/** 重命名已存储的会话；名称裁剪后为空、过长或会话未知时返回 false。 */
	bool RenameSession(const FString& SessionId, const FString& Title);

	/** 从列表中隐藏会话并丢弃其对话；归档当前会话会开始一个新对话。 */
	bool ArchiveSession(const FString& SessionId);

	void RestoreSession(const FString& SessionId);

	/** 永久移除会话的记录与对话记录，规则与归档相同；删除当前会话会开始一个新对话。 */
	bool DeleteSession(const FString& SessionId);

private:
	bool IsCurrent(const FShuttlegentConversation& Conversation) const { return Current.Get() == &Conversation; }
	TSharedPtr<FShuttlegentConversation> FindConversation(const FString& SessionId) const;
	bool IsPrompting(const FShuttlegentConversation& Conversation) const;
	bool IsBusy(const FShuttlegentConversation& Conversation) const;
	EShuttlegentChatStatus GetStatus(const FShuttlegentConversation& Conversation) const;

	/**
	 * 对话在等待用户输入：写入确认、计划审批、运行时权限请求，或 Agent 提问。提问不带会话，
	 * 归属正在运行回合的对话。
	 */
	bool NeedsInput(const FShuttlegentConversation& Conversation) const;
	EShuttlegentAttention GetAttention(const FShuttlegentConversation& Conversation) const;

	/** 用户看得见当前对话时，清除它的“完成未看”；有变化时返回 true。 */
	bool MarkCurrentSeen();
	TSharedPtr<FShuttlegentChatEntry> GetPendingPermission(const FShuttlegentConversation& Conversation) const;

	/** 打开中的写入请求及其结果是否属于该对话（没有打开的请求时视为属于当前对话）。 */
	bool OwnsWrite(const FShuttlegentConversation& Conversation) const;

	/** 离开当前对话：空闲的对话被丢弃并关闭其会话，忙碌的对话继续运行。 */
	void LeaveCurrent();
	bool CanDrop(const FShuttlegentConversation& Conversation) const;
	void DropConversation(FShuttlegentConversation& Conversation);

	/** 在另一个对话成为当前对话后，广播当前对话的全部事件。 */
	void BroadcastAll();
	void NotifyStructure(const FShuttlegentConversation& Conversation);
	void NotifyContent(const FShuttlegentConversation& Conversation);
	void NotifyStatus(const FShuttlegentConversation& Conversation);
	void NotifyQueue(const FShuttlegentConversation& Conversation);

	TSharedPtr<FShuttlegentChatEntry> AddEntry(FShuttlegentConversation& Conversation, EShuttlegentChatEntryKind Kind, const FString& Text);
	TSharedPtr<FShuttlegentChatEntry> FindToolCall(const FShuttlegentConversation& Conversation, const FString& ToolCallId) const;
	void AddNotice(FShuttlegentConversation& Conversation, const FText& Text, bool bError);
	void RejectPendingPermissions(FShuttlegentConversation& Conversation);
	void AddRunLog(FShuttlegentConversation& Conversation, const FText& Text, bool bWarning);

	void RecordStartIssue(EShuttlegentStartIssue Issue, const FString& Detail);

	/** 等待运行时或会话的提示回到其队列最前端并暂停，而不是丢失。 */
	void ParkPendingPrompts();
	void ParkPendingPrompt(FShuttlegentConversation& Conversation);

	/** 进入计划模式后发送提示（/plan <消息>）：会话未创建时记下选择，就绪后先应用再发送；会话已打开时提示先挂起，运行时确认后发出，失败则放回队列。 */
	void SubmitInPlanMode(const TSharedPtr<FShuttlegentConversation>& Conversation, const FString& Text, const TArray<FShuttlegentPromptImage>& Images);

	/** 打开或关闭对话的计划模式；OnApplied 在运行时答复后以是否生效调用（会话未创建时只记下选择，立即以 true 调用）。 */
	void ApplyPlanMode(const TSharedPtr<FShuttlegentConversation>& Conversation, bool bActive, TFunction<void(bool)> OnApplied);

	/** 发送提示，必要时先启动运行时并创建或恢复该对话的会话。 */
	void SubmitPrompt(FShuttlegentConversation& Conversation, const FString& Text, const TArray<FShuttlegentPromptImage>& Images = TArray<FShuttlegentPromptImage>());
	void SendPrompt(FShuttlegentConversation& Conversation, const FString& Text, const TArray<FShuttlegentPromptImage>& Images);

	/** 创建（尚无会话 id 时）或恢复该对话的会话，或将其标记为待运行时就绪后处理。 */
	void RequestSession(FShuttlegentConversation& Conversation);

	bool GetConfigChoices(const FShuttlegentConversation& Conversation, const FString& ConfigId, TArray<FShuttlegentConfigChoice>& OutChoices, FString& OutCurrentValue) const;
	void SetConfigValue(FShuttlegentConversation& Conversation, const FString& ConfigId, const FString& Value);

	/** 以当前所选选项固定，因此之后更换模型不会改写已入队的内容。 */
	FShuttlegentQueuedPrompt MakeQueued(const FString& Text, const TArray<FShuttlegentReference>& References);
	void PauseQueue(FShuttlegentConversation& Conversation);

	/** 当对话的队列处于运行状态且该对话没有其他运行内容时，发送下一个排队条目。 */
	void SendNextQueued(FShuttlegentConversation& Conversation);

	/** 将一个 session/update 合并为条目；At 为其到达时间（实时）或记录时间（重放）。新增了条目时返回 true。 */
	bool ApplyUpdate(FShuttlegentConversation& Conversation, const TSharedPtr<FJsonObject>& Update, const FDateTime& At);
	void ApplyTurnEnd(FShuttlegentConversation& Conversation, const FString& StopReason, const FString& ErrorMessage);
	void ReplayTranscript(FShuttlegentConversation& Conversation);

	/** 条目在对话中的位置，以及它是第几条用户消息（从 0 计，与运行时的 user/message 计数一致）；不是用户消息时返回 false。 */
	bool FindUserMessage(const FShuttlegentConversation& Conversation, int32 EntryId, int32& OutIndex, int32& OutOrdinal) const;

	/**
	 * 接手运行时分叉出的新会话：复制截断前的会话记录，登记索引（分支加后缀，回退沿用原标题），回退时归档原会话，
	 * 然后打开新会话。
	 */
	void AdoptFork(const FString& SourceSessionId, const FString& NewSessionId, int32 Ordinal, bool bThroughTurn, bool bRewind);

	void HandleStateChanged(EShuttlegentAgentState NewState);
	void HandleSessionReady(const FString& SessionId);
	void HandleSessionUpdate(const FString& SessionId, const TSharedPtr<FJsonObject>& Update);
	void HandleTurnFinished(const FString& SessionId, const FString& StopReason, const FString& ErrorMessage);
	void HandleRuntimeExited(int32 ExitCode, bool bExpected);
	void HandleError(const FString& SessionId, const FString& Message);
	void HandleConfigOptionsChanged(const FString& SessionId);

	/** 向运行时确认该模型路由是否接受图片（已知或正在确认时跳过）；结果回来后刷新面板状态。 */
	void RequestImageInput(const FString& ModelValue);
	void HandleContextBreakdown(const FString& SessionId, const FShuttlegentContextBreakdown& Breakdown);

	/** session tap 插件推来的会话事件：待办清单、上下文压缩与计划模式切换（目标暂不显示）。 */
	void HandleSessionEvent(const FShuttlegentSessionEvent& Event);
	void ApplyTodos(FShuttlegentConversation& Conversation, const TSharedPtr<FJsonObject>& Data);
	void ApplyCompaction(FShuttlegentConversation& Conversation, const FString& Type, const TSharedPtr<FJsonObject>& Data);

	/** 发送等待会话就绪的提示（如有）。 */
	void SendPendingPrompt(FShuttlegentConversation& Conversation);

	void HandlePlanReview(const FShuttlegentPlanReview& Review);
	void HandlePlanReviewClosed(const FString& SessionId, const FString& ReviewId);

	/** 给计划条目记下审批结果，写入会话记录，并清除等待中的审批。 */
	void SettlePlanReview(FShuttlegentConversation& Conversation, const TCHAR* Outcome, const FString& Feedback);

	/** 有处于计划模式的对话正在运行回合；写入门据此拒绝写入（写入请求不带会话，无法区分是哪个对话发起的）。 */
	bool IsPlanTurnRunning() const;

	/**
	 * 写入门对新请求的处理方式：计划模式的回合在运行时拒绝；否则按正在运行的对话中最严格的写入模式，删除、移动、
	 * 改名一律确认，没有对话在运行时也确认。
	 */
	EShuttlegentWritePolicy GetWritePolicy(const FShuttlegentWriteRequest& Request) const;
	void HandlePermissionRequest(const TSharedPtr<FJsonObject>& Params, TFunction<void(bool)> Respond);

	void HandleWriteRequested(const FShuttlegentWriteRequest& Request);
	void HandleWriteClosed(int32 RequestId, EShuttlegentWriteDecision Decision);
	void HandleWriteFinished(const FShuttlegentWriteResult& Result);
	void HandleUndoBufferChanged();
	void EnsureUndoListener();

	void HandleQuestionsChanged();
	void HandleQuestionClosed(const FShuttlegentQuestion& Question, EShuttlegentQuestionOutcome Outcome, const FString& Answer);
};
