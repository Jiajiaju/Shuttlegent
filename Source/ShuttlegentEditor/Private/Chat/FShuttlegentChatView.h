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
#include "ShuttlegentAgentState.h"
#include "ShuttlegentSessionRecord.h"
#include "ShuttlegentStartIssue.h"

class FShuttlegentChatModel;
struct FShuttlegentQuestion;
struct FShuttlegentWriteRequest;

DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatViewChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentChatViewTurnFinished, bool /*bCompleted*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentChatViewDraftRestored, const FString& /*Text*/);

/**
 * 一个页签的对话视图：持有该页签显示的对话（当前对话），把面板对“当前对话”的读取与操作转给 FShuttlegentChatModel，
 * 并从模型带对话的事件中挑出属于当前对话的转发给面板。切换对话（新对话、打开会话、归档或删除、分支与回退）在此完成：
 * 被离开的对话交还模型，没有视图显示且空闲时由模型释放。所在页签是否在前台经 SetVisible 告诉模型，据此清除“完成未看”。
 * 仅限游戏线程。
 */
class FShuttlegentChatView : public TSharedFromThis<FShuttlegentChatView>
{
public:
	/** 当前对话新增了条目或替换了列表，或换了一个对话；需要重建行。 */
	FOnShuttlegentChatViewChanged OnStructureChanged;

	/** 当前对话条目的文本或状态已变化；行通过属性更新。 */
	FOnShuttlegentChatViewChanged OnContentChanged;

	/** 当前对话的运行时状态、配置选项、用量或运行时丢失横幅已变化。 */
	FOnShuttlegentChatViewChanged OnStatusChanged;

	/** 当前对话的一个回合已结束；若被取消、失败或因其他原因停止，bCompleted 为 false。 */
	FOnShuttlegentChatViewTurnFinished OnTurnFinished;

	/** 会话索引或任一持有对话的状态或队列已变化，或换了一个对话；需要刷新会话列表与标题。 */
	FOnShuttlegentChatViewChanged OnSessionsChanged;

	/** 当前对话的运行日志新增了事件，或换了一个对话。 */
	FOnShuttlegentChatViewChanged OnRunLogChanged;

	/** 当前对话的队列已变化，或换了一个对话。 */
	FOnShuttlegentChatViewChanged OnQueueChanged;

	/** 回到某条消息后，其原文应放回输入框以便修改重发。 */
	FOnShuttlegentChatViewDraftRestored OnDraftRestored;

private:
	TSharedPtr<FShuttlegentChatModel> Model;

	/** 视图显示的对话；Initialize 之后始终有效。 */
	TSharedPtr<FShuttlegentConversation> Current;

	/** 所在页签在前台。 */
	bool bVisible = false;

public:
	/** 新建一个空对话并显示它，开始转发模型事件。 */
	void Initialize(const TSharedRef<FShuttlegentChatModel>& InModel);

	/** 停止转发并交还当前对话。 */
	void Shutdown();

	/** 所在页签转到前台或离开前台；转到前台时当前对话的“完成未看”随之清除，找不到所属对话的请求退回到当前对话。 */
	void SetVisible(bool bInVisible);

	/** 显示一个空对话；下一条提示将创建新会话。被离开的对话若处于忙碌状态则继续运行。 */
	void NewConversation();

	/** 显示一个会话：已持有的对话按原样显示，否则显示其已存储的对话记录并在运行时中恢复。 */
	void OpenSession(const FString& SessionId);

	const TArray<TSharedPtr<FShuttlegentChatEntry>>& GetEntries() const { return Current->Entries; }
	const TArray<FShuttlegentRunLogEntry>& GetRunLog() const { return Current->RunLog; }
	const FString& GetCurrentSessionId() const { return Current->SessionId; }
	bool IsStopping() const { return Current->bStopping; }
	EShuttlegentTurnOutcome GetLastOutcome() const { return Current->LastOutcome; }
	int64 GetUsedTokens() const { return Current->UsedTokens; }
	int64 GetContextSize() const { return Current->ContextSize; }
	const FShuttlegentContextBreakdown& GetContextBreakdown() const { return Current->ContextBreakdown; }
	const TArray<FShuttlegentTodoItem>& GetTodos() const { return Current->Todos; }
	bool IsCompacting() const { return Current->bCompacting; }
	bool IsPlanMode() const { return Current->bPlanMode; }
	const FShuttlegentPlanReview& GetPendingPlanReview() const { return Current->PendingPlanReview; }
	EShuttlegentWriteMode GetWriteMode() const { return Current->WriteMode; }
	bool IsResumeFailed() const { return Current->bResumeFailed; }
	bool NeedsVerify() const { return Current->bNeedsVerify; }
	FShuttlegentPromptQueue GetCurrentQueue() const { return Current->Queue; }

