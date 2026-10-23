#include "ShuttlegentAcpConnection.h"

#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Process/ShuttlegentRuntimeProcess.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentRuntimeLaunchSpec.h"

namespace ShuttlegentAcpConnectionPrivate
{
	// JSON-RPC envelope fields
	inline const TCHAR* FieldJsonRpc = TEXT("jsonrpc");
	inline const TCHAR* FieldId = TEXT("id");
	inline const TCHAR* FieldMethod = TEXT("method");
	inline const TCHAR* FieldParams = TEXT("params");
	inline const TCHAR* FieldResult = TEXT("result");
	inline const TCHAR* FieldError = TEXT("error");
	inline const TCHAR* FieldCode = TEXT("code");
	inline const TCHAR* FieldMessage = TEXT("message");
	inline const TCHAR* JsonRpcVersion = TEXT("2.0");

	// Name of the core ticker that pumps the connection
	inline const TCHAR* TickerName = TEXT("ShuttlegentAcpConnection");

	// Maximum stdout lines dispatched per frame, so a burst of updates cannot stall the editor
	inline constexpr int32 MaxLinesPerTick = 256;

	using FCondensedWriter = TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;
	using FCondensedWriterFactory = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

	FString Serialize(const TSharedRef<FJsonObject>& Message)
	{
		FString Output;
		const TSharedRef<FCondensedWriter> Writer = FCondensedWriterFactory::Create(&Output);
		FJsonSerializer::Serialize(Message, Writer);
		return Output;
	}

	TSharedRef<FJsonObject> MakeEnvelope()
	{
		TSharedRef<FJsonObject> Message = MakeShared<FJsonObject>();
		Message->SetStringField(FieldJsonRpc, JsonRpcVersion);
		return Message;
	}

	TSharedPtr<FJsonObject> ParamsOrEmpty(const TSharedPtr<FJsonObject>& Params)
	{
		return Params.IsValid() ? Params : MakeShared<FJsonObject>();
	}
}

FShuttlegentAcpConnection::FShuttlegentAcpConnection() = default;

FShuttlegentAcpConnection::~FShuttlegentAcpConnection()
{
	Close();
}

bool FShuttlegentAcpConnection::Open(const FShuttlegentRuntimeLaunchSpec& Spec, FString& OutError)
{
	if (bOpen)
	{
		OutError = TEXT("Connection is already open.");
		return false;
	}

	Process = MakeUnique<FShuttlegentRuntimeProcess>();
	if (!Process->Launch(Spec, OutError))
	{
		Process.Reset();
		return false;
	}

	bOpen = true;
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FShuttlegentAcpConnection::Tick));
	return true;
}

void FShuttlegentAcpConnection::Close()
{
	if (!Process.IsValid()) return;

	Process->Shutdown(ShuttlegentProcessConstant::GracefulExitTimeoutSeconds);
	HandleClosed();
}

int64 FShuttlegentAcpConnection::Request(const FString& Method, const TSharedPtr<FJsonObject>& Params, FResponseCallback Callback)
{
	const int64 Id = NextRequestId++;
	if (!bOpen)
	{
		FShuttlegentAcpResponse Failure;
		Failure.ErrorCode = ShuttlegentAcpConstant::RuntimeExitedErrorCode;
		Failure.ErrorMessage = TEXT("Runtime is not running.");
		if (Callback) Callback(Failure);
		return Id;
	}

	TSharedRef<FJsonObject> Message = ShuttlegentAcpConnectionPrivate::MakeEnvelope();
	Message->SetNumberField(ShuttlegentAcpConnectionPrivate::FieldId, static_cast<double>(Id));
	Message->SetStringField(ShuttlegentAcpConnectionPrivate::FieldMethod, Method);
	Message->SetObjectField(ShuttlegentAcpConnectionPrivate::FieldParams, ShuttlegentAcpConnectionPrivate::ParamsOrEmpty(Params));

	PendingRequests.Add(Id, MoveTemp(Callback));
	Send(Message);
	return Id;
}

void FShuttlegentAcpConnection::Notify(const FString& Method, const TSharedPtr<FJsonObject>& Params)
{
	if (!bOpen) return;

	TSharedRef<FJsonObject> Message = ShuttlegentAcpConnectionPrivate::MakeEnvelope();
	Message->SetStringField(ShuttlegentAcpConnectionPrivate::FieldMethod, Method);
	Message->SetObjectField(ShuttlegentAcpConnectionPrivate::FieldParams, ShuttlegentAcpConnectionPrivate::ParamsOrEmpty(Params));
	Send(Message);
}

void FShuttlegentAcpConnection::Respond(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Result)
{
	if (!bOpen || !Id.IsValid()) return;

	TSharedRef<FJsonObject> Message = ShuttlegentAcpConnectionPrivate::MakeEnvelope();
	Message->SetField(ShuttlegentAcpConnectionPrivate::FieldId, Id);
	Message->SetObjectField(ShuttlegentAcpConnectionPrivate::FieldResult, ShuttlegentAcpConnectionPrivate::ParamsOrEmpty(Result));
	Send(Message);
}

