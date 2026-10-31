#include "ShuttlegentMcpServerModule.h"

#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"

#include "Endpoint/FShuttlegentContextBreakdown.h"
#include "Endpoint/FShuttlegentExternalMcp.h"
#include "Endpoint/FShuttlegentExternalMcpState.h"
#include "Endpoint/FShuttlegentMcpEndpoint.h"
#include "Endpoint/FShuttlegentToolRegistry.h"
#include "ShuttlegentAgentService.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentCoreModule.h"
#include "ShuttlegentMcpServerConstants.h"
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

	Endpoint->OnContextBreakdown = [this](const FString& SessionId, const FShuttlegentContextBreakdown& Breakdown)
	{
		ContextBreakdownEvent.Broadcast(SessionId, Breakdown);
	};

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
	Service->RuntimeEnvironmentProvider = [WeakEndpoint](TMap<FString, FString>& Environment)
	{
		// A runtime that starts gets a new endpoint token; Shuttlegent's own runtime plugins learn it from the environment
		const TSharedPtr<FShuttlegentMcpEndpoint> Pinned = WeakEndpoint.Pin();
		if (!Pinned.IsValid() || !Pinned->IsListening()) return;

		Pinned->RotateToken();
		Environment.Add(ShuttlegentEnvVar::EditorEndpointUrl, Pinned->GetUrl());
		Environment.Add(ShuttlegentEnvVar::EditorEndpointToken, Pinned->GetToken());
	};
}

void FShuttlegentMcpServerModule::ShutdownModule()
{
	FShuttlegentCoreModule* Core = FModuleManager::GetModulePtr<FShuttlegentCoreModule>(ShuttlegentMcpServerModulePrivate::CoreModuleName);
	const TSharedPtr<FShuttlegentAgentService> Service = Core != nullptr ? Core->GetAgentService() : nullptr;
	if (Service.IsValid())
	{
		Service->McpServersProvider = nullptr;
		Service->RuntimeEnvironmentProvider = nullptr;
	}
	if (Endpoint.IsValid())
	{
		Endpoint->OnContextBreakdown = nullptr;
		Endpoint->Stop();
	}
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

FString FShuttlegentMcpServerModule::GetEndpointName() const
{
	return ShuttlegentMcpEndpointConstant::ServerName;
}

bool FShuttlegentMcpServerModule::IsEndpointListening() const
{
	return Endpoint.IsValid() && Endpoint->IsListening();
}

uint32 FShuttlegentMcpServerModule::GetEndpointPort() const
{
	return IsEndpointListening() ? Endpoint->GetPort() : 0;
}

TArray<FShuttlegentExternalMcpState> FShuttlegentMcpServerModule::GetExternalServers() const
{
	return FShuttlegentExternalMcp::DescribeServers();
}

FShuttlegentMcpServerModule& FShuttlegentMcpServerModule::Get()
{
	return FModuleManager::LoadModuleChecked<FShuttlegentMcpServerModule>(ShuttlegentMcpServerModulePrivate::ModuleName);
}

IMPLEMENT_MODULE(FShuttlegentMcpServerModule, ShuttlegentMcpServer)
