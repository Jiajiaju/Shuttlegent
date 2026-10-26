#pragma once

#include "CoreMinimal.h"

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
#include "WriteGate/FShuttlegentWriteRequest.h"

class FJsonObject;
class FJsonValue;
class FShuttlegentAgentService;
class FShuttlegentWriteGate;
enum class EShuttlegentWriteDecision : uint8;

DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatStructureChanged);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatContentChanged);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatStatusChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentChatTurnFinished, bool /*bCompleted*/);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatSessionsChanged);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatRunLogChanged);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatQueueChanged);

/**
 * Editor-side conversation state between FShuttlegentAgentService and the chat panel.
 *
 * Lives in the editor module, so conversations survive closing and reopening the panel.
 * Holds one FShuttlegentConversation per session the user has running, queued or open;
 * the panel shows the current one, and the others keep running and receiving updates in
 * the background (Shuttlegent.md section 14.3.7). Leaving a conversation that is idle
 * closes its session and drops it; opening it again replays its transcript.
 *
 * Folds session/update chunks into entries (one per assistant message id, one per tool
 * call) and queues a conversation's first prompt until the runtime and its session are
 * ready. Drafts sent while a turn runs go to that conversation's FIFO queue and are sent
 * one by one when turns end normally; a stop, a rejected permission, a failed turn or a
 * lost runtime pauses the queue until the user continues it (Shuttlegent UI interaction
 * spec section 4.3). Events are about the current conversation; a background one only
 * raises OnSessionsChanged when its status or queue changes. Game thread only.
 */
class FShuttlegentChatModel : public TSharedFromThis<FShuttlegentChatModel>
{
public:
	/** Entries of the current conversation were added or the list was replaced; rebuild rows. */
	FOnShuttlegentChatStructureChanged OnStructureChanged;

	/** Text or status of the current conversation's entries changed; rows update through attributes. */
	FOnShuttlegentChatContentChanged OnContentChanged;

	/** Runtime state, config options, usage or the lost-runtime banner of the current conversation changed. */
	FOnShuttlegentChatStatusChanged OnStatusChanged;

	/** A turn of the current conversation ended; bCompleted is false when it was cancelled, failed or stopped for another reason. */
	FOnShuttlegentChatTurnFinished OnTurnFinished;

	/** The session index, the current session, or the status or queue of any held conversation changed; refresh the session list and the title. */
	FOnShuttlegentChatSessionsChanged OnSessionsChanged;

	/** An event was added to the current conversation's run log, or another conversation became current. */
	FOnShuttlegentChatRunLogChanged OnRunLogChanged;

	/** The current conversation's queue changed: items added, sent, removed or taken back, paused or continued. */
	FOnShuttlegentChatQueueChanged OnQueueChanged;

private:
	TWeakPtr<FShuttlegentAgentService> Service;

	/** Conversations held in memory; always contains Current. */
	TArray<TSharedPtr<FShuttlegentConversation>> Conversations;
	TSharedPtr<FShuttlegentConversation> Current;

	/** Conversations whose session/new is in flight, oldest first; each answer goes to the oldest. */
	TArray<TWeakPtr<FShuttlegentConversation>> AwaitingNewSession;

	/** Config options of the last session that reported them; shown for a conversation whose session is not open. */
	TArray<TSharedPtr<FJsonValue>> LastConfigOptions;

	int32 NextEntryId = 1;
	int32 NextQueueId = 1;
	bool bRuntimeLost = false;

	/** Why the runtime cannot start or serve prompts; shown as the start guide card until a retry or a ready runtime clears it. */
	EShuttlegentStartIssue StartIssue = EShuttlegentStartIssue::None;

	/** Reason of a start that failed for another reason (English, from the runtime or the launcher); no key values. */
	FString StartIssueDetail;

	/** A retry stopped the running runtime; it starts again once the old one has exited. */
	bool bRestartPending = false;
	bool bPermissionHandlerAttached = false;

	/** Editor write gate (ShuttlegentMcpServer); the panel confirms its requests and shows its results. */
	TWeakPtr<FShuttlegentWriteGate> WriteGate;

	/** Conversation the open write requests and their result belong to: the current one when the first of them arrived. */
	TWeakPtr<FShuttlegentConversation> WriteOwner;

	/** A confirmed write is running; cleared when its result arrives or the runtime is lost. */
	bool bWriting = false;

	/** Write requests waiting for confirmation, by request id, and the one confirmed last: what a write unknown entry lists. */
	TMap<int32, FShuttlegentWriteRequest> OpenWriteRequests;
	FShuttlegentWriteRequest ApprovedWrite;

	/** Undo buffer listener, added once the editor exists, so write results can tell whether they are still undoable. */
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

	/** Session shown in the panel; empty for a new conversation. */
	const FString& GetCurrentSessionId() const { return Current->SessionId; }

