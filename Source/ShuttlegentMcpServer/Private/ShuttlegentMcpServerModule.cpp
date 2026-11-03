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
#include "Endpoint/FShuttlegentPlanReview.h"
#include "Endpoint/FShuttlegentSessionEvent.h"
#include "Endpoint/FShuttlegentToolRegistry.h"
#include "ShuttlegentAgentService.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentCoreModule.h"
#include "ShuttlegentLog.h"
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
	Endpoint->OnSessionEvent = [this](const FShuttlegentSessionEvent& Event)
	{
		UE_LOG(LogShuttlegent, Verbose, TEXT("Shuttlegent session event %s in %s"), *Event.Type, *Event.SessionId);
		SessionEventEvent.Broadcast(Event);
	};
	Endpoint->OnPlanReview = [this](const FShuttlegentPlanReview& Review)
	{
		HandlePlanReview(Review);
	};
	Endpoint->OnPlanReviewClosed = [this](const FString& SessionId, const FString& ReviewId)
	{
		PlanReviewClosedEvent.Broadcast(SessionId, ReviewId);
	};

	const TSharedPtr<FShuttlegentAgentService> Service = FShuttlegentCoreModule::Get().GetAgentService();
	if (!Service.IsValid()) return;

	const TWeakPtr<FShuttlegentMcpEndpoint> WeakEndpoint = Endpoint;
	Service->McpServersProvider = [WeakEndpoint](const FString& ConversationKey)
	{
		// 先编辑器端点，后项目的外部服务器
		const TSharedPtr<FShuttlegentMcpEndpoint> Pinned = WeakEndpoint.Pin();
		TArray<TSharedPtr<FJsonValue>> Servers = Pinned.IsValid() ? Pinned->MakeDeclaration(ConversationKey) : TArray<TSharedPtr<FJsonValue>>();
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
		Endpoint->OnSessionEvent = nullptr;
		Endpoint->OnPlanReview = nullptr;
		Endpoint->OnPlanReviewClosed = nullptr;
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

void FShuttlegentMcpServerModule::CallRuntimeControl(const TCHAR* Method, const TSharedRef<FJsonObject>& Params, float TimeoutSeconds, TFunction<void(const TSharedPtr<FJsonObject>&, const FString&)> OnDone) const
{
	if (RuntimeControlUrl.IsEmpty() || !Endpoint.IsValid())
	{
		OnDone(nullptr, TEXT("The runtime's session control plugin is not available."));
		return;
	}

	const TSharedRef<FJsonObject> Message = MakeShared<FJsonObject>();
	Message->SetStringField(ShuttlegentMcpEndpointConstant::FieldMethod, Method);
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
	Request->SetTimeout(TimeoutSeconds);
	Request->OnProcessRequestComplete().BindLambda([OnDone](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
	{
		if (!bConnected || !Response.IsValid())
		{
			OnDone(nullptr, TEXT("The runtime's session control plugin did not answer."));
			return;
		}

		TSharedPtr<FJsonObject> Answer;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, Answer) || !Answer.IsValid())
		{
			OnDone(nullptr, FString::Printf(TEXT("Unreadable answer from the session control plugin (HTTP %d)."), Response->GetResponseCode()));
			return;
		}

		const TSharedPtr<FJsonObject>* Result = nullptr;
		if (Answer->TryGetObjectField(ShuttlegentMcpEndpointConstant::FieldResult, Result))
		{
			OnDone(*Result, FString());
			return;
		}

		FString Error;
		Answer->TryGetStringField(ShuttlegentMcpEndpointConstant::FieldError, Error);
		OnDone(nullptr, Error.IsEmpty() ? FString::Printf(TEXT("The session control request failed (HTTP %d)."), Response->GetResponseCode()) : Error);
	});
	Request->ProcessRequest();
}

void FShuttlegentMcpServerModule::ForkSession(const FString& SessionId, int32 UserMessage, bool bThroughTurn, TFunction<void(const FString&, const FString&)> OnDone) const
{
	const TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(ShuttlegentMcpEndpointConstant::FieldSessionId, SessionId);
	Params->SetNumberField(ShuttlegentMcpEndpointConstant::FieldUserMessage, UserMessage);
	Params->SetStringField(ShuttlegentMcpEndpointConstant::FieldCut, bThroughTurn ? ShuttlegentMcpEndpointConstant::CutAfter : ShuttlegentMcpEndpointConstant::CutBefore);

	CallRuntimeControl(ShuttlegentMcpEndpointConstant::MethodFork, Params, ShuttlegentMcpEndpointConstant::ForkTimeoutSeconds, [OnDone](const TSharedPtr<FJsonObject>& Result, const FString& Error)
	{
		FString NewSessionId;
		if (Result.IsValid() && Result->TryGetStringField(ShuttlegentMcpEndpointConstant::FieldSessionId, NewSessionId) && !NewSessionId.IsEmpty())
		{
			OnDone(NewSessionId, FString());
			return;
		}

		OnDone(FString(), Error.IsEmpty() ? FString(ShuttlegentMcpEndpointConstant::ForkNoSession) : Error);
	});
}

void FShuttlegentMcpServerModule::RunCommand(const FString& SessionId, const FString& Line, TFunction<void(bool, const FString&)> OnDone) const
{
	const TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(ShuttlegentMcpEndpointConstant::FieldSessionId, SessionId);
	Params->SetStringField(ShuttlegentMcpEndpointConstant::FieldLine, Line);

	CallRuntimeControl(ShuttlegentMcpEndpointConstant::MethodCommand, Params, ShuttlegentMcpEndpointConstant::CommandTimeoutSeconds, [OnDone, Line](const TSharedPtr<FJsonObject>& Result, const FString& Error)
	{
		if (!Result.IsValid())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Shuttlegent runtime command %s failed: %s"), *Line, *Error);
			OnDone(false, Error);
			return;
		}

		FString Kind;
		FString Text;
		Result->TryGetStringField(ShuttlegentMcpEndpointConstant::FieldKind, Kind);
		Result->TryGetStringField(ShuttlegentMcpEndpointConstant::FieldText, Text);

		const bool bSucceeded = Kind == ShuttlegentMcpEndpointConstant::CommandKindSuccess;
		UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent runtime command %s: %s %s"), *Line, *Kind, *Text);
		OnDone(bSucceeded, Text);
	});
}

