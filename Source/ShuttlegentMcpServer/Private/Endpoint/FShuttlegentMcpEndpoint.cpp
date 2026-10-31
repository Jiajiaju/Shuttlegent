#include "Endpoint/FShuttlegentMcpEndpoint.h"

#include "Dom/JsonValue.h"
#include "HttpPath.h"
#include "HttpServerModule.h"
#include "HttpServerRequest.h"
#include "HttpServerResponse.h"
#include "IHttpRouter.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Guid.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Endpoint/FShuttlegentContextBreakdown.h"
#include "Endpoint/FShuttlegentToolRegistry.h"
#include "FShuttlegentProjectConfig.h"
#include "Endpoint/FShuttlegentToolRunner.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentMcpServerConstants.h"
#include "Tool/IShuttlegentTool.h"
#include "WriteGate/FShuttlegentWriteGate.h"

namespace FShuttlegentMcpEndpointPrivate
{
	FString ToJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Json;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
		FJsonSerializer::Serialize(Object, Writer);
		return Json;
	}

	TUniquePtr<FHttpServerResponse> MakeJsonResponse(const TSharedRef<FJsonObject>& Body)
	{
		return FHttpServerResponse::Create(ToJson(Body), ShuttlegentMcpEndpointConstant::ContentTypeJson);
	}

	TSharedRef<FJsonObject> MakeEnvelope(const TSharedPtr<FJsonValue>& Id)
	{
		const TSharedRef<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetStringField(ShuttlegentMcpEndpointConstant::FieldJsonRpc, ShuttlegentMcpEndpointConstant::JsonRpcVersion);
		Envelope->SetField(ShuttlegentMcpEndpointConstant::FieldId, Id.IsValid() ? Id : MakeShared<FJsonValueNull>());
		return Envelope;
	}

	TSharedRef<FJsonObject> MakeError(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message)
	{
		const TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
		Error->SetNumberField(ShuttlegentMcpEndpointConstant::FieldCode, Code);
		Error->SetStringField(ShuttlegentMcpEndpointConstant::FieldMessage, Message);

		const TSharedRef<FJsonObject> Envelope = MakeEnvelope(Id);
		Envelope->SetObjectField(ShuttlegentMcpEndpointConstant::FieldError, Error);
		return Envelope;
	}

	/** MCP tools/call result: one text block and the error flag. */
	TSharedRef<FJsonObject> MakeToolResult(const FShuttlegentToolResult& Result)
	{
		const TSharedRef<FJsonObject> Block = MakeShared<FJsonObject>();
		Block->SetStringField(ShuttlegentMcpEndpointConstant::FieldType, ShuttlegentMcpEndpointConstant::ContentTypeText);
		Block->SetStringField(ShuttlegentMcpEndpointConstant::FieldText, Result.Text);

		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetArrayField(ShuttlegentMcpEndpointConstant::FieldContent, { MakeShared<FJsonValueObject>(Block) });
		Object->SetBoolField(ShuttlegentMcpEndpointConstant::FieldIsError, Result.bError);
		return Object;
	}

	FString GetPluginVersion()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ShuttlegentPath::PluginName);
		return Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : FString();
	}
}

