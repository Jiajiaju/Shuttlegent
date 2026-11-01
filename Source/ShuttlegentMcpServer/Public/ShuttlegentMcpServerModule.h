#pragma once

#include "Delegates/Delegate.h"
#include "Misc/Optional.h"
#include "Modules/ModuleInterface.h"

class FJsonObject;
class FShuttlegentMcpEndpoint;
class IShuttlegentTool;
class FShuttlegentQuestionGate;
class FShuttlegentWriteGate;
class FShuttlegentWriteProbe;
struct FShuttlegentContextBreakdown;
struct FShuttlegentPlanReview;
struct FShuttlegentSessionEvent;
struct FShuttlegentExternalMcpState;

/** 收到运行时发来的会话上下文构成（ACP 会话 id、构成）；游戏线程。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShuttlegentContextBreakdown, const FString& /*SessionId*/, const FShuttlegentContextBreakdown& /*Breakdown*/);

/** 收到 session tap 运行时插件推来的会话事件；游戏线程。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentSessionEvent, const FShuttlegentSessionEvent& /*Event*/);

/** 计划模式下 Agent 交出计划等待审批；游戏线程。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnShuttlegentPlanReview, const FShuttlegentPlanReview& /*Review*/);

/** 回合取消，某次计划审批不再等待（会话 id、审批 id）；游戏线程。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShuttlegentPlanReviewClosed, const FString& /*SessionId*/, const FString& /*ReviewId*/);

class SHUTTLEGENTMCPSERVER_API FShuttlegentMcpServerModule : public IModuleInterface
{
private:
	/** 每个编辑器写入工具在运行前都须请求的权限门。 */
	TSharedPtr<FShuttlegentWriteGate> WriteGate;

	/** agent 通过 ask_user 向用户提出、在面板中作答的问题。 */
	TSharedPtr<FShuttlegentQuestionGate> QuestionGate;

	/** Shuttlegent.WriteProbe 控制台命令，经同一写入门执行的写入工具，用于验收。 */
	TSharedPtr<FShuttlegentWriteProbe> WriteProbe;

	/** 运行时调用编辑器工具所经由的 MCP 端点；随每个会话向运行时声明。 */
	TSharedPtr<FShuttlegentMcpEndpoint> Endpoint;

	FOnShuttlegentContextBreakdown ContextBreakdownEvent;
	FOnShuttlegentSessionEvent SessionEventEvent;
	FOnShuttlegentPlanReview PlanReviewEvent;
	FOnShuttlegentPlanReviewClosed PlanReviewClosedEvent;

	/** session control 运行时插件报来的控制端口 URL；运行时每次启动前清空，插件加载后重新报来。 */
	FString RuntimeControlUrl;

public:
	//~ Begin IModuleInterface Interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface.

	static FShuttlegentMcpServerModule& Get();

	TSharedPtr<FShuttlegentWriteGate> GetWriteGate() const { return WriteGate; }
	TSharedPtr<FShuttlegentQuestionGate> GetQuestionGate() const { return QuestionGate; }

	/** 已注册工具提供者提供的全部工具，无论开启与否；供设置中的工具开关使用。 */
	TArray<TSharedRef<IShuttlegentTool>> GetRegisteredTools() const;

	/** 运行时识别编辑器端点所用的名称；其工具名以此为前缀。 */
	FString GetEndpointName() const;

	/** 编辑器端点是否正在监听，及其端口（未监听时为 0）；供面板使用。 */
	bool IsEndpointListening() const;
	uint32 GetEndpointPort() const;

	/** 项目的外部 MCP 服务器，以及其中某个未被纳入下一个会话声明的原因；供面板使用。 */
	TArray<FShuttlegentExternalMcpState> GetExternalServers() const;

	/** context tap 运行时插件在每条 assistant 消息之后上报的上下文构成。 */
	FOnShuttlegentContextBreakdown& OnContextBreakdown() { return ContextBreakdownEvent; }

	/** 当前运行时的 session control 插件已报来控制端口，可以分叉会话。 */
	bool CanForkSessions() const { return !RuntimeControlUrl.IsEmpty(); }

	/**
	 * 让运行时把会话截在第 UserMessage 条用户消息（从 0 计）之前，或 bThroughTurn 时截在该轮结束之后，另存为新会话。
	 * OnDone 在游戏线程收到新会话 id；失败时 id 为空并给出原因。会话须已在运行时打开且没有进行中的回合。
	 */
	void ForkSession(const FString& SessionId, int32 UserMessage, bool bThroughTurn, TFunction<void(const FString& /*NewSessionId*/, const FString& /*Error*/)> OnDone) const;

	/** session tap 运行时插件推来的会话事件：待办、计划模式、压缩、目标、子任务。 */
	FOnShuttlegentSessionEvent& OnSessionEvent() { return SessionEventEvent; }

	/** 计划模式下等待审批的计划；没有任何订阅者时模块直接以“不可用”退回，回合不会卡住。 */
	FOnShuttlegentPlanReview& OnPlanReview() { return PlanReviewEvent; }
	FOnShuttlegentPlanReviewClosed& OnPlanReviewClosed() { return PlanReviewClosedEvent; }

	/**
	 * 在运行时执行一条斜杠命令（如 /compact、/plan），ACP 会把它当作普通文本发给模型。OnDone 在游戏线程收到命令的
	 * 文字结果；失败时 bSucceeded 为 false 并给出原因。/compact 要求会话空闲。
	 */
	void RunCommand(const FString& SessionId, const FString& Line, TFunction<void(bool /*bSucceeded*/, const FString& /*Text*/)> OnDone) const;

	/**
	 * 进入或退出计划模式（bActive 未设置时只查询）；回合进行中时在下一步生效。OnDone 收到运行时记录的当前状态
	 * （回合进行中切换时仍为切换前的状态，切换生效后由 plan/mode 事件报来）与错误原因，成功时原因为空。
	 */
	void SetPlanMode(const FString& SessionId, TOptional<bool> bActive, TFunction<void(bool /*bActiveNow*/, const FString& /*Error*/)> OnDone) const;

	/**
	 * 查询某个模型路由是否接受图片；ModelValue 为会话 "model" 配置项的取值。运行时只按默认模型声明一次图片能力，
	 * 沿用旧模型的会话须逐个确认。OnDone 在游戏线程收到结果，请求失败时为未设置。
	 */
	void QueryImageInput(const FString& ModelValue, TFunction<void(TOptional<bool> /*bImage*/)> OnDone) const;

	/** 答复计划审批：批准后 Agent 退出计划模式并执行；否则继续规划，Feedback 交给 Agent。OnDone 收到错误原因，成功时为空。 */
	void AnswerPlanReview(const FString& ReviewId, bool bApprove, const FString& Feedback, TFunction<void(const FString& /*Error*/)> OnDone) const;

private:
	/** 向 session control 插件发一个请求；OnDone 在游戏线程收到 result 对象，失败时为空并给出原因。 */
	void CallRuntimeControl(const TCHAR* Method, const TSharedRef<FJsonObject>& Params, float TimeoutSeconds, TFunction<void(const TSharedPtr<FJsonObject>& /*Result*/, const FString& /*Error*/)> OnDone) const;

	/** 没有界面订阅审批时，以“不可用”退回计划，Agent 改为直接作答。 */
	void HandlePlanReview(const FShuttlegentPlanReview& Review);
};
