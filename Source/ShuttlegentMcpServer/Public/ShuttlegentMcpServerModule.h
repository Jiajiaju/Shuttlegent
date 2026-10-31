#pragma once

#include "Modules/ModuleInterface.h"

class FShuttlegentMcpEndpoint;
class IShuttlegentTool;
class FShuttlegentQuestionGate;
class FShuttlegentWriteGate;
class FShuttlegentWriteProbe;
enum class EShuttlegentAgentState : uint8;

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

private:
	/** A runtime that starts gets a new endpoint token. */
	void HandleAgentStateChanged(EShuttlegentAgentState NewState);
};