void FShuttlegentMcpServerModule::SetPlanMode(const FString& SessionId, TOptional<bool> bActive, TFunction<void(bool, const FString&)> OnDone) const
{
	const TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(ShuttlegentMcpEndpointConstant::FieldSessionId, SessionId);
	if (bActive.IsSet()) Params->SetBoolField(ShuttlegentMcpEndpointConstant::FieldActive, bActive.GetValue());

	CallRuntimeControl(ShuttlegentMcpEndpointConstant::MethodPlanMode, Params, ShuttlegentMcpEndpointConstant::ControlTimeoutSeconds, [OnDone, SessionId](const TSharedPtr<FJsonObject>& Result, const FString& Error)
	{
		if (!Result.IsValid())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Shuttlegent plan mode request for %s failed: %s"), *SessionId, *Error);
			OnDone(false, Error);
			return;
		}

		FString Outcome;
		bool bActiveNow = false;
		const TSharedPtr<FJsonObject>* State = nullptr;
		Result->TryGetStringField(ShuttlegentMcpEndpointConstant::FieldOutcome, Outcome);
		if (Result->TryGetObjectField(ShuttlegentMcpEndpointConstant::FieldState, State)) (*State)->TryGetBoolField(ShuttlegentMcpEndpointConstant::FieldActive, bActiveNow);

		UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent plan mode for %s: %s, active %s"), *SessionId, *Outcome, bActiveNow ? TEXT("yes") : TEXT("no"));
		OnDone(bActiveNow, FString());
	});
}

void FShuttlegentMcpServerModule::AnswerPlanReview(const FString& ReviewId, bool bApprove, const FString& Feedback, TFunction<void(const FString&)> OnDone) const
{
	const TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(ShuttlegentMcpEndpointConstant::FieldReviewId, ReviewId);
	Params->SetBoolField(ShuttlegentMcpEndpointConstant::FieldApprove, bApprove);
	if (!Feedback.IsEmpty()) Params->SetStringField(ShuttlegentMcpEndpointConstant::FieldFeedback, Feedback);

	CallRuntimeControl(ShuttlegentMcpEndpointConstant::MethodAnswerPlanReview, Params, ShuttlegentMcpEndpointConstant::ControlTimeoutSeconds, [OnDone, ReviewId, bApprove](const TSharedPtr<FJsonObject>& Result, const FString& Error)
	{
		UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent plan review %s %s: %s"), *ReviewId, bApprove ? TEXT("approved") : TEXT("sent back"), Result.IsValid() ? TEXT("ok") : *Error);
		OnDone(Result.IsValid() ? FString() : Error);
	});
}

void FShuttlegentMcpServerModule::QueryImageInput(const FString& ModelValue, TFunction<void(TOptional<bool>)> OnDone) const
{
	const TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(ShuttlegentMcpEndpointConstant::FieldModel, ModelValue);

	CallRuntimeControl(ShuttlegentMcpEndpointConstant::MethodImageInput, Params, ShuttlegentMcpEndpointConstant::ControlTimeoutSeconds, [OnDone, ModelValue](const TSharedPtr<FJsonObject>& Result, const FString& Error)
	{
		bool bImage = false;
		if (!Result.IsValid() || !Result->TryGetBoolField(ShuttlegentMcpEndpointConstant::FieldImage, bImage))
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Shuttlegent image input query for %s failed: %s"), *ModelValue, *Error);
			OnDone(TOptional<bool>());
			return;
		}

		UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent image input for %s: %s"), *ModelValue, bImage ? TEXT("yes") : TEXT("no"));
		OnDone(bImage);
	});
}

void FShuttlegentMcpServerModule::SetNextAgent(const FString& AgentId, TFunction<void(const FString&)> OnDone) const
{
	const TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(ShuttlegentMcpEndpointConstant::FieldAgentId, AgentId);

	CallRuntimeControl(ShuttlegentMcpEndpointConstant::MethodNextAgent, Params, ShuttlegentMcpEndpointConstant::ControlTimeoutSeconds, [OnDone, AgentId](const TSharedPtr<FJsonObject>& Result, const FString& Error)
	{
		UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent next session agent %s: %s"), *AgentId, Result.IsValid() ? TEXT("ok") : *Error);
		OnDone(Result.IsValid() ? FString() : Error);
	});
}

void FShuttlegentMcpServerModule::HandlePlanReview(const FShuttlegentPlanReview& Review)
{
	UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent plan review %s requested in session %s"), *Review.ReviewId, *Review.SessionId);
	if (PlanReviewEvent.IsBound())
	{
		PlanReviewEvent.Broadcast(Review);
		return;
	}

	AnswerPlanReview(Review.ReviewId, false, ShuttlegentMcpEndpointConstant::PlanReviewUnavailable, [](const FString&) {});
}

FShuttlegentMcpServerModule& FShuttlegentMcpServerModule::Get()
{
	return FModuleManager::LoadModuleChecked<FShuttlegentMcpServerModule>(ShuttlegentMcpServerModulePrivate::ModuleName);
}

IMPLEMENT_MODULE(FShuttlegentMcpServerModule, ShuttlegentMcpServer)
