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
 * 基于运行时 stdio 的 JSON-RPC 2.0，每行一条消息（ACP stdio 传输）。
 *
 * 所有回调与委托都在游戏线程触发：由 core ticker 每帧取空进程队列。
 * 运行时退出时仍未完成的请求以错误结束，调用方不会无限等待。
 */
class SHUTTLEGENTCORE_API FShuttlegentAcpConnection
{
public:
	using FResponseCallback = TFunction<void(const FShuttlegentAcpResponse&)>;

	/** 来自 Agent 的通知（无 id），例如 session/update。 */
	FOnShuttlegentAcpNotification OnNotification;

	/** 来自 Agent、需经 Respond / RespondError 答复的请求，例如 session/request_permission。 */
	FOnShuttlegentAcpAgentRequest OnAgentRequest;

	/** 运行时写到 stderr 的一行。 */
	FOnShuttlegentAcpStdErr OnStdErr;

	/** 运行时进程已退出；未完成的请求已经以失败结束。 */
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

	/** 启动运行时并开始分发消息。 */
	bool Open(const FShuttlegentRuntimeLaunchSpec& Spec, FString& OutError);

	/** 关闭 stdin，等待运行时退出（超时则强制结束），并让未完成的请求失败。 */
	void Close();

	bool IsOpen() const { return bOpen; }

	/** 发送请求；Callback 在游戏线程收到答复。返回请求 id。 */
	int64 Request(const FString& Method, const TSharedPtr<FJsonObject>& Params, FResponseCallback Callback);

	/** 发送通知（不需要答复）。 */
	void Notify(const FString& Method, const TSharedPtr<FJsonObject>& Params);

	/** 答复经 OnAgentRequest 收到的请求。 */
	void Respond(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Result);

	/** 以 JSON-RPC 错误答复经 OnAgentRequest 收到的请求。 */
	void RespondError(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message);

private:
	bool Tick(float DeltaTime);
	void Send(const TSharedRef<FJsonObject>& Message);
	void Dispatch(const FString& Line);
	void HandleClosed();
};