bool FShuttlegentMcpEndpoint::Start(const TSharedRef<FShuttlegentWriteGate>& InGate)
{
	Gate = InGate;
	RotateToken();

	// Listeners must be enabled for GetHttpRouter to try the port, so a taken port is reported instead of shared silently
	FHttpServerModule& HttpServer = FHttpServerModule::Get();
	HttpServer.StartAllListeners();

	TWeakPtr<FShuttlegentMcpEndpoint> WeakThis = AsShared();
	const FHttpRequestHandler Handler = FHttpRequestHandler::CreateLambda([WeakThis](const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
	{
		const TSharedPtr<FShuttlegentMcpEndpoint> This = WeakThis.Pin();
		if (!This.IsValid()) return false;
		return This->HandleRequest(Request, OnComplete);
	});

	const EHttpServerRequestVerbs Verbs = EHttpServerRequestVerbs::VERB_POST | EHttpServerRequestVerbs::VERB_GET | EHttpServerRequestVerbs::VERB_DELETE;
	for (uint32 Candidate = ShuttlegentMcpEndpointConstant::FirstPort; Candidate < ShuttlegentMcpEndpointConstant::FirstPort + ShuttlegentMcpEndpointConstant::PortCount; ++Candidate)
	{
		const TSharedPtr<IHttpRouter> CandidateRouter = HttpServer.GetHttpRouter(Candidate, true);
		if (!CandidateRouter.IsValid()) continue;

		// A port another module of this editor already listens on may have the path taken
		const FHttpRouteHandle CandidateRoute = CandidateRouter->BindRoute(FHttpPath(ShuttlegentMcpEndpointConstant::Path), Verbs, Handler);
		if (!CandidateRoute.IsValid()) continue;

		Router = CandidateRouter;
		Route = CandidateRoute;
		Port = Candidate;
		UE_LOG(LogShuttlegent, Log, TEXT("MCP endpoint listening on %s:%u%s."), ShuttlegentMcpEndpointConstant::Host, Port, ShuttlegentMcpEndpointConstant::Path);
		return true;
	}

	UE_LOG(LogShuttlegent, Error, TEXT("MCP endpoint could not bind any port from %u to %u; editor tools are unavailable."), ShuttlegentMcpEndpointConstant::FirstPort, ShuttlegentMcpEndpointConstant::FirstPort + ShuttlegentMcpEndpointConstant::PortCount - 1);
	return false;
}

void FShuttlegentMcpEndpoint::Stop()
{
	if (Router.IsValid() && Route.IsValid()) Router->UnbindRoute(Route);
	Route.Reset();
	Router.Reset();
	Port = 0;
}

void FShuttlegentMcpEndpoint::RotateToken()
{
	Token = FGuid::NewGuid().ToString(EGuidFormats::Digits) + FGuid::NewGuid().ToString(EGuidFormats::Digits);
}

TArray<TSharedPtr<FJsonValue>> FShuttlegentMcpEndpoint::MakeDeclaration() const
{
	TArray<TSharedPtr<FJsonValue>> Servers;
	if (!IsListening()) return Servers;

	const TSharedRef<FJsonObject> Authorization = MakeShared<FJsonObject>();
	Authorization->SetStringField(ShuttlegentMcpEndpointConstant::FieldName, ShuttlegentMcpEndpointConstant::HeaderAuthorization);
	Authorization->SetStringField(ShuttlegentMcpEndpointConstant::FieldValue, FString(ShuttlegentMcpEndpointConstant::BearerPrefix) + Token);

	const TSharedRef<FJsonObject> Server = MakeShared<FJsonObject>();
	Server->SetStringField(ShuttlegentMcpEndpointConstant::FieldType, ShuttlegentMcpEndpointConstant::DeclarationTypeHttp);
	Server->SetStringField(ShuttlegentMcpEndpointConstant::FieldName, ShuttlegentMcpEndpointConstant::ServerName);
	Server->SetStringField(ShuttlegentMcpEndpointConstant::FieldUrl, GetUrl());
	Server->SetArrayField(ShuttlegentMcpEndpointConstant::FieldHeaders, { MakeShared<FJsonValueObject>(Authorization) });
	Servers.Add(MakeShared<FJsonValueObject>(Server));
	return Servers;
}

bool FShuttlegentMcpEndpoint::HandleRequest(const FHttpServerRequest& Request, const FHttpResultCallback& OnComplete)
{
	if (!IsAuthorized(Request))
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("MCP endpoint refused a request without the current token."));
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::Denied));
		return true;
	}

	// No server-to-client stream; a DELETE ends a session this endpoint does not keep
	if (Request.Verb == EHttpServerRequestVerbs::VERB_GET)
	{
		OnComplete(FHttpServerResponse::Error(EHttpServerResponseCodes::BadMethod));
		return true;
	}
	if (Request.Verb == EHttpServerRequestVerbs::VERB_DELETE)
	{
		OnComplete(FHttpServerResponse::Ok());
		return true;
	}

	const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Request.Body.GetData()), Request.Body.Num());
	const FString Body(Converted.Length(), Converted.Get());
	TSharedPtr<FJsonObject> Message;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
	if (!FJsonSerializer::Deserialize(Reader, Message) || !Message.IsValid())
	{
		OnComplete(FShuttlegentMcpEndpointPrivate::MakeJsonResponse(FShuttlegentMcpEndpointPrivate::MakeError(nullptr, ShuttlegentMcpEndpointConstant::ParseError, TEXT("Body is not a JSON-RPC object."))));
		return true;
	}

	// A notification gets no answer
	const TSharedPtr<FJsonValue> Id = Message->TryGetField(ShuttlegentMcpEndpointConstant::FieldId);
	FString Method;
	Message->TryGetStringField(ShuttlegentMcpEndpointConstant::FieldMethod, Method);
	if (!Id.IsValid())
	{
		const TSharedPtr<FJsonObject>* NotificationParams = nullptr;
		Message->TryGetObjectField(ShuttlegentMcpEndpointConstant::FieldParams, NotificationParams);
		HandleNotification(Method, NotificationParams != nullptr ? *NotificationParams : nullptr);

		TUniquePtr<FHttpServerResponse> Accepted = FHttpServerResponse::Ok();
		Accepted->Code = EHttpServerResponseCodes::Accepted;
		OnComplete(MoveTemp(Accepted));
		return true;
	}

	const TSharedPtr<FJsonObject>* Params = nullptr;
	Message->TryGetObjectField(ShuttlegentMcpEndpointConstant::FieldParams, Params);
	Dispatch(Method, Params != nullptr ? *Params : nullptr, [Id, OnComplete](const TSharedRef<FJsonObject>& Result, int32 ErrorCode, const FString& ErrorMessage)
	{
		if (ErrorCode != 0)
		{
			OnComplete(FShuttlegentMcpEndpointPrivate::MakeJsonResponse(FShuttlegentMcpEndpointPrivate::MakeError(Id, ErrorCode, ErrorMessage)));
			return;
		}

		const TSharedRef<FJsonObject> Envelope = FShuttlegentMcpEndpointPrivate::MakeEnvelope(Id);
		Envelope->SetObjectField(ShuttlegentMcpEndpointConstant::FieldResult, Result);
		OnComplete(FShuttlegentMcpEndpointPrivate::MakeJsonResponse(Envelope));
	});
	return true;
}

