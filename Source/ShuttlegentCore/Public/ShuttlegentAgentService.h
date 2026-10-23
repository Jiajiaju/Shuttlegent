#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "ShuttlegentAgentState.h"
#include "ShuttlegentSessionStore.h"

class FShuttlegentAcpConnection;
struct FShuttlegentAcpResponse;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentStateChanged, EShuttlegentAgentState /*NewState*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentSessionReady, const FString& /*SessionId*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShuttlegentSessionUpdate, const FString& /*SessionId*/, const TSharedPtr<FJsonObject>& /*Update*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnShuttlegentTurnFinished, const FString& /*SessionId*/, const FString& /*StopReason*/, const FString& /*ErrorMessage*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShuttlegentRuntimeExited, int32 /*ExitCode*/, bool /*bExpected*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentError, const FString& /*Message*/);
DECLARE_MULTICAST_DELEGATE(FOnShuttlegentConfigOptionsChanged);

/** Answers one permission request: call Respond(true) to allow once, Respond(false) to reject. */
DECLARE_DELEGATE_TwoParams(FShuttlegentPermissionHandler, const TSharedPtr<FJsonObject>& /*Params*/, TFunction<void(bool /*bAllow*/)> /*Respond*/);

/**
 * Owns the agent runtime and one active ACP session.
 *
 * Lifecycle: Start -> (NewSession | ResumeSession) -> Prompt / Cancel ... -> Stop.
 * Everything runs on the game thread. Updates of a turn that has ended (cancelled or
 * finished) are dropped, so late chunks never leak into the next turn.
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

	/** GetConfigOptions changed (session created or resumed, or an option was set). */
	FOnShuttlegentConfigOptionsChanged OnConfigOptionsChanged;

	/** When bound, permission requests go here; otherwise UShuttlegentSettings::PermissionPolicy answers them. */
	FShuttlegentPermissionHandler PermissionHandler;

private:
	TUniquePtr<FShuttlegentAcpConnection> Connection;
	FShuttlegentSessionStore SessionStore;
	EShuttlegentAgentState State = EShuttlegentAgentState::Stopped;
	FString ActiveSessionId;
	TArray<TSharedPtr<FJsonValue>> ConfigOptions;
	uint64 TurnGeneration = 0;
	bool bStopRequested = false;

public:
	FShuttlegentAgentService();
	~FShuttlegentAgentService();

	/** Launches the runtime and sends initialize. A fresh runtime has no session until NewSession or ResumeSession. */
	bool Start(FString& OutError);

	/** Shuts the runtime down. The last session can be resumed after the next Start. */
	void Stop();

	/** Creates a session whose cwd is the project directory. */
	bool NewSession(FString& OutError);

	/** Reattaches to a persisted session; its model context is restored by the runtime. */
	bool ResumeSession(const FString& SessionId, FString& OutError);

	/** Sends one text prompt to the active session. */
	bool Prompt(const FString& Text, FString& OutError);

	/** Cancels the in-flight prompt, if any. */
	void Cancel();

	/** Changes a session config option such as "model" or "reasoning_effort". */
	bool SetConfigOption(const FString& ConfigId, const FString& Value, FString& OutError);

	/** Closes the active session; it stays resumable. */
	void CloseSession();

	EShuttlegentAgentState GetState() const { return State; }
	const FString& GetActiveSessionId() const { return ActiveSessionId; }

	/** Config options from the last session/new, session/resume or set_config_option response. */
	const TArray<TSharedPtr<FJsonValue>>& GetConfigOptions() const { return ConfigOptions; }

	FShuttlegentSessionStore& GetSessionStore() { return SessionStore; }

private:
	void SetState(EShuttlegentAgentState NewState);
	TSharedRef<FJsonObject> MakeSessionParams(const FString& SessionId) const;
	void ApplySessionResult(const FString& SessionId, const TSharedPtr<FJsonObject>& Result, bool bCreated);
	/** Current "model" config value and its display name (the value itself when the choice has no name). */
	void GetCurrentModel(FString& OutValue, FString& OutName) const;
	void HandleNotification(const FString& Method, const TSharedPtr<FJsonObject>& Params);
	void HandleAgentRequest(const FString& Method, const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Params);
	void RespondPermission(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Params, bool bAllow);
	void HandleRuntimeClosed(int32 ExitCode);
};
