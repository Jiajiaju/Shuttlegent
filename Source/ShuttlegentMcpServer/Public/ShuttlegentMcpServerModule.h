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

/** A session's context breakdown arrived from the runtime (ACP session id, breakdown); game thread. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShuttlegentContextBreakdown, const FString& /*SessionId*/, const FShuttlegentContextBreakdown& /*Breakdown*/);

class SHUTTLEGENTMCPSERVER_API FShuttlegentMcpServerModule : public IModuleInterface
{
private:
	/** Permission gate every editor write tool asks before it runs. */
	TSharedPtr<FShuttlegentWriteGate> WriteGate;

	/** Questions the agent asks the user through ask_user, answered in the panel. */
	TSharedPtr<FShuttlegentQuestionGate> QuestionGate;

	/** Shuttlegent.WriteProbe console command, a write tool on the same gate for acceptance. */
	TSharedPtr<FShuttlegentWriteProbe> WriteProbe;

	/** The MCP endpoint the runtime calls editor tools through; declared to it with every session. */
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

	/** Every tool the registered providers offer, switched on or off; for the settings' tool switches. */
	TArray<TSharedRef<IShuttlegentTool>> GetRegisteredTools() const;

	/** Name the runtime knows the editor endpoint by; its tool names carry it as a prefix. */
	FString GetEndpointName() const;

	/** Whether the editor endpoint listens, and its port (0 while not listening); for the panel. */
	bool IsEndpointListening() const;
	uint32 GetEndpointPort() const;

	/** The project's external MCP servers and why any is left out of the next session's declaration; for the panel. */
	TArray<FShuttlegentExternalMcpState> GetExternalServers() const;

	/** Context breakdowns the context tap runtime plugin reports after each assistant message. */
	FOnShuttlegentContextBreakdown& OnContextBreakdown() { return ContextBreakdownEvent; }
};
