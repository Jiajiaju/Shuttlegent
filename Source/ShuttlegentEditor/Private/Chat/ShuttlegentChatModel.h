#pragma once

#include "CoreMinimal.h"

#include "Chat/FShuttlegentChatEntry.h"
#include "Chat/FShuttlegentConfigChoice.h"
#include "ShuttlegentAgentState.h"

class FJsonObject;
class FShuttlegentAgentService;

DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatStructureChanged);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatContentChanged);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentChatStatusChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentChatTurnFinished, bool /*bCompleted*/);

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

private:
	TWeakPtr<FShuttlegentAgentService> Service;
	TArray<TSharedPtr<FShuttlegentChatEntry>> Entries;
	TMap<int32, TFunction<void(bool)>> PendingPermissions;
	FString PendingPrompt;
	FString PendingResumeSessionId;
	FString LastSessionId;
	int64 UsedTokens = 0;
	int64 ContextSize = 0;
	int32 NextEntryId = 1;
	bool bRuntimeLost = false;
	bool bPermissionHandlerAttached = false;

public:
	void Initialize(const TSharedRef<FShuttlegentAgentService>& InService);
	void Shutdown();

	const TArray<TSharedPtr<FShuttlegentChatEntry>>& GetEntries() const { return Entries; }
	EShuttlegentAgentState GetState() const;
	bool IsRuntimeLost() const { return bRuntimeLost; }
	int64 GetUsedTokens() const { return UsedTokens; }
	int64 GetContextSize() const { return ContextSize; }

	/** Starts the runtime if it is not running. */
	void EnsureRuntime();

	/** Sends a prompt, starting the runtime and creating a session first when needed. */
	void SubmitPrompt(const FString& Text);

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

	/** Recent sessions for the history menu, most recent first. */
	TArray<TPair<FString, FString>> GetRecentSessions() const;

private:
	TSharedPtr<FShuttlegentChatEntry> AddEntry(EShuttlegentChatEntryKind Kind, const FString& Text);
	TSharedPtr<FShuttlegentChatEntry> FindToolCall(const FString& ToolCallId) const;
	void AddNotice(const FText& Text, bool bError);
	void ResetEntries();
	void SendPrompt(const FString& Text);
	bool ApplyUpdate(const TSharedPtr<FJsonObject>& Update);
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
