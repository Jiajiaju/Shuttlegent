#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "HttpResultCallback.h"
#include "HttpRouteHandle.h"

class FShuttlegentWriteGate;
class IHttpRouter;
struct FHttpServerRequest;
struct FShuttlegentContextBreakdown;

/**
 * The editor's MCP endpoint (Shuttlegent.md section 10.1): Streamable HTTP on localhost, plain JSON answers, no SSE.
 *
 * Listens on the first free port from 47800 and accepts only requests that carry the current bearer token; the token
 * changes every time the runtime starts and reaches it through the ACP mcpServers declaration. Handles initialize,
 * tools/list, tools/call and ping; any other method gets "method not found", which also answers the client's
 * server/discover probe. GET gets 405. Requests are handled on the game thread; a write tool's answer waits for the
 * user's confirmation.
 */
class FShuttlegentMcpEndpoint : public TSharedFromThis<FShuttlegentMcpEndpoint>
{
private:
	TWeakPtr<FShuttlegentWriteGate> Gate;
	TSharedPtr<IHttpRouter> Router;
	FHttpRouteHandle Route;
	uint32 Port = 0;
	FString Token;

public:
	/** A shuttlegent/contextBreakdown notification arrived from the context tap runtime plugin; game thread. */
	TFunction<void(const FString& /*SessionId*/, const FShuttlegentContextBreakdown& /*Breakdown*/)> OnContextBreakdown;

	/** Starts listening; false when no port of the range could be bound. */
	bool Start(const TSharedRef<FShuttlegentWriteGate>& InGate);
	void Stop();

	bool IsListening() const { return Route.IsValid(); }
	uint32 GetPort() const { return Port; }
	const FString& GetToken() const { return Token; }

	/** The endpoint's URL on localhost. */
	FString GetUrl() const;

	/** A new random token; requests with the old one are refused from now on. */
	void RotateToken();

	/** The ACP mcpServers entries that declare this endpoint, with the current token; empty while not listening. */
	TArray<TSharedPtr<FJsonValue>> MakeDeclaration() const;

private:
	bool HandleRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool IsAuthorized(const FHttpServerRequest& Request) const;

	/** Handles a notification the endpoint understands (the context tap's breakdown); others are ignored. */
	void HandleNotification(const FString& Method, const TSharedPtr<FJsonObject>& Params) const;

	/** Answers one JSON-RPC request; OnAnswer gets the result or error object (tools/call may answer later). */
	void Dispatch(const FString& Method, const TSharedPtr<FJsonObject>& Params, TFunction<void(const TSharedRef<FJsonObject>& /*Result*/, int32 /*ErrorCode*/, const FString& /*ErrorMessage*/)> OnAnswer);
	TSharedRef<FJsonObject> MakeInitializeResult(const TSharedPtr<FJsonObject>& Params) const;
	TSharedRef<FJsonObject> MakeToolsListResult() const;
};
