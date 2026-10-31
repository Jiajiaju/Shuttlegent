#pragma once

#include "Delegates/Delegate.h"
#include "Modules/ModuleInterface.h"

class FShuttlegentMcpEndpoint;
class IShuttlegentTool;
class FShuttlegentQuestionGate;
class FShuttlegentWriteGate;
class FShuttlegentWriteProbe;
struct FShuttlegentContextBreakdown;
struct FShuttlegentExternalMcpState;

/** 收到运行时发来的会话上下文构成（ACP 会话 id、构成）；游戏线程。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShuttlegentContextBreakdown, const FString& /*SessionId*/, const FShuttlegentContextBreakdown& /*Breakdown*/);

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
};
