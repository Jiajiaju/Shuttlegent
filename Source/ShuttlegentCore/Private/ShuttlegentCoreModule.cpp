#include "ShuttlegentCoreModule.h"

#include "Modules/ModuleManager.h"

#include "ShuttlegentAgentService.h"
#include "ShuttlegentConsoleCommands.h"

namespace ShuttlegentCoreModulePrivate
{
	inline const TCHAR* ModuleName = TEXT("ShuttlegentCore");
}

FShuttlegentCoreModule::FShuttlegentCoreModule() = default;

FShuttlegentCoreModule::~FShuttlegentCoreModule() = default;

FShuttlegentCoreModule& FShuttlegentCoreModule::Get()
{
	return FModuleManager::LoadModuleChecked<FShuttlegentCoreModule>(ShuttlegentCoreModulePrivate::ModuleName);
}

void FShuttlegentCoreModule::StartupModule()
{
	AgentService = MakeShared<FShuttlegentAgentService>();
	ConsoleCommands = MakeUnique<FShuttlegentConsoleCommands>(AgentService.ToSharedRef());
}

void FShuttlegentCoreModule::ShutdownModule()
{
	ConsoleCommands.Reset();

	if (AgentService.IsValid()) AgentService->Stop();
	AgentService.Reset();
}

IMPLEMENT_MODULE(FShuttlegentCoreModule, ShuttlegentCore)
