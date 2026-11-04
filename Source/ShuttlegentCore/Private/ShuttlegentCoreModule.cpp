#include "ShuttlegentCoreModule.h"

#include "Modules/ModuleManager.h"

#include "FShuttlegentConnectionTester.h"
#include "FShuttlegentMemoryStore.h"
#include "FShuttlegentModelRoutes.h"
#include "ShuttlegentAgentService.h"
#include "ShuttlegentConsoleCommands.h"
#include "ShuttlegentSettings.h"

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
	// 补齐内置连接；上次退出时还在检测的连接视为未检测，填了密钥却未检测的连接现在检测
	UShuttlegentSettings* Settings = GetMutableDefault<UShuttlegentSettings>();
	Settings->EnsureBuiltins();
	for (int32 Index = 0; Index < Settings->Connections.Num(); ++Index)
	{
		FShuttlegentConnection& Connection = Settings->Connections[Index];
		if (Connection.Status == EShuttlegentConnectionStatus::Testing) Connection.Status = EShuttlegentConnectionStatus::Untested;
		const bool bStale = Connection.Status == EShuttlegentConnectionStatus::Untested || Connection.TestedFingerprint != FShuttlegentModelRoutes::MakeFingerprint(Connection);
		if (bStale && !Connection.ApiKey.TrimStartAndEnd().IsEmpty()) FShuttlegentConnectionTester::Test(Index);
	}

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
