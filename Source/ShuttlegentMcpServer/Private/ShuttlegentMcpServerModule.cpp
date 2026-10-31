#include "ShuttlegentMcpServerModule.h"

#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"

#include "Endpoint/FShuttlegentExternalMcp.h"
#include "Endpoint/FShuttlegentMcpEndpoint.h"
#include "Endpoint/FShuttlegentToolRegistry.h"
#include "ShuttlegentAgentService.h"
#include "ShuttlegentCoreModule.h"
#include "QuestionGate/FShuttlegentQuestionGate.h"
#include "WriteGate/FShuttlegentWriteGate.h"
#include "WriteGate/FShuttlegentWriteProbe.h"

namespace ShuttlegentMcpServerModulePrivate
{
	// Module name registered by IMPLEMENT_MODULE below
	inline const FName ModuleName = TEXT("ShuttlegentMcpServer");

	// Core module, which may already be gone when this module shuts down
	inline const FName CoreModuleName = TEXT("ShuttlegentCore");
}

void FShuttlegentMcpServerModule::StartupModule()
{
	WriteGate = MakeShared<FShuttlegentWriteGate>();
	WriteGate->Initialize();
	WriteProbe = MakeShared<FShuttlegentWriteProbe>(WriteGate.ToSharedRef());
	QuestionGate = MakeShared<FShuttlegentQuestionGate>();

	// Commandlets have no runtime to serve
	if (IsRunningCommandlet()) return;

	Endpoint = MakeShared<FShuttlegentMcpEndpoint>();
	if (!Endpoint->Start(WriteGate.ToSharedRef())) return;

	const TSharedPtr<FShuttlegentAgentService> Service = FShuttlegentCoreModule::Get().GetAgentService();
	if (!Service.IsValid()) return;

	const TWeakPtr<FShuttlegentMcpEndpoint> WeakEndpoint = Endpoint;
	Service->McpServersProvider = [WeakEndpoint]()
	{
		// The editor endpoint first, then the project's external servers
		const TSharedPtr<FShuttlegentMcpEndpoint> Pinned = WeakEndpoint.Pin();
		TArray<TSharedPtr<FJsonValue>> Servers = Pinned.IsValid() ? Pinned->MakeDeclaration() : TArray<TSharedPtr<FJsonValue>>();
		Servers.Append(FShuttlegentExternalMcp::MakeDeclarations());
		return Servers;
	};
	Service->OnStateChanged.AddRaw(this, &FShuttlegentMcpServerModule::HandleAgentStateChanged);
}

void FShuttlegentMcpServerModule::HandleAgentStateChanged(EShuttlegentAgentState NewState)
{
	if (NewState == EShuttlegentAgentState::Starting && Endpoint.IsValid()) Endpoint->RotateToken();
}

void FShuttlegentMcpServerModule::ShutdownModule()
{
	FShuttlegentCoreModule* Core = FModuleManager::GetModulePtr<FShuttlegentCoreModule>(ShuttlegentMcpServerModulePrivate::CoreModuleName);
	const TSharedPtr<FShuttlegentAgentService> Service = Core != nullptr ? Core->GetAgentService() : nullptr;
	if (Service.IsValid())
	{
		Service->OnStateChanged.RemoveAll(this);
		Service->McpServersProvider = nullptr;
	}
	if (Endpoint.IsValid()) Endpoint->Stop();
	Endpoint.Reset();

	WriteProbe.Reset();
	if (QuestionGate.IsValid()) QuestionGate->Shutdown();
	QuestionGate.Reset();
	if (WriteGate.IsValid()) WriteGate->Shutdown();
	WriteGate.Reset();
}

TArray<TSharedRef<IShuttlegentTool>> FShuttlegentMcpServerModule::GetRegisteredTools() const
{
	return FShuttlegentToolRegistry::Collect();
}

FShuttlegentMcpServerModule& FShuttlegentMcpServerModule::Get()
{
	return FModuleManager::LoadModuleChecked<FShuttlegentMcpServerModule>(ShuttlegentMcpServerModulePrivate::ModuleName);
}

IMPLEMENT_MODULE(FShuttlegentMcpServerModule, ShuttlegentMcpServer)
