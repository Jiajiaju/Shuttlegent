#include "ShuttlegentCoreModule.h"

#include "Modules/ModuleManager.h"

#include "FShuttlegentMemoryStore.h"
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

	// 个人文件统一放在 Saved/Shuttlegent；旧位置的个人提示词运行时已不再读取
	FShuttlegentMemoryStore::MigrateLocalPrompt();
}

void FShuttlegentCoreModule::ShutdownModule()
{
	ConsoleCommands.Reset();

	if (AgentService.IsValid()) AgentService->Stop();
	AgentService.Reset();
}

IMPLEMENT_MODULE(FShuttlegentCoreModule, ShuttlegentCore)
