#pragma once

#include "Modules/ModuleInterface.h"

class FShuttlegentAgentService;
class FShuttlegentConsoleCommands;

class SHUTTLEGENTCORE_API FShuttlegentCoreModule : public IModuleInterface
{
private:
	TSharedPtr<FShuttlegentAgentService> AgentService;
	TUniquePtr<FShuttlegentConsoleCommands> ConsoleCommands;

public:
	FShuttlegentCoreModule();
	virtual ~FShuttlegentCoreModule() override;

	static FShuttlegentCoreModule& Get();

	/** The editor-wide agent service; created at startup, runtime launched on demand. */
	TSharedPtr<FShuttlegentAgentService> GetAgentService() const { return AgentService; }

	//~ Begin IModuleInterface Interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface.
};