bool FShuttlegentMcpEndpoint::IsAuthorized(const FHttpServerRequest& Request) const
{
	const TArray<FString>* Values = Request.Headers.Find(ShuttlegentMcpEndpointConstant::HeaderAuthorization);
	if (Values == nullptr || Values->IsEmpty() || Token.IsEmpty()) return false;

	// The token is case-sensitive, unlike FString's default comparison
	return (*Values)[0].Equals(FString(ShuttlegentMcpEndpointConstant::BearerPrefix) + Token, ESearchCase::CaseSensitive);
}

FString FShuttlegentMcpEndpoint::GetUrl() const
{
	return FString::Format(ShuttlegentMcpEndpointConstant::UrlFormat, { ShuttlegentMcpEndpointConstant::Host, Port, ShuttlegentMcpEndpointConstant::Path });
}

void FShuttlegentMcpEndpoint::HandleNotification(const FString& Method, const TSharedPtr<FJsonObject>& Params) const
{
	if (Method != ShuttlegentMcpEndpointConstant::MethodContextBreakdown || !Params.IsValid() || !OnContextBreakdown) return;

	FString SessionId;
	if (!Params->TryGetStringField(ShuttlegentMcpEndpointConstant::FieldSessionId, SessionId) || SessionId.IsEmpty()) return;

	FShuttlegentContextBreakdown Breakdown;
	Params->TryGetNumberField(ShuttlegentMcpEndpointConstant::FieldSystemTokens, Breakdown.SystemTokens);
	Params->TryGetNumberField(ShuttlegentMcpEndpointConstant::FieldToolsTokens, Breakdown.ToolsTokens);
	Params->TryGetNumberField(ShuttlegentMcpEndpointConstant::FieldMessageTokens, Breakdown.MessageTokens);
	OnContextBreakdown(SessionId, Breakdown);
}

void FShuttlegentMcpEndpoint::Dispatch(const FString& Method, const TSharedPtr<FJsonObject>& Params, TFunction<void(const TSharedRef<FJsonObject>&, int32, const FString&)> OnAnswer)
{
	const TSharedRef<FJsonObject> Empty = MakeShared<FJsonObject>();
	if (Method == ShuttlegentMcpEndpointConstant::MethodInitialize)
	{
		OnAnswer(MakeInitializeResult(Params), 0, FString());
		return;
	}
	if (Method == ShuttlegentMcpEndpointConstant::MethodToolsList)
	{
		OnAnswer(MakeToolsListResult(), 0, FString());
		return;
	}
	if (Method == ShuttlegentMcpEndpointConstant::MethodPing)
	{
		OnAnswer(Empty, 0, FString());
		return;
	}
	if (Method != ShuttlegentMcpEndpointConstant::MethodToolsCall)
	{
		OnAnswer(Empty, ShuttlegentMcpEndpointConstant::MethodNotFound, FString::Printf(TEXT("Unknown method %s."), *Method));
		return;
	}

	FString Name;
	if (!Params.IsValid() || !Params->TryGetStringField(ShuttlegentMcpEndpointConstant::FieldName, Name))
	{
		OnAnswer(Empty, ShuttlegentMcpEndpointConstant::InvalidParams, TEXT("tools/call needs a tool name."));
		return;
	}

	const TSharedPtr<IShuttlegentTool> Tool = FShuttlegentToolRegistry::Find(Name);
	const TSharedPtr<FShuttlegentWriteGate> PinnedGate = Gate.Pin();
	if (!Tool.IsValid() || !PinnedGate.IsValid())
	{
		OnAnswer(Empty, ShuttlegentMcpEndpointConstant::InvalidParams, FString::Printf(TEXT("Unknown tool %s."), *Name));
		return;
	}

	// The list already hides it; a call by name still meets the same switch
	if (!FShuttlegentProjectConfig::IsToolEnabled(Name))
	{
		OnAnswer(FShuttlegentMcpEndpointPrivate::MakeToolResult(FShuttlegentToolResult::Error(FString::Printf(TEXT("The user switched %s off in the Shuttlegent settings."), *Name))), 0, FString());
		return;
	}

	const TSharedPtr<FJsonObject>* ArgumentsField = nullptr;
	const TSharedRef<FJsonObject> Arguments = Params->TryGetObjectField(ShuttlegentMcpEndpointConstant::FieldArguments, ArgumentsField) && ArgumentsField->IsValid() ? ArgumentsField->ToSharedRef() : MakeShared<FJsonObject>();
	UE_LOG(LogShuttlegent, Log, TEXT("MCP tool call: %s"), *Name);
	FShuttlegentToolRunner::Call(PinnedGate.ToSharedRef(), Tool.ToSharedRef(), Arguments, [OnAnswer](const FShuttlegentToolResult& Result)
	{
		OnAnswer(FShuttlegentMcpEndpointPrivate::MakeToolResult(Result), 0, FString());
	});
}

