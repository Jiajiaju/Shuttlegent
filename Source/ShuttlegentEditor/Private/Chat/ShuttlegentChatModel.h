#pragma once

#include "CoreMinimal.h"

#include "Chat/EShuttlegentChatStatus.h"
#include "Chat/EShuttlegentTurnOutcome.h"
#include "Chat/FShuttlegentChatEntry.h"
#include "Chat/FShuttlegentConfigChoice.h"
#include "Chat/FShuttlegentRunLogEntry.h"
#include "ShuttlegentAgentState.h"
#include "ShuttlegentSessionRecord.h"

class FJsonObject;
class FShuttlegentAgentService;

DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatStructureChanged);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatContentChanged);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatStatusChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentChatTurnFinished, bool /*bCompleted*/);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatSessionsChanged);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatRunLogChanged);

/**
 * Editor-side conversation state between FShuttlegentAgentService and the chat panel.
 *
 * Lives in the editor module, so the conversation survives closing and reopening the
 * panel. Folds session/update chunks into entries (one per assistant message id, one
 * per tool call), replays transcripts of reopened sessions, and queues the first prompt
 * until the runtime and a session are ready. Game thread only.
 */
class FShuttlegentChatModel : public TSharedFromThis<FShuttlegentChatModel>
{
public:
	/** Entries were added or the list was reset; rebuild rows. */
	FOnShuttlegentChatStructureChanged OnStructureChanged;

	/** Text or status of existing entries changed; rows update through attributes. */
	FOnShuttlegentChatContentChanged OnContentChanged;

	/** Runtime state, config options, usage or the lost-runtime banner changed. */
	FOnShuttlegentChatStatusChanged OnStatusChanged;

	/** A turn ended; bCompleted is false when it was cancelled, failed or stopped for another reason. */
	FOnShuttlegentChatTurnFinished OnTurnFinished;

	/** The session index or the current session changed; refresh the session list and the title. */
	FOnShuttlegentChatSessionsChanged OnSessionsChanged;

	/** An event was added to the run log, or the log was reset. */
	FOnShuttlegentChatRunLogChanged OnRunLogChanged;

private:
	TWeakPtr<FShuttlegentAgentService> Service;
	TArray<TSharedPtr<FShuttlegentChatEntry>> Entries;
	TMap<int32, TFunction<void(bool)>> PendingPermissions;
	TArray<FShuttlegentRunLogEntry> RunLog;
	FString PendingPrompt;
	FString PendingResumeSessionId;
	FString LastSessionId;
	int64 UsedTokens = 0;
	int64 ContextSize = 0;
	int32 NextEntryId = 1;
	EShuttlegentTurnOutcome LastOutcome = EShuttlegentTurnOutcome::None;
	bool bRuntimeLost = false;
	bool bPermissionHandlerAttached = false;

	/** Stop was sent for the running turn; cleared when the turn ends. */
	bool bStopping = false;

	/** A session/resume is in flight or waits for the runtime to start. */
	bool bResuming = false;

public:
	void Initialize(const TSharedRef<FShuttlegentAgentService>& InService);
	void Shutdown();

	const TArray<TSharedPtr<FShuttlegentChatEntry>>& GetEntries() const { return Entries; }
	EShuttlegentAgentState GetState() const;
	bool IsRuntimeLost() const { return bRuntimeLost; }
	bool IsStopping() const { return bStopping; }
	EShuttlegentTurnOutcome GetLastOutcome() const { return LastOutcome; }
	const TArray<FShuttlegentRunLogEntry>& GetRunLog() const { return RunLog; }

	/** Session shown in the panel: the active one, or the last one opened; empty for a new conversation. */
	const FString& GetCurrentSessionId() const { return LastSessionId; }

	/** Status of the current conversation for the header and the session list. */
	EShuttlegentChatStatus GetStatus() const;

	/** Oldest runtime permission request still waiting for an answer, or nullptr. */
	TSharedPtr<FShuttlegentChatEntry> GetPendingPermission() const;

	/** Tool call entry a permission request is about, or nullptr. */
	TSharedPtr<FShuttlegentChatEntry> FindToolCallOf(const TSharedPtr<FShuttlegentChatEntry>& Permission) const;
	int64 GetUsedTokens() const { return UsedTokens; }
	int64 GetContextSize() const { return ContextSize; }

	/** Starts the runtime if it is not running. */
	void EnsureRuntime();

	/** Sends a prompt, starting the runtime and creating a session first when needed. */
	void SubmitPrompt(const FString& Text);

	/** Sends one cancellation for the running turn; later calls wait for the turn to end. */
	void Cancel();

	/** Clears the conversation; the next prompt creates a new session. */
	void NewConversation();

	/** Shows a stored session's transcript and resumes it in the runtime. */
	void OpenSession(const FString& SessionId);

	/** Restarts a lost runtime and resumes the last session. */
	void Reconnect();

	/** While attached (panel open), runtime permission requests become panel entries. */
	void SetPermissionHandlerAttached(bool bAttached);

	void AnswerPermission(int32 EntryId, bool bAllow);

	/** Choices and current value of a session config option ("model", "reasoning_effort"). */
	bool GetConfigChoices(const FString& ConfigId, TArray<FShuttlegentConfigChoice>& OutChoices, FString& OutCurrentValue) const;

	void SetConfigValue(const FString& ConfigId, const FString& Value);

	/** Sessions of the index, most recently updated first: the active list, or the archived ones. */
	TArray<FShuttlegentSessionRecord> GetSessions(bool bArchived) const;

	/** Title of the current session; empty for a new conversation or an untitled session. */
	FString GetCurrentTitle() const;

	/** A running or permission-waiting session cannot be archived. */
	bool CanArchive(const FString& SessionId) const;

	/** Renames a stored session; false when the name is empty after trimming, too long, or the session is unknown. */
	bool RenameSession(const FString& SessionId, const FString& Title);

	/** Hides a session from the list; archiving the current session starts a new conversation. */
	bool ArchiveSession(const FString& SessionId);

	void RestoreSession(const FString& SessionId);

private:
	TSharedPtr<FShuttlegentChatEntry> AddEntry(EShuttlegentChatEntryKind Kind, const FString& Text);
	TSharedPtr<FShuttlegentChatEntry> FindToolCall(const FString& ToolCallId) const;
	void AddNotice(const FText& Text, bool bError);
	void ResetEntries();
	void RejectPendingPermissions();
	void AddRunLog(const FText& Text, bool bWarning);
	void ResetRunLog();
	void SendPrompt(const FString& Text);
	/** Folds one session/update into entries; At is when it arrived (live) or was recorded (replay). Returns true when entries were added. */
	bool ApplyUpdate(const TSharedPtr<FJsonObject>& Update, const FDateTime& At);
	void ApplyTurnEnd(const FString& StopReason, const FString& ErrorMessage);
	void ReplayTranscript(const FString& SessionId);

	void HandleStateChanged(EShuttlegentAgentState NewState);
	void HandleSessionReady(const FString& SessionId);
	void HandleSessionUpdate(const FString& SessionId, const TSharedPtr<FJsonObject>& Update);
	void HandleTurnFinished(const FString& SessionId, const FString& StopReason, const FString& ErrorMessage);
	void HandleRuntimeExited(int32 ExitCode, bool bExpected);
	void HandleError(const FString& Message);
	void HandleConfigOptionsChanged();
	void HandlePermissionRequest(const TSharedPtr<FJsonObject>& Params, TFunction<void(bool)> Respond);
};
