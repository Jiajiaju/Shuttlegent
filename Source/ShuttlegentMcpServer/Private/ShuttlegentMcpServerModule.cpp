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
	// 由下方 IMPLEMENT_MODULE 注册的模块名
	inline const FName ModuleName = TEXT("ShuttlegentMcpServer");

	// Core 模块，本模块关闭时它可能已被卸载
	inline const FName CoreModuleName = TEXT("ShuttlegentCore");
}

void FShuttlegentMcpServerModule::StartupModule()
{
	WriteGate = MakeShared<FShuttlegentWriteGate>();
	WriteGate->Initialize();
	WriteProbe = MakeShared<FShuttlegentWriteProbe>(WriteGate.ToSharedRef());
	QuestionGate = MakeShared<FShuttlegentQuestionGate>();

	// Commandlet 没有需要服务的运行时
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
		// 先编辑器端点，后项目的外部服务器
		const TSharedPtr<FShuttlegentMcpEndpoint> Pinned = WeakEndpoint.Pin();
		TArray<TSharedPtr<FJsonValue>> Servers = Pinned.IsValid() ? Pinned->MakeDeclaration() : TArray<TSharedPtr<FJsonValue>>();
		Servers.Append(FShuttlegentExternalMcp::MakeDeclarations());
		return Servers;
	};
	Service->RuntimeEnvironmentProvider = [WeakEndpoint](TMap<FString, FString>& Environment)
	{
		// 每次启动的运行时都会获得新的端点 token；Shuttlegent 自有的运行时插件从环境变量中获取它
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