void FShuttlegentAcpConnection::RespondError(const TSharedPtr<FJsonValue>& Id, int32 Code, const FString& Message)
{
	if (!bOpen || !Id.IsValid()) return;

	TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
	Error->SetNumberField(ShuttlegentAcpConnectionPrivate::FieldCode, Code);
	Error->SetStringField(ShuttlegentAcpConnectionPrivate::FieldMessage, Message);

	TSharedRef<FJsonObject> Envelope = ShuttlegentAcpConnectionPrivate::MakeEnvelope();
	Envelope->SetField(ShuttlegentAcpConnectionPrivate::FieldId, Id);
	Envelope->SetObjectField(ShuttlegentAcpConnectionPrivate::FieldError, Error);
	Send(Envelope);
}

bool FShuttlegentAcpConnection::Tick(float DeltaTime)
{
	if (!Process.IsValid()) return false;

	FString Line;
	while (Process->DequeueStdErrLine(Line))
	{
		UE_LOG(LogShuttlegent, Verbose, TEXT("runtime stderr: %s"), *Line);
		OnStdErr.Broadcast(Line);
	}

	int32 Dispatched = 0;
	while (Dispatched < ShuttlegentAcpConnectionPrivate::MaxLinesPerTick && Process.IsValid() && Process->DequeueStdOutLine(Line))
	{
		Dispatch(Line);
		++Dispatched;
	}

	if (!Process.IsValid()) return false;

	const bool bDrained = Dispatched < ShuttlegentAcpConnectionPrivate::MaxLinesPerTick;
	if (bDrained && !Process->IsRunning())
	{
		UE_LOG(LogShuttlegent, Log, TEXT("Runtime exited with code %d."), Process->GetExitCode());
		Process->Shutdown(0.0f);
		HandleClosed();
		return false;
	}
	return true;
}

void FShuttlegentAcpConnection::Send(const TSharedRef<FJsonObject>& Message)
{
	if (!Process.IsValid()) return;

	const FString Line = ShuttlegentAcpConnectionPrivate::Serialize(Message);
	UE_LOG(LogShuttlegent, VeryVerbose, TEXT("acp send: %s"), *Line);
	Process->EnqueueLine(Line);
}

void FShuttlegentAcpConnection::Dispatch(const FString& Line)
{
	if (Line.IsEmpty()) return;

	TSharedPtr<FJsonObject> Message;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Line);
	if (!FJsonSerializer::Deserialize(Reader, Message) || !Message.IsValid())
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Ignoring non-JSON runtime output: %s"), *Line.Left(200));
		return;
	}

	UE_LOG(LogShuttlegent, VeryVerbose, TEXT("acp recv: %s"), *Line);

	const TSharedPtr<FJsonValue> Id = Message->TryGetField(ShuttlegentAcpConnectionPrivate::FieldId);
	FString Method;
	const bool bHasMethod = Message->TryGetStringField(ShuttlegentAcpConnectionPrivate::FieldMethod, Method);
	const TSharedPtr<FJsonObject>* ParamsField = nullptr;
	Message->TryGetObjectField(ShuttlegentAcpConnectionPrivate::FieldParams, ParamsField);
	const TSharedPtr<FJsonObject> Params = ParamsField != nullptr ? *ParamsField : MakeShared<FJsonObject>();

	if (bHasMethod && Id.IsValid())
	{
		OnAgentRequest.Broadcast(Method, Id, Params);
		return;
	}
	if (bHasMethod)
	{
		OnNotification.Broadcast(Method, Params);
		return;
	}

	double IdNumber = 0.0;
	if (!Id.IsValid() || !Id->TryGetNumber(IdNumber))
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Ignoring runtime message without method or numeric id."));
		return;
	}

	FResponseCallback Callback;
	if (!PendingRequests.RemoveAndCopyValue(static_cast<int64>(IdNumber), Callback)) return;

	FShuttlegentAcpResponse Response;
	const TSharedPtr<FJsonObject>* ResultField = nullptr;
	const TSharedPtr<FJsonObject>* ErrorField = nullptr;
	if (Message->TryGetObjectField(ShuttlegentAcpConnectionPrivate::FieldResult, ResultField))
	{
		Response.bSuccess = true;
		Response.Result = *ResultField;
	}
	else if (Message->TryGetObjectField(ShuttlegentAcpConnectionPrivate::FieldError, ErrorField))
	{
		(*ErrorField)->TryGetNumberField(ShuttlegentAcpConnectionPrivate::FieldCode, Response.ErrorCode);
		(*ErrorField)->TryGetStringField(ShuttlegentAcpConnectionPrivate::FieldMessage, Response.ErrorMessage);
	}
	else
	{
		// "result": null and other non-object results count as success without fields.
		Response.bSuccess = Message->HasField(ShuttlegentAcpConnectionPrivate::FieldResult);
	}

	if (Callback) Callback(Response);
}

void FShuttlegentAcpConnection::HandleClosed()
{
	if (TickerHandle.IsValid())
	{
		FTSTicker::RemoveTicker(TickerHandle);
		TickerHandle.Reset();
	}

	const int32 ExitCode = Process.IsValid() ? Process->GetExitCode() : INDEX_NONE;
	Process.Reset();

	const bool bWasOpen = bOpen;
	bOpen = false;

	TMap<int64, FResponseCallback> Pending = MoveTemp(PendingRequests);
	PendingRequests.Reset();
	for (TPair<int64, FResponseCallback>& Entry : Pending)
	{
		FShuttlegentAcpResponse Failure;
		Failure.ErrorCode = ShuttlegentAcpConstant::RuntimeExitedErrorCode;
		Failure.ErrorMessage = TEXT("Runtime exited before answering.");
		if (Entry.Value) Entry.Value(Failure);
	}

	if (bWasOpen) OnClosed.Broadcast(ExitCode);
}
