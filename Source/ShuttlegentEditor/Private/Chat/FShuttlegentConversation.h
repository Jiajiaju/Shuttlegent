#pragma once

#include "CoreMinimal.h"

#include "Chat/EShuttlegentTurnOutcome.h"
#include "Chat/FShuttlegentChatEntry.h"
#include "Chat/FShuttlegentPromptQueue.h"
#include "Chat/FShuttlegentRunLogEntry.h"

/**
 * One conversation the panel holds: a session's entries, run log, queue and turn state.
 * Several conversations run at the same time; the panel shows one (Shuttlegent.md section 14.3.7).
 */
struct FShuttlegentConversation : public TSharedFromThis<FShuttlegentConversation>
{
	/** Runtime session; empty for a new conversation until session/new answers. */
	FString SessionId;

	TArray<TSharedPtr<FShuttlegentChatEntry>> Entries;

	/** This conversation's permission requests still waiting for an answer, by entry id. */
	TMap<int32, TFunction<void(bool)>> PendingPermissions;

	TArray<FShuttlegentRunLogEntry> RunLog;
	FShuttlegentPromptQueue Queue;

	/** Prompt waiting for the runtime or the session to come up. */
	FString PendingPrompt;

	/** The last prompt sent, as sent (with its reference block); a failed turn retries it. */
	FString LastSentPrompt;

	/** Config values chosen while the session was not open, by config id; applied once it is. */
	TMap<FString, FString> PendingConfig;

	int64 UsedTokens = 0;
	int64 ContextSize = 0;
	EShuttlegentTurnOutcome LastOutcome = EShuttlegentTurnOutcome::None;

	/** Stop was sent for the running turn; cleared when the turn ends. */
	bool bStopping = false;

	/** A session/resume is in flight or waits for the runtime to start. */
	bool bResuming = false;

	/** Needs session/new or session/resume once the runtime is ready. */
	bool bWantsSession = false;

	/** The last session/resume failed; the local record is kept and a new session would not carry its context. */
	bool bResumeFailed = false;

	/** A write result is unknown and its objects are not verified yet; sending is disabled until they are. */
	bool bNeedsVerify = false;
};
