#include "ShuttlegentMcpServerModule.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

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

	// 调用 session control 插件所用的 HTTP 方法
	inline const TCHAR* VerbPost = TEXT("POST");
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
	Endpoint->OnRuntimeControl = [this](const FString& Url)
	{
		RuntimeControlUrl = Url;
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
	Service->RuntimeEnvironmentProvider = [this, WeakEndpoint](TMap<FString, FString>& Environment)
	{
		// 上一个运行时的控制端口随之失效，新运行时的 session control 插件加载后会重新报来
		RuntimeControlUrl.Reset();

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
		Endpoint->OnRuntimeControl = nullptr;
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

void FShuttlegentMcpServerModule::ForkSession(const FString& SessionId, int32 UserMessage, bool bThroughTurn, TFunction<void(const FString&, const FString&)> OnDone) const
{
	if (RuntimeControlUrl.IsEmpty() || !Endpoint.IsValid())
	{
		OnDone(FString(), TEXT("The runtime's session control plugin is not available."));
		return;
	}

	const TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(ShuttlegentMcpEndpointConstant::FieldSessionId, SessionId);
	Params->SetNumberField(ShuttlegentMcpEndpointConstant::FieldUserMessage, UserMessage);
	Params->SetStringField(ShuttlegentMcpEndpointConstant::FieldCut, bThroughTurn ? ShuttlegentMcpEndpointConstant::CutAfter : ShuttlegentMcpEndpointConstant::CutBefore);

	const TSharedRef<FJsonObject> Message = MakeShared<FJsonObject>();
	Message->SetStringField(ShuttlegentMcpEndpointConstant::FieldMethod, ShuttlegentMcpEndpointConstant::MethodFork);
	Message->SetObjectField(ShuttlegentMcpEndpointConstant::FieldParams, Params);

	FString Body;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Body);
	FJsonSerializer::Serialize(Message, Writer);

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(RuntimeControlUrl);
	Request->SetVerb(ShuttlegentMcpServerModulePrivate::VerbPost);
	Request->SetHeader(ShuttlegentMcpEndpointConstant::HeaderContentType, ShuttlegentMcpEndpointConstant::ContentTypeJson);
	Request->SetHeader(ShuttlegentMcpEndpointConstant::HeaderAuthorization, FString(ShuttlegentMcpEndpointConstant::BearerPrefix) + Endpoint->GetToken());
	Request->SetContentAsString(Body);
	Request->SetTimeout(ShuttlegentMcpEndpointConstant::ForkTimeoutSeconds);
	Request->OnProcessRequestComplete().BindLambda([OnDone](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		if (!bConnected || !Response.IsValid())
		{
			OnDone(FString(), TEXT("The runtime's session control plugin did not answer."));
			return;
		}

		TSharedPtr<FJsonObject> Answer;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, Answer) || !Answer.IsValid())
		{
			OnDone(FString(), FString::Printf(TEXT("Unreadable answer from the session control plugin (HTTP %d)."), Response->GetResponseCode()));
			return;
		}

		FString NewSessionId;
		const TSharedPtr<FJsonObject>* Result = nullptr;
		if (Answer->TryGetObjectField(ShuttlegentMcpEndpointConstant::FieldResult, Result) && (*Result)->TryGetStringField(ShuttlegentMcpEndpointConstant::FieldSessionId, NewSessionId) && !NewSessionId.IsEmpty())
		{
			OnDone(NewSessionId, FString());
			return;
		}

		FString Error;
		Answer->TryGetStringField(ShuttlegentMcpEndpointConstant::FieldError, Error);
		OnDone(FString(), Error.IsEmpty() ? FString::Printf(TEXT("The fork failed (HTTP %d)."), Response->GetResponseCode()) : Error);
	});
	Request->ProcessRequest();
}

FShuttlegentMcpServerModule& FShuttlegentMcpServerModule::Get()
{
	return FModuleManager::LoadModuleChecked<FShuttlegentMcpServerModule>(ShuttlegentMcpServerModulePrivate::ModuleName);
}

IMPLEMENT_MODULE(FShuttlegentMcpServerModule, ShuttlegentMcpServer)
