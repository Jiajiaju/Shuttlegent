#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "ShuttlegentAgentState.h"
#include "ShuttlegentStartIssue.h"
#include "ShuttlegentSessionStore.h"

class FShuttlegentAcpConnection;
struct FShuttlegentAcpResponse;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentStateChanged, EShuttlegentAgentState /*NewState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentSessionReady, const FString& /*SessionId*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShuttlegentSessionUpdate, const FString& /*SessionId*/, const TSharedPtr<FJsonObject>& /*Update*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnShuttlegentTurnFinished, const FString& /*SessionId*/, const FString& /*StopReason*/, const FString& /*ErrorMessage*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShuttlegentRuntimeExited, int32 /*ExitCode*/, bool /*bExpected*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShuttlegentError, const FString& /*SessionId*/, const FString& /*Message*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentConfigOptionsChanged, const FString& /*SessionId*/);

/** Answers one permission request: call Respond(true) to allow once, Respond(false) to reject. */
DECLARE_DELEGATE_TwoParams(FShuttlegentPermissionHandler, const TSharedPtr<FJsonObject>& /*Params*/, TFunction<void(bool /*bAllow*/)> /*Respond*/);

/** One session the runtime has open: whether it runs a prompt, its turn generation and its config options. */
struct FShuttlegentOpenSession
{
	bool bPrompting = false;

	/** Bumped by every prompt and close; a prompt response of an older generation is dropped. */
	uint64 TurnGeneration = 0;

	/** From the last session/new, session/resume or set_config_option response of this session. */
	TArray<TSharedPtr<FJsonValue>> ConfigOptions;
};

/**
 * Owns the agent runtime and the ACP sessions it has open.
 *
 * Lifecycle: Start -> (NewSession | ResumeSession)* -> Prompt / Cancel per session ... -> Stop.
 * Several sessions can be open and prompted at the same time; the runtime schedules each
 * session on its own (Shuttlegent.md section 14.3.7). Everything runs on the game thread.
 * Updates of a session's turn that has ended (cancelled or finished) are dropped, so late
 * chunks never leak into its next turn. Every event names its session.
 */
class SHUTTLEGENTCORE_API FShuttlegentAgentService : public TSharedFromThis<FShuttlegentAgentService>
{
public:
	FOnShuttlegentStateChanged OnStateChanged;
	FOnShuttlegentSessionReady OnSessionReady;
	FOnShuttlegentSessionUpdate OnSessionUpdate;
	FOnShuttlegentTurnFinished OnTurnFinished;
	FOnShuttlegentRuntimeExited OnRuntimeExited;
	FOnShuttlegentError OnError;

	/** A session's config options changed (session created or resumed, or an option was set). */
	FOnShuttlegentConfigOptionsChanged OnConfigOptionsChanged;

	/** When bound, permission requests go here (their params name the session); otherwise UShuttlegentSettings::PermissionPolicy answers them. */
	FShuttlegentPermissionHandler PermissionHandler;

	/** ACP mcpServers entries declared with every session/new and session/resume; ShuttlegentMcpServer declares the editor endpoint here. Empty when unbound. */
	TFunction<TArray<TSharedPtr<FJsonValue>>()> McpServersProvider;

private:
	TUniquePtr<FShuttlegentAcpConnection> Connection;
	FShuttlegentSessionStore SessionStore;
	EShuttlegentAgentState State = EShuttlegentAgentState::Stopped;
	TMap<FString, FShuttlegentOpenSession> OpenSessions;
	bool bStopRequested = false;
	EShuttlegentStartIssue StartIssue = EShuttlegentStartIssue::None;

public:
	FShuttlegentAgentService();
	~FShuttlegentAgentService();

	/** Launches the runtime and sends initialize. A fresh runtime has no session until NewSession or ResumeSession. On failure GetStartIssue tells the kind. */
	bool Start(FString& OutError);

	/** Kind of the last Start failure (also a failed initialize); None after a successful start. */
	EShuttlegentStartIssue GetStartIssue() const { return StartIssue; }

	/** Kind of a runtime error message: a missing or rejected provider key, or Other. */
	static EShuttlegentStartIssue ClassifyError(const FString& Message);

	/** Shuts the runtime down. Its sessions can be resumed after the next Start. */
	void Stop();

	/** Creates a session whose cwd is the project directory; OnSessionReady names it. Other open sessions are not touched. */
	bool NewSession(FString& OutError);

	/** Reattaches to a persisted session; its model context is restored by the runtime. Other open sessions are not touched. */
	bool ResumeSession(const FString& SessionId, FString& OutError);

	/** Sends one text prompt to an open session that runs no prompt. */
	bool Prompt(const FString& SessionId, const FString& Text, FString& OutError);

	/** Cancels the session's in-flight prompt, if any. */
	void Cancel(const FString& SessionId);

	/** Changes a config option of an open, idle session, such as "model" or "reasoning_effort". */
	bool SetConfigOption(const FString& SessionId, const FString& ConfigId, const FString& Value, FString& OutError);

	/** Closes an open session; it stays resumable. */
	void CloseSession(const FString& SessionId);

	EShuttlegentAgentState GetState() const { return State; }
	bool IsSessionOpen(const FString& SessionId) const { return OpenSessions.Contains(SessionId); }
	bool IsPrompting(const FString& SessionId) const;

	/** Config options of an open session; empty for a session that is not open. */
	TArray<TSharedPtr<FJsonValue>> GetConfigOptions(const FString& SessionId) const;

	FShuttlegentSessionStore& GetSessionStore() { return SessionStore; }

private:
	void SetState(EShuttlegentAgentState NewState);
	TSharedRef<FJsonObject> MakeSessionParams(const FString& SessionId) const;
	void ApplySessionResult(const FString& SessionId, const TSharedPtr<FJsonObject>& Result, bool bCreated);
	/** Current "model" value among config options and its display name (the value itself when the choice has no name). */
	static void GetCurrentModel(const TArray<TSharedPtr<FJsonValue>>& ConfigOptions, FString& OutValue, FString& OutName);
	void HandleNotification(const FString& Method, const TSharedPtr<FJsonObject>& Params);
	void HandleAgentRequest(const FString& Method, const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Params);
	void RespondPermission(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Params, bool bAllow);
	void HandleRuntimeClosed(int32 ExitCode);
};
