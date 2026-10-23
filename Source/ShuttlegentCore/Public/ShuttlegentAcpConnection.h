#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

#include "ShuttlegentAcpResponse.h"

class FShuttlegentRuntimeProcess;
struct FShuttlegentRuntimeLaunchSpec;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShuttlegentAcpNotification, const FString& /*Method*/, const TSharedPtr<FJsonObject>& /*Params*/);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnShuttlegentAcpAgentRequest, const FString& /*Method*/, const TSharedPtr<FJsonValue>& /*Id*/, const TSharedPtr<FJsonObject>& /*Params*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentAcpStdErr, const FString& /*Line*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentAcpClosed, int32 /*ExitCode*/);

/**
 * JSON-RPC 2.0 over the runtime's stdio, one message per line (ACP stdio transport).
 *
 * All callbacks and delegates fire on the game thread: a core ticker drains the
 * process queues every frame. Requests that are pending when the runtime exits
 * complete with an error so callers never wait forever.
 */
class SHUTTLEGENTCORE_API FShuttlegentAcpConnection
{
public:
	using FResponseCallback = TFunction<void(const FShuttlegentAcpResponse&)>;

	/** Notification from the agent (no id), e.g. session/update. */
	FOnShuttlegentAcpNotification OnNotification;

	/** Request from the agent that expects a reply through Respond / RespondError, e.g. session/request_permission. */
	FOnShuttlegentAcpAgentRequest OnAgentRequest;

	/** One line the runtime wrote to stderr. */
	FOnShuttlegentAcpStdErr OnStdErr;

	/** The runtime process has exited; pending requests have already failed. */
	FOnShuttlegentAcpClosed OnClosed;

private:
	TUniquePtr<FShuttlegentRuntimeProcess> Process;
	TMap<int64, FResponseCallback> PendingRequests;
	FTSTicker::FDelegateHandle TickerHandle {};
	int64 NextRequestId = 1;
	bool bOpen = false;

public:
	FShuttlegentAcpConnection();
	~FShuttlegentAcpConnection();

	FShuttlegentAcpConnection(const FShuttlegentAcpConnection&) = delete;
	FShuttlegentAcpConnection& operator=(const FShuttlegentAcpConnection&) = delete;

	/** Launches the runtime and starts dispatching. */
	bool Open(const FShuttlegentRuntimeLaunchSpec& Spec, FString& OutError);

	/** Closes stdin, waits for the runtime to exit (terminating it after a timeout) and fails pending requests. */
	void Close();

	bool IsOpen() const { return bOpen; }

	/** Sends a request; Callback runs on the game thread with the response. Returns the request id. */
	int64 Request(const FString& Method, const TSharedPtr<FJsonObject>& Params, FResponseCallback Callback);

	/** Sends a notification (no response expected). */
	void Notify(const FString& Method, const TSharedPtr<FJsonObject>& Params);

	/** Answers a request received through OnAgentRequest. */
	void Respond(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Result);

	/** Answers a request received through OnAgentRequest with a JSON-RPC error. */
	void RespondError(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message);

private:
	bool Tick(float DeltaTime);
	void Send(const TSharedRef<FJsonObject>& Message);
	void Dispatch(const FString& Line);
	void HandleClosed();
};