	/** The current conversation's session runs a prompt. */
	bool IsPrompting() const;

	/** Status of the current conversation for the header. */
	EShuttlegentChatStatus GetStatus() const;

	/** Status and queued prompt count of a session held in memory, for the session list; false for a session that is not held. */
	bool GetSessionActivity(const FString& SessionId, EShuttlegentChatStatus& OutStatus, int32& OutQueued) const;

	/** Oldest runtime permission request of the current conversation still waiting for an answer, or nullptr. */
	TSharedPtr<FShuttlegentChatEntry> GetPendingPermission() const;

	/** Tool call entry a permission request of the current conversation is about, or nullptr. */
	TSharedPtr<FShuttlegentChatEntry> FindToolCallOf(const TSharedPtr<FShuttlegentChatEntry>& Permission) const;
	int64 GetUsedTokens() const { return Current->UsedTokens; }
	int64 GetContextSize() const { return Current->ContextSize; }

	/** Starts the runtime if it is not running; a failure becomes the start issue. */
	void EnsureRuntime();

	/** Starts the runtime after editor startup when UShuttlegentSettings::bPrewarmRuntime is on and nothing blocks it. */
	void Prewarm();

	EShuttlegentStartIssue GetStartIssue() const { return StartIssue; }
	const FString& GetStartIssueDetail() const { return StartIssueDetail; }

	/** Clears the start issue and starts the runtime again (restarting a running one, so a fixed key is read). */
	void RetryStart();

	/** The current conversation's last session/resume failed. */
	bool IsResumeFailed() const { return Current->bResumeFailed; }

	/** Resumes the current conversation's session again after a failed resume. */
	void RetryResume();

	/** The current conversation has a write result the user has not verified; sending is disabled. */
	bool NeedsVerify() const { return Current->bNeedsVerify; }

	/** Marks an unknown write result of the current conversation as verified by the user. */
	void AcknowledgeUnknownWrite(int32 EntryId);

	/**
	 * Sends a draft with its references in the current conversation, or queues it while a turn runs there. Every
	 * reference must still exist; the ones that do not are returned and nothing is sent or queued.
	 */
	EShuttlegentSubmitResult SubmitDraft(const FString& Text, const TArray<FShuttlegentReference>& References, TArray<FString>& OutInvalidPaths);

	/** In the current conversation a turn runs, a stop is pending, or the runtime or the session is still coming up: new drafts are queued. */
	bool IsBusy() const;

	/** Queue of the current conversation. */
	FShuttlegentPromptQueue GetCurrentQueue() const;

	void RemoveQueued(int32 ItemId);

	/**
	 * Takes a queued item back into the draft. When the draft is not empty it takes the item's place in the queue,
	 * so nothing the user wrote is lost. Returns false when the item is gone.
	 */
	bool TakeQueued(int32 ItemId, const FString& DraftText, const TArray<FShuttlegentReference>& DraftReferences, FShuttlegentQueuedPrompt& OutItem);

	/** Continues the current conversation's paused queue and sends its next item when nothing runs there. */
	void ResumeQueue();

	/** The current conversation's failed turn can be sent again. */
	bool CanRetry() const;

	/** Sends the failed turn's prompt again; the queue stays paused. */
	void Retry();

	/** Oldest editor write waiting for confirmation when it belongs to the current conversation, or nullptr. */
	const FShuttlegentWriteRequest* GetPendingWrite() const;

	/** Confirms or refuses an editor write; a second answer to the same request does nothing. */
	void AnswerWrite(int32 RequestId, bool bApprove);

	/** Whether a write result can still be undone directly, and why not when it cannot (Shuttlegent UI interaction spec section 5.4). */
	bool CanUndoWrite(const FShuttlegentChatEntry& Entry, FText& OutReason) const;

	/** Undoes a write result's transaction when it is still on top of the editor's undo buffer. */
	void UndoWrite(int32 EntryId);

	/** Sends one cancellation for the current conversation's running turn; later calls wait for the turn to end. */
	void Cancel();

	/** Shows an empty conversation; the next prompt creates a new session. The conversation left keeps running if it is busy. */
	void NewConversation();

	/** Shows a session: a held conversation as it is, otherwise its stored transcript, resumed in the runtime. */
	void OpenSession(const FString& SessionId);

	/** Restarts a lost runtime and resumes the current session. */
	void Reconnect();

	/** While attached (panel open), runtime permission requests become panel entries of the conversation they name. */
	void SetPermissionHandlerAttached(bool bAttached);

	void AnswerPermission(int32 EntryId, bool bAllow);

	/** Choices and current value of a config option ("model", "reasoning_effort") of the current conversation. */
	bool GetConfigChoices(const FString& ConfigId, TArray<FShuttlegentConfigChoice>& OutChoices, FString& OutCurrentValue) const;

