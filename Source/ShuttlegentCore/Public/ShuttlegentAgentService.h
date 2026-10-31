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

/** 答复一个权限请求：调用 Respond(true) 允许一次，Respond(false) 拒绝。 */
DECLARE_DELEGATE_TwoParams(FShuttlegentPermissionHandler, const TSharedPtr<FJsonObject>& /*Params*/, TFunction<void(bool /*bAllow*/)> /*Respond*/);

/** 运行时打开的一个会话：是否正在执行提问、回合代次与配置选项。 */
struct FShuttlegentOpenSession
{
	bool bPrompting = false;

	/** 每次提问与关闭时递增；旧代次的提问答复会被丢弃。 */
	uint64 TurnGeneration = 0;

	/** 取自该会话最近一次 session/new、session/resume 或 set_config_option 的答复。 */
	TArray<TSharedPtr<FJsonValue>> ConfigOptions;
};

/**
 * 持有 Agent 运行时及其打开的 ACP 会话。
 *
 * 生命周期：Start -> (NewSession | ResumeSession)* -> 各会话 Prompt / Cancel ... -> Stop。
 * 可以同时打开并提问多个会话，运行时分别调度各会话（Shuttlegent.md 第 14.3.7 节）。全部在游戏线程运行。
 * 会话中已结束（取消或完成）的回合的更新会被丢弃，迟到的片段不会混入下一回合。每个事件都注明所属会话。
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

	/** 会话的配置选项发生变化（会话创建或恢复，或设置了某个选项）。 */
	FOnShuttlegentConfigOptionsChanged OnConfigOptionsChanged;

	/** 绑定后权限请求交由它处理（参数中注明会话）；未绑定时按 UShuttlegentSettings::PermissionPolicy 答复。 */
	FShuttlegentPermissionHandler PermissionHandler;

	/** 每次 session/new 与 session/resume 声明的 ACP mcpServers 条目；ShuttlegentMcpServer 在此声明编辑器端点。未绑定时为空。 */
	TFunction<TArray<TSharedPtr<FJsonValue>>()> McpServersProvider;

	/** 在运行时启动前向其环境追加变量；ShuttlegentMcpServer 在此加入端点 URL 与新 token。 */
	TFunction<void(TMap<FString, FString>& /*Environment*/)> RuntimeEnvironmentProvider;

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

	/** 启动运行时并发送 initialize。新启动的运行时在 NewSession 或 ResumeSession 之前没有会话。失败时由 GetStartIssue 给出类型。 */
	bool Start(FString& OutError);

	/** 最近一次 Start 失败（含 initialize 失败）的类型；启动成功后为 None。 */
	EShuttlegentStartIssue GetStartIssue() const { return StartIssue; }

	/** 运行时错误消息的类型：缺少密钥、密钥被拒，或 Other。 */
	static EShuttlegentStartIssue ClassifyError(const FString& Message);

	/** 关闭运行时。其会话可在下次 Start 后恢复。 */
	void Stop();

	/** 新建一个以项目目录为 cwd 的会话，由 OnSessionReady 给出其 ID。不影响其他已打开的会话。 */
	bool NewSession(FString& OutError);

	/** 重新接入一个已保存的会话，由运行时恢复其模型上下文。不影响其他已打开的会话。 */
	bool ResumeSession(const FString& SessionId, FString& OutError);

	/** 向一个已打开且没有进行中提问的会话发送一条文本提问。 */
	bool Prompt(const FString& SessionId, const FString& Text, FString& OutError);

	/** 取消该会话进行中的提问（如有）。 */
	void Cancel(const FString& SessionId);

	/** 修改一个已打开且空闲的会话的配置选项，例如 "model" 或 "reasoning_effort"。 */
	bool SetConfigOption(const FString& SessionId, const FString& ConfigId, const FString& Value, FString& OutError);

	/** 关闭一个已打开的会话；之后仍可恢复。 */
	void CloseSession(const FString& SessionId);

	EShuttlegentAgentState GetState() const { return State; }
	bool IsSessionOpen(const FString& SessionId) const { return OpenSessions.Contains(SessionId); }
	bool IsPrompting(const FString& SessionId) const;

	/** 已打开会话的配置选项；未打开的会话返回空。 */
	TArray<TSharedPtr<FJsonValue>> GetConfigOptions(const FString& SessionId) const;

	FShuttlegentSessionStore& GetSessionStore() { return SessionStore; }

private:
	void SetState(EShuttlegentAgentState NewState);
	TSharedRef<FJsonObject> MakeSessionParams(const FString& SessionId) const;
	void ApplySessionResult(const FString& SessionId, const TSharedPtr<FJsonObject>& Result, bool bCreated);
	/** 配置选项中当前的 "model" 值及其显示名（选项没有名称时即为该值）。 */
	static void GetCurrentModel(const TArray<TSharedPtr<FJsonValue>>& ConfigOptions, FString& OutValue, FString& OutName);
	void HandleNotification(const FString& Method, const TSharedPtr<FJsonObject>& Params);
	void HandleAgentRequest(const FString& Method, const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Params);
	void RespondPermission(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Params, bool bAllow);
	void HandleRuntimeClosed(int32 ExitCode);
};