	EShuttlegentAgentState GetState() const;
	bool IsRuntimeLost() const;
	EShuttlegentStartIssue GetStartIssue() const;
	const FString& GetStartIssueDetail() const;
	bool IsPrompting() const;
	bool IsBusy() const;
	EShuttlegentChatStatus GetStatus() const;
	TSharedPtr<FShuttlegentChatEntry> GetPendingPermission() const;
	TSharedPtr<FShuttlegentChatEntry> FindToolCallOf(const TSharedPtr<FShuttlegentChatEntry>& Permission) const;
	bool CanCompact(FText& OutReason) const;
	bool CanSetPlanMode(FText& OutReason) const;
	bool CanAttachImages(FText& OutReason) const;
	bool CanRetry() const;
	bool CanEditHistory(FText& OutReason) const;
	bool CanUndoWrite(const FShuttlegentChatEntry& Entry, FText& OutReason) const;
	const FShuttlegentWriteRequest* GetPendingWrite() const;
	const FShuttlegentQuestion* GetPendingQuestion() const;
	bool GetConfigChoices(const FString& ConfigId, TArray<FShuttlegentConfigChoice>& OutChoices, FString& OutCurrentValue) const;
	bool IsAgentLocked() const;
	FString GetCurrentTitle() const;
	TArray<FShuttlegentSessionRecord> GetSessions(bool bArchived) const;
	bool CanArchive(const FString& SessionId) const;
	bool GetSessionActivity(const FString& SessionId, EShuttlegentChatStatus& OutStatus, int32& OutQueued) const;
	EShuttlegentAttention GetSessionAttention(const FString& SessionId) const;

	void CompactContext();
	void SetPlanMode(bool bActive);
	void AnswerPlanReview(bool bApprove, const FString& Feedback, bool bAcceptEdits);
	void SetWriteMode(EShuttlegentWriteMode Mode);
	void EnsureRuntime();
	void RetryStart();
	void RetryResume();
	void AcknowledgeUnknownWrite(int32 EntryId);
	EShuttlegentSubmitResult SubmitDraft(const FString& Text, const TArray<FShuttlegentReference>& References, bool bEnterPlanMode, const TArray<FShuttlegentPromptImage>& Images, TArray<FString>& OutInvalidPaths);
	void RemoveQueued(int32 ItemId);
	bool TakeQueued(int32 ItemId, const FString& DraftText, const TArray<FShuttlegentReference>& DraftReferences, FShuttlegentQueuedPrompt& OutItem);
	void ResumeQueue();
	void Retry();
	void AnswerWrite(int32 RequestId, bool bApprove);
	void UndoWrite(int32 EntryId);
	void UndoMemory(int32 EntryId);
	void BranchFrom(int32 EntryId);
	void RewindTo(int32 EntryId);
	void AnswerQuestion(int32 QuestionId, const FString& Answer, bool bSkip);
	bool SubmitChoice(int32 EntryId, const FString& AnswerJson, FText& OutReason);
	void Cancel();
	void Reconnect();
	void SetPermissionHandlerAttached(bool bAttached);
	void AnswerPermission(int32 EntryId, bool bAllow);
	void SetConfigValue(const FString& ConfigId, const FString& Value);
	bool RenameSession(const FString& SessionId, const FString& Title);
	bool ArchiveSession(const FString& SessionId);
	void RestoreSession(const FString& SessionId);
	bool DeleteSession(const FString& SessionId);

private:
	/** 改为显示另一个对话：先接手新对话，再交还被离开的对话，然后广播全部事件。 */
	void Show(const TSharedRef<FShuttlegentConversation>& Next);

	/** 换了一个对话后，广播当前对话的全部事件。 */
	void BroadcastAll();

	bool IsCurrent(const FShuttlegentConversation& Conversation) const { return Current.Get() == &Conversation; }

	void HandleStructureChanged(const FShuttlegentConversation& Conversation);
	void HandleContentChanged(const FShuttlegentConversation& Conversation);
	void HandleStatusChanged(const FShuttlegentConversation& Conversation);
	void HandleTurnFinished(const FShuttlegentConversation& Conversation, bool bCompleted);
	void HandleSessionsChanged();
	void HandleRunLogChanged(const FShuttlegentConversation& Conversation);
	void HandleQueueChanged(const FShuttlegentConversation& Conversation);
	void HandleDraftRestored(const FString& SessionId, const FString& Text);
	void HandleSessionClosed(const FString& SessionId);
	void HandleForkAdopted(const FString& SourceSessionId, const FString& NewSessionId);
};