	/** Sets a config option of the current conversation; before its session is open, the value waits for it. */
	void SetConfigValue(const FString& ConfigId, const FString& Value);

	/** Sessions of the index, most recently updated first: the active list, or the archived ones. */
	TArray<FShuttlegentSessionRecord> GetSessions(bool bArchived) const;

	/** Title of the current session; empty for a new conversation or an untitled session. */
	FString GetCurrentTitle() const;

	/** A running, permission-waiting or write-waiting session cannot be archived. */
	bool CanArchive(const FString& SessionId) const;

	/** Renames a stored session; false when the name is empty after trimming, too long, or the session is unknown. */
	bool RenameSession(const FString& SessionId, const FString& Title);

	/** Hides a session from the list and drops its conversation; archiving the current session starts a new conversation. */
	bool ArchiveSession(const FString& SessionId);

	void RestoreSession(const FString& SessionId);

	/** Removes a session's record and transcript for good, under the same rule as archiving; deleting the current session starts a new conversation. */
	bool DeleteSession(const FString& SessionId);

private:
	bool IsCurrent(const FShuttlegentConversation& Conversation) const { return Current.Get() == &Conversation; }
	TSharedPtr<FShuttlegentConversation> FindConversation(const FString& SessionId) const;
	bool IsPrompting(const FShuttlegentConversation& Conversation) const;
	bool IsBusy(const FShuttlegentConversation& Conversation) const;
	EShuttlegentChatStatus GetStatus(const FShuttlegentConversation& Conversation) const;
	TSharedPtr<FShuttlegentChatEntry> GetPendingPermission(const FShuttlegentConversation& Conversation) const;

	/** Whether the open write requests and their result belong to the conversation (the current one when no request is open). */
	bool OwnsWrite(const FShuttlegentConversation& Conversation) const;

	/** Leaves the current conversation: an idle one is dropped and its session closed, a busy one keeps running. */
	void LeaveCurrent();
	bool CanDrop(const FShuttlegentConversation& Conversation) const;
	void DropConversation(FShuttlegentConversation& Conversation);

	/** Every event of the current conversation, after another one became current. */
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

	/** Prompts that waited for the runtime or a session go back to the front of their queue, paused, instead of being lost. */
	void ParkPendingPrompts();
	void ParkPendingPrompt(FShuttlegentConversation& Conversation);

	/** Sends a prompt, starting the runtime and creating or resuming the conversation's session first when needed. */
	void SubmitPrompt(FShuttlegentConversation& Conversation, const FString& Text);
	void SendPrompt(FShuttlegentConversation& Conversation, const FString& Text);

	/** Creates (no session id yet) or resumes the conversation's session, or marks it for when the runtime is ready. */
	void RequestSession(FShuttlegentConversation& Conversation);

	bool GetConfigChoices(const FShuttlegentConversation& Conversation, const FString& ConfigId, TArray<FShuttlegentConfigChoice>& OutChoices, FString& OutCurrentValue) const;
	void SetConfigValue(FShuttlegentConversation& Conversation, const FString& ConfigId, const FString& Value);

	/** Frozen with the options selected now, so a later change of model does not rewrite what was queued. */
	FShuttlegentQueuedPrompt MakeQueued(const FString& Text, const TArray<FShuttlegentReference>& References);
	void PauseQueue(FShuttlegentConversation& Conversation);

	/** Sends the conversation's next queued item when its queue runs and nothing else does there. */
	void SendNextQueued(FShuttlegentConversation& Conversation);

	/** Folds one session/update into entries; At is when it arrived (live) or was recorded (replay). Returns true when entries were added. */
	bool ApplyUpdate(FShuttlegentConversation& Conversation, const TSharedPtr<FJsonObject>& Update, const FDateTime& At);
	void ApplyTurnEnd(FShuttlegentConversation& Conversation, const FString& StopReason, const FString& ErrorMessage);
	void ReplayTranscript(FShuttlegentConversation& Conversation);

	void HandleStateChanged(EShuttlegentAgentState NewState);
	void HandleSessionReady(const FString& SessionId);
	void HandleSessionUpdate(const FString& SessionId, const TSharedPtr<FJsonObject>& Update);
	void HandleTurnFinished(const FString& SessionId, const FString& StopReason, const FString& ErrorMessage);
	void HandleRuntimeExited(int32 ExitCode, bool bExpected);
	void HandleError(const FString& SessionId, const FString& Message);
	void HandleConfigOptionsChanged(const FString& SessionId);
	void HandlePermissionRequest(const TSharedPtr<FJsonObject>& Params, TFunction<void(bool)> Respond);

	void HandleWriteRequested(const FShuttlegentWriteRequest& Request);
	void HandleWriteClosed(int32 RequestId, EShuttlegentWriteDecision Decision);
	void HandleWriteFinished(const FShuttlegentWriteResult& Result);
	void HandleUndoBufferChanged();
	void EnsureUndoListener();
};
