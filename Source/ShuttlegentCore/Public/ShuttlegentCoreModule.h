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

	/** 编辑器全局的 Agent 服务；启动时创建，运行时按需启动。 */
	TSharedPtr<FShuttlegentAgentService> GetAgentService() const { return AgentService; }

	//~ Begin IModuleInterface Interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface.
};
