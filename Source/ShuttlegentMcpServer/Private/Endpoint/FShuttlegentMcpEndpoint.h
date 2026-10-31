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
 * 编辑器的 MCP 端点（Shuttlegent.md 第 10.1 节）：localhost 上的 Streamable HTTP，以纯 JSON 答复，不使用 SSE。
 *
 * 监听从 47800 起的第一个空闲端口，只接受携带当前 bearer token 的请求；token 在每次运行时启动时更换，并经由
 * ACP mcpServers 声明传给运行时。处理 initialize、tools/list、tools/call 与 ping；其他方法一律答复
 * "method not found"，这也同时答复了客户端的 server/discover 探测。GET 返回 405。请求在游戏线程上处理；
 * 写入工具的答复须等待用户确认。
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
	/** 收到来自 context tap 运行时插件的 shuttlegent/contextBreakdown 通知；游戏线程。 */
	TFunction<void(const FString& /*SessionId*/, const FShuttlegentContextBreakdown& /*Breakdown*/)> OnContextBreakdown;

	/** 收到来自 session control 运行时插件的 shuttlegent/runtimeControl 通知（其控制端口 URL）；游戏线程。 */
	TFunction<void(const FString& /*Url*/)> OnRuntimeControl;

	/** 开始监听；范围内没有可绑定的端口时返回 false。 */
	bool Start(const TSharedRef<FShuttlegentWriteGate>& InGate);
	void Stop();

	bool IsListening() const { return Route.IsValid(); }
	uint32 GetPort() const { return Port; }
	const FString& GetToken() const { return Token; }

	/** 端点在 localhost 上的 URL。 */
	FString GetUrl() const;

	/** 生成新的随机 token；此后携带旧 token 的请求一律拒绝。 */
	void RotateToken();

	/** 声明本端点的 ACP mcpServers 条目，附带当前 token；未监听时为空。 */
	TArray<TSharedPtr<FJsonValue>> MakeDeclaration() const;

private:
	bool HandleRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete);
	bool IsAuthorized(const FHttpServerRequest& Request) const;

	/** 处理端点能识别的通知（context tap 的上下文构成）；其他通知忽略。 */
	void HandleNotification(const FString& Method, const TSharedPtr<FJsonObject>& Params) const;

	/** 答复一个 JSON-RPC 请求；OnAnswer 接收结果对象或错误对象（tools/call 可能稍后答复）。 */
	void Dispatch(const FString& Method, const TSharedPtr<FJsonObject>& Params, TFunction<void(const TSharedRef<FJsonObject>& /*Result*/, int32 /*ErrorCode*/, const FString& /*ErrorMessage*/)> OnAnswer);
	TSharedRef<FJsonObject> MakeInitializeResult(const TSharedPtr<FJsonObject>& Params) const;
	TSharedRef<FJsonObject> MakeToolsListResult() const;
};