TSharedRef<FJsonObject> FShuttlegentMcpEndpoint::MakeInitializeResult(const TSharedPtr<FJsonObject>& Params) const
{
	FString ProtocolVersion = ShuttlegentMcpEndpointConstant::DefaultProtocolVersion;
	if (Params.IsValid()) Params->TryGetStringField(ShuttlegentMcpEndpointConstant::FieldProtocolVersion, ProtocolVersion);

	const TSharedRef<FJsonObject> Tools = MakeShared<FJsonObject>();
	Tools->SetBoolField(ShuttlegentMcpEndpointConstant::FieldListChanged, false);
	const TSharedRef<FJsonObject> Capabilities = MakeShared<FJsonObject>();
	Capabilities->SetObjectField(ShuttlegentMcpEndpointConstant::FieldTools, Tools);

	const TSharedRef<FJsonObject> ServerInfo = MakeShared<FJsonObject>();
	ServerInfo->SetStringField(ShuttlegentMcpEndpointConstant::FieldName, ShuttlegentMcpEndpointConstant::ServerInfoName);
	ServerInfo->SetStringField(ShuttlegentMcpEndpointConstant::FieldVersion, FShuttlegentMcpEndpointPrivate::GetPluginVersion());

	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(ShuttlegentMcpEndpointConstant::FieldProtocolVersion, ProtocolVersion);
	Result->SetObjectField(ShuttlegentMcpEndpointConstant::FieldCapabilities, Capabilities);
	Result->SetObjectField(ShuttlegentMcpEndpointConstant::FieldServerInfo, ServerInfo);
	Result->SetStringField(ShuttlegentMcpEndpointConstant::FieldInstructions, ShuttlegentMcpEndpointConstant::Instructions);
	return Result;
}

TSharedRef<FJsonObject> FShuttlegentMcpEndpoint::MakeToolsListResult() const
{
	TArray<TSharedPtr<FJsonValue>> Tools;
	for (const TSharedRef<IShuttlegentTool>& Tool : FShuttlegentToolRegistry::Collect())
	{
		const FShuttlegentToolDefinition& Definition = Tool->GetDefinition();
		if (!FShuttlegentProjectConfig::IsToolEnabled(Definition.Name)) continue;

		const TSharedRef<FJsonObject> Annotations = MakeShared<FJsonObject>();
		Annotations->SetBoolField(ShuttlegentMcpEndpointConstant::FieldReadOnlyHint, Definition.bReadOnly);
		Annotations->SetBoolField(ShuttlegentMcpEndpointConstant::FieldDestructiveHint, !Definition.bReadOnly);

		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(ShuttlegentMcpEndpointConstant::FieldName, Definition.Name);
		Item->SetStringField(ShuttlegentMcpEndpointConstant::FieldDescription, Definition.Description);
		Item->SetObjectField(ShuttlegentMcpEndpointConstant::FieldInputSchema, Definition.InputSchema.IsValid() ? Definition.InputSchema.ToSharedRef() : MakeShared<FJsonObject>());
		Item->SetObjectField(ShuttlegentMcpEndpointConstant::FieldAnnotations, Annotations);
		Tools.Add(MakeShared<FJsonValueObject>(Item));
	}

	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetArrayField(ShuttlegentMcpEndpointConstant::FieldTools, Tools);
	return Result;
}
