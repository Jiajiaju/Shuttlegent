#include "ShuttlegentAgentService.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

#include "ShuttlegentAcpConnection.h"
#include "ShuttlegentAcpResponse.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentRuntimeLaunchSpec.h"
#include "ShuttlegentSettings.h"

namespace ShuttlegentAgentServicePrivate
{
	// ACP request and response fields
	inline const TCHAR* FieldProtocolVersion = TEXT("protocolVersion");
	inline const TCHAR* FieldClientCapabilities = TEXT("clientCapabilities");
	inline const TCHAR* FieldClientInfo = TEXT("clientInfo");
	inline const TCHAR* FieldFs = TEXT("fs");
	inline const TCHAR* FieldReadTextFile = TEXT("readTextFile");
	inline const TCHAR* FieldWriteTextFile = TEXT("writeTextFile");
	inline const TCHAR* FieldTerminal = TEXT("terminal");
	inline const TCHAR* FieldName = TEXT("name");
	inline const TCHAR* FieldVersion = TEXT("version");
	inline const TCHAR* FieldSessionId = TEXT("sessionId");
	inline const TCHAR* FieldCwd = TEXT("cwd");
	inline const TCHAR* FieldMcpServers = TEXT("mcpServers");
	inline const TCHAR* FieldConfigOptions = TEXT("configOptions");
	inline const TCHAR* FieldConfigId = TEXT("configId");
	inline const TCHAR* FieldValue = TEXT("value");
	inline const TCHAR* FieldId = TEXT("id");
	inline const TCHAR* FieldCurrentValue = TEXT("currentValue");
	inline const TCHAR* FieldPrompt = TEXT("prompt");
	inline const TCHAR* FieldType = TEXT("type");
	inline const TCHAR* FieldText = TEXT("text");
	inline const TCHAR* FieldStopReason = TEXT("stopReason");
	inline const TCHAR* FieldError = TEXT("error");
	inline const TCHAR* FieldUpdate = TEXT("update");
	inline const TCHAR* FieldSessionUpdate = TEXT("sessionUpdate");
	inline const TCHAR* FieldOptions = TEXT("options");
	inline const TCHAR* FieldOptionId = TEXT("optionId");
	inline const TCHAR* FieldKind = TEXT("kind");
	inline const TCHAR* FieldOutcome = TEXT("outcome");
	inline const TCHAR* FieldToolCall = TEXT("toolCall");
	inline const TCHAR* FieldToolCallId = TEXT("toolCallId");
	inline const TCHAR* FieldAllowed = TEXT("allowed");

	inline const TCHAR* ContentTypeText = TEXT("text");
	inline const TCHAR* OutcomeSelected = TEXT("selected");
	inline const TCHAR* OutcomeCancelled = TEXT("cancelled");
	inline const TCHAR* ConfigIdModel = TEXT("model");

	// session/update kinds that belong to a running turn and are dropped once it ended
	inline const TCHAR* TurnUpdateKinds[] =
	{
		TEXT("agent_message_chunk"),
		TEXT("agent_thought_chunk"),
		TEXT("tool_call"),
		TEXT("tool_call_update"),
	};

	// Transcript entry kinds
	inline const TCHAR* TranscriptPrompt = TEXT("prompt");
	inline const TCHAR* TranscriptUpdate = TEXT("update");
	inline const TCHAR* TranscriptTurnEnd = TEXT("turn_end");
	inline const TCHAR* TranscriptPermission = TEXT("permission");

	// clientInfo.name sent with initialize
	inline const TCHAR* ClientName = TEXT("shuttlegent");

	bool IsTurnUpdate(const FString& Kind)
	{
		for (const TCHAR* TurnKind : TurnUpdateKinds)
		{
			if (Kind == TurnKind) return true;
		}
		return false;
	}

	FString GetPluginVersion()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ShuttlegentPath::PluginName);
		return Plugin.IsValid() ? Plugin->GetDescriptor().VersionName : FString();
	}

	/** Display name of Value among a config option's choices, searching one level of grouped choices; empty when not listed. */
	FString FindChoiceName(const TArray<TSharedPtr<FJsonValue>>& Choices, const FString& Value)
	{
		for (const TSharedPtr<FJsonValue>& ChoiceValue : Choices)
		{
			const TSharedPtr<FJsonObject> Choice = ChoiceValue.IsValid() ? ChoiceValue->AsObject() : nullptr;
			if (!Choice.IsValid()) continue;

			const TArray<TSharedPtr<FJsonValue>>* GroupChoices = nullptr;
			if (Choice->TryGetArrayField(FieldOptions, GroupChoices))
			{
				const FString GroupName = FindChoiceName(*GroupChoices, Value);
				if (!GroupName.IsEmpty()) return GroupName;
				continue;
			}

			FString ChoiceId;
			FString Name;
			if (!Choice->TryGetStringField(FieldValue, ChoiceId) || ChoiceId != Value) continue;

			Choice->TryGetStringField(FieldName, Name);
			return Name;
		}
		return FString();
	}

	FString MakeTitle(const FString& Prompt)
	{
		FString Title = Prompt.Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" ")).TrimStartAndEnd();
		if (Title.Len() > ShuttlegentSessionConstant::TitleMaxChars) Title = Title.Left(ShuttlegentSessionConstant::TitleMaxChars) + TEXT("...");
		return Title;
	}
}

FShuttlegentAgentService::FShuttlegentAgentService() = default;

FShuttlegentAgentService::~FShuttlegentAgentService()
{
	bStopRequested = true;
	Connection.Reset();
}

bool FShuttlegentAgentService::Start(FString& OutError)
{
	if (State != EShuttlegentAgentState::Stopped)
	{
		OutError = TEXT("Runtime is already running.");
		return false;
	}

	FShuttlegentRuntimeLaunchSpec Spec;
	if (!FShuttlegentRuntimeLaunchSpec::Build(Spec, OutError)) return false;

	Connection = MakeUnique<FShuttlegentAcpConnection>();
	Connection->OnNotification.AddSP(this, &FShuttlegentAgentService::HandleNotification);
	Connection->OnAgentRequest.AddSP(this, &FShuttlegentAgentService::HandleAgentRequest);
	Connection->OnClosed.AddSP(this, &FShuttlegentAgentService::HandleRuntimeClosed);
	if (!Connection->Open(Spec, OutError))
	{
		Connection.Reset();
		return false;
	}

	bStopRequested = false;
	ActiveSessionId.Reset();
	ConfigOptions.Reset();
	SetState(EShuttlegentAgentState::Starting);

	TSharedRef<FJsonObject> Fs = MakeShared<FJsonObject>();
	Fs->SetBoolField(ShuttlegentAgentServicePrivate::FieldReadTextFile, false);
	Fs->SetBoolField(ShuttlegentAgentServicePrivate::FieldWriteTextFile, false);

	TSharedRef<FJsonObject> Capabilities = MakeShared<FJsonObject>();
	Capabilities->SetObjectField(ShuttlegentAgentServicePrivate::FieldFs, Fs);
	Capabilities->SetBoolField(ShuttlegentAgentServicePrivate::FieldTerminal, false);

	TSharedRef<FJsonObject> ClientInfo = MakeShared<FJsonObject>();
	ClientInfo->SetStringField(ShuttlegentAgentServicePrivate::FieldName, ShuttlegentAgentServicePrivate::ClientName);
	ClientInfo->SetStringField(ShuttlegentAgentServicePrivate::FieldVersion, ShuttlegentAgentServicePrivate::GetPluginVersion());

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetNumberField(ShuttlegentAgentServicePrivate::FieldProtocolVersion, ShuttlegentAcpConstant::ProtocolVersion);
	Params->SetObjectField(ShuttlegentAgentServicePrivate::FieldClientCapabilities, Capabilities);
	Params->SetObjectField(ShuttlegentAgentServicePrivate::FieldClientInfo, ClientInfo);

	TWeakPtr<FShuttlegentAgentService> WeakThis = AsShared();
	Connection->Request(ShuttlegentAcpMethod::Initialize, Params, [WeakThis](const FShuttlegentAcpResponse& Response)
	{
		const TSharedPtr<FShuttlegentAgentService> This = WeakThis.Pin();
		if (!This.IsValid() || This->State != EShuttlegentAgentState::Starting) return;

		if (!Response.bSuccess)
		{
			This->OnError.Broadcast(FString::Printf(TEXT("initialize failed: %s"), *Response.ErrorMessage));
			This->Stop();
			return;
		}

		UE_LOG(LogShuttlegent, Log, TEXT("Runtime initialized."));
		This->SetState(EShuttlegentAgentState::Ready);
	});
	return true;
}

void FShuttlegentAgentService::Stop()
{
	if (!Connection.IsValid()) return;

	bStopRequested = true;
	++TurnGeneration;
	Connection->Close();
}

bool FShuttlegentAgentService::NewSession(FString& OutError)
{
	if (State != EShuttlegentAgentState::Ready)
	{
		OutError = TEXT("Runtime is not ready.");
		return false;
	}

	TSharedRef<FJsonObject> Params = MakeSessionParams(FString());
	TWeakPtr<FShuttlegentAgentService> WeakThis = AsShared();
	Connection->Request(ShuttlegentAcpMethod::SessionNew, Params, [WeakThis](const FShuttlegentAcpResponse& Response)
	{
		const TSharedPtr<FShuttlegentAgentService> This = WeakThis.Pin();
		if (!This.IsValid()) return;

		FString SessionId;
		if (!Response.bSuccess || !Response.Result.IsValid() || !Response.Result->TryGetStringField(ShuttlegentAgentServicePrivate::FieldSessionId, SessionId))
		{
			This->OnError.Broadcast(FString::Printf(TEXT("session/new failed: %s"), *Response.ErrorMessage));
			return;
		}

		This->ApplySessionResult(SessionId, Response.Result, true);
	});
	return true;
}

bool FShuttlegentAgentService::ResumeSession(const FString& SessionId, FString& OutError)
{
	if (State != EShuttlegentAgentState::Ready)
	{
		OutError = TEXT("Runtime is not ready.");
		return false;
	}
	if (SessionId.IsEmpty())
	{
		OutError = TEXT("Session id is empty.");
		return false;
	}

	TSharedRef<FJsonObject> Params = MakeSessionParams(SessionId);
	TWeakPtr<FShuttlegentAgentService> WeakThis = AsShared();
	Connection->Request(ShuttlegentAcpMethod::SessionResume, Params, [WeakThis, SessionId](const FShuttlegentAcpResponse& Response)
	{
		const TSharedPtr<FShuttlegentAgentService> This = WeakThis.Pin();
		if (!This.IsValid()) return;

		if (!Response.bSuccess)
		{
			This->OnError.Broadcast(FString::Printf(TEXT("session/resume failed: %s"), *Response.ErrorMessage));
			return;
		}

		This->ApplySessionResult(SessionId, Response.Result, false);
	});
	return true;
}

bool FShuttlegentAgentService::Prompt(const FString& Text, FString& OutError)
{
	if (State != EShuttlegentAgentState::Ready)
	{
		OutError = State == EShuttlegentAgentState::Prompting ? TEXT("A prompt is already running.") : TEXT("Runtime is not ready.");
		return false;
	}
	if (ActiveSessionId.IsEmpty())
	{
		OutError = TEXT("No active session.");
		return false;
	}
	if (Text.TrimStartAndEnd().IsEmpty())
	{
		OutError = TEXT("Prompt is empty.");
		return false;
	}

	const FShuttlegentSessionRecord* Existing = SessionStore.FindRecord(ActiveSessionId);
	if (Existing != nullptr && Existing->Title.IsEmpty())
	{
		FShuttlegentSessionRecord Titled = *Existing;
		Titled.Title = ShuttlegentAgentServicePrivate::MakeTitle(Text);
		SessionStore.UpsertRecord(Titled);
	}

	TSharedRef<FJsonObject> Block = MakeShared<FJsonObject>();
	Block->SetStringField(ShuttlegentAgentServicePrivate::FieldType, ShuttlegentAgentServicePrivate::ContentTypeText);
	Block->SetStringField(ShuttlegentAgentServicePrivate::FieldText, Text);

	TArray<TSharedPtr<FJsonValue>> PromptBlocks;
	PromptBlocks.Add(MakeShared<FJsonValueObject>(Block));

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(ShuttlegentAgentServicePrivate::FieldSessionId, ActiveSessionId);
	Params->SetArrayField(ShuttlegentAgentServicePrivate::FieldPrompt, PromptBlocks);

	TSharedRef<FJsonObject> TranscriptEntry = MakeShared<FJsonObject>();
	TranscriptEntry->SetStringField(ShuttlegentAgentServicePrivate::FieldText, Text);
	SessionStore.AppendTranscript(ActiveSessionId, ShuttlegentAgentServicePrivate::TranscriptPrompt, TranscriptEntry);

	const uint64 Generation = ++TurnGeneration;
	const FString SessionId = ActiveSessionId;
	SetState(EShuttlegentAgentState::Prompting);

	TWeakPtr<FShuttlegentAgentService> WeakThis = AsShared();
	Connection->Request(ShuttlegentAcpMethod::SessionPrompt, Params, [WeakThis, Generation, SessionId](const FShuttlegentAcpResponse& Response)
	{
		const TSharedPtr<FShuttlegentAgentService> This = WeakThis.Pin();
		if (!This.IsValid() || Generation != This->TurnGeneration) return;

		FString StopReason;
		if (Response.bSuccess && Response.Result.IsValid()) Response.Result->TryGetStringField(ShuttlegentAgentServicePrivate::FieldStopReason, StopReason);

		TSharedRef<FJsonObject> TurnEnd = MakeShared<FJsonObject>();
		TurnEnd->SetStringField(ShuttlegentAgentServicePrivate::FieldStopReason, StopReason);
		if (!Response.bSuccess) TurnEnd->SetStringField(ShuttlegentAgentServicePrivate::FieldError, Response.ErrorMessage);
		This->SessionStore.AppendTranscript(SessionId, ShuttlegentAgentServicePrivate::TranscriptTurnEnd, TurnEnd);

		if (This->State == EShuttlegentAgentState::Prompting) This->SetState(EShuttlegentAgentState::Ready);
		This->OnTurnFinished.Broadcast(SessionId, StopReason, Response.bSuccess ? FString() : Response.ErrorMessage);
	});
	return true;
}

void FShuttlegentAgentService::Cancel()
{
	if (State != EShuttlegentAgentState::Prompting || !Connection.IsValid()) return;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(ShuttlegentAgentServicePrivate::FieldSessionId, ActiveSessionId);
	Connection->Notify(ShuttlegentAcpMethod::SessionCancel, Params);
}

bool FShuttlegentAgentService::SetConfigOption(const FString& ConfigId, const FString& Value, FString& OutError)
{
	if (State != EShuttlegentAgentState::Ready || ActiveSessionId.IsEmpty())
	{
		OutError = TEXT("No idle active session.");
		return false;
	}

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(ShuttlegentAgentServicePrivate::FieldSessionId, ActiveSessionId);
	Params->SetStringField(ShuttlegentAgentServicePrivate::FieldConfigId, ConfigId);
	Params->SetStringField(ShuttlegentAgentServicePrivate::FieldValue, Value);

	TWeakPtr<FShuttlegentAgentService> WeakThis = AsShared();
	const FString SessionId = ActiveSessionId;
	Connection->Request(ShuttlegentAcpMethod::SessionSetConfigOption, Params, [WeakThis, SessionId, ConfigId](const FShuttlegentAcpResponse& Response)
	{
		const TSharedPtr<FShuttlegentAgentService> This = WeakThis.Pin();
		if (!This.IsValid()) return;

		if (!Response.bSuccess)
		{
			This->OnError.Broadcast(FString::Printf(TEXT("set_config_option %s failed: %s"), *ConfigId, *Response.ErrorMessage));
			return;
		}

		const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
		if (Response.Result.IsValid() && Response.Result->TryGetArrayField(ShuttlegentAgentServicePrivate::FieldConfigOptions, Options)) This->ConfigOptions = *Options;
		This->OnConfigOptionsChanged.Broadcast();

		const FShuttlegentSessionRecord* Existing = This->SessionStore.FindRecord(SessionId);
		if (Existing != nullptr)
		{
			FShuttlegentSessionRecord Updated = *Existing;
			This->GetCurrentModel(Updated.Model, Updated.ModelName);
			This->SessionStore.UpsertRecord(Updated);
		}
		UE_LOG(LogShuttlegent, Log, TEXT("Config option %s updated."), *ConfigId);
	});
	return true;
}

void FShuttlegentAgentService::CloseSession()
{
	if (ActiveSessionId.IsEmpty() || !Connection.IsValid() || !Connection->IsOpen()) return;

	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	Params->SetStringField(ShuttlegentAgentServicePrivate::FieldSessionId, ActiveSessionId);
	Connection->Request(ShuttlegentAcpMethod::SessionClose, Params, nullptr);

	++TurnGeneration;
	ActiveSessionId.Reset();
	if (State == EShuttlegentAgentState::Prompting) SetState(EShuttlegentAgentState::Ready);
}

void FShuttlegentAgentService::SetState(EShuttlegentAgentState NewState)
{
	if (State == NewState) return;

	State = NewState;
	OnStateChanged.Broadcast(State);
}

TSharedRef<FJsonObject> FShuttlegentAgentService::MakeSessionParams(const FString& SessionId) const
{
	TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
	if (!SessionId.IsEmpty()) Params->SetStringField(ShuttlegentAgentServicePrivate::FieldSessionId, SessionId);
	Params->SetStringField(ShuttlegentAgentServicePrivate::FieldCwd, FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
	Params->SetArrayField(ShuttlegentAgentServicePrivate::FieldMcpServers, TArray<TSharedPtr<FJsonValue>>());
	return Params;
}

void FShuttlegentAgentService::ApplySessionResult(const FString& SessionId, const TSharedPtr<FJsonObject>& Result, bool bCreated)
{
	ActiveSessionId = SessionId;
	ConfigOptions.Reset();

	const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
	if (Result.IsValid() && Result->TryGetArrayField(ShuttlegentAgentServicePrivate::FieldConfigOptions, Options)) ConfigOptions = *Options;

	const FShuttlegentSessionRecord* Existing = SessionStore.FindRecord(SessionId);
	FShuttlegentSessionRecord Record = Existing != nullptr ? *Existing : FShuttlegentSessionRecord {};
	Record.SessionId = SessionId;
	Record.WorkingDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
	GetCurrentModel(Record.Model, Record.ModelName);
	if (bCreated || Existing == nullptr) Record.CreatedAt = FDateTime::UtcNow();
	SessionStore.UpsertRecord(Record);

	UE_LOG(LogShuttlegent, Log, TEXT("Session %s %s."), *SessionId, bCreated ? TEXT("created") : TEXT("resumed"));
	OnConfigOptionsChanged.Broadcast();
	OnSessionReady.Broadcast(SessionId);
}

void FShuttlegentAgentService::GetCurrentModel(FString& OutValue, FString& OutName) const
{
	OutValue.Reset();
	OutName.Reset();

	for (const TSharedPtr<FJsonValue>& Value : ConfigOptions)
	{
		const TSharedPtr<FJsonObject> Option = Value.IsValid() ? Value->AsObject() : nullptr;
		FString Id;
		if (!Option.IsValid() || !Option->TryGetStringField(ShuttlegentAgentServicePrivate::FieldId, Id) || Id != ShuttlegentAgentServicePrivate::ConfigIdModel) continue;

		Option->TryGetStringField(ShuttlegentAgentServicePrivate::FieldCurrentValue, OutValue);

		const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
		if (Option->TryGetArrayField(ShuttlegentAgentServicePrivate::FieldOptions, Choices)) OutName = ShuttlegentAgentServicePrivate::FindChoiceName(*Choices, OutValue);
		if (OutName.IsEmpty()) OutName = OutValue;
		return;
	}
}

void FShuttlegentAgentService::HandleNotification(const FString& Method, const TSharedPtr<FJsonObject>& Params)
{
	if (Method != ShuttlegentAcpMethod::SessionUpdate || !Params.IsValid()) return;

	FString SessionId;
	const TSharedPtr<FJsonObject>* UpdateField = nullptr;
	if (!Params->TryGetStringField(ShuttlegentAgentServicePrivate::FieldSessionId, SessionId) || !Params->TryGetObjectField(ShuttlegentAgentServicePrivate::FieldUpdate, UpdateField)) return;
	if (SessionId != ActiveSessionId) return;

	const TSharedPtr<FJsonObject> Update = *UpdateField;
	FString Kind;
	Update->TryGetStringField(ShuttlegentAgentServicePrivate::FieldSessionUpdate, Kind);
	if (State != EShuttlegentAgentState::Prompting && ShuttlegentAgentServicePrivate::IsTurnUpdate(Kind))
	{
		UE_LOG(LogShuttlegent, Verbose, TEXT("Dropped late %s for session %s."), *Kind, *SessionId);
		return;
	}

	SessionStore.AppendTranscript(SessionId, ShuttlegentAgentServicePrivate::TranscriptUpdate, Update);
	OnSessionUpdate.Broadcast(SessionId, Update);
}

void FShuttlegentAgentService::HandleAgentRequest(const FString& Method, const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Params)
{
	if (Method != ShuttlegentAcpMethod::SessionRequestPermission)
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Runtime requested unsupported client method %s."), *Method);
		Connection->RespondError(Id, ShuttlegentAcpConstant::MethodNotFoundErrorCode, FString::Printf(TEXT("Client does not implement %s"), *Method));
		return;
	}

	if (PermissionHandler.IsBound())
	{
		TWeakPtr<FShuttlegentAgentService> WeakThis = AsShared();
		PermissionHandler.Execute(Params, [WeakThis, Id, Params](bool bAllow)
		{
			const TSharedPtr<FShuttlegentAgentService> This = WeakThis.Pin();
			if (This.IsValid()) This->RespondPermission(Id, Params, bAllow);
		});
		return;
	}

	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	const bool bAllow = IsValid(Settings) && Settings->PermissionPolicy == EShuttlegentPermissionPolicy::Allow;
	RespondPermission(Id, Params, bAllow);
}

void FShuttlegentAgentService::RespondPermission(const TSharedPtr<FJsonValue>& Id, const TSharedPtr<FJsonObject>& Params, bool bAllow)
{
	if (!Connection.IsValid()) return;

	const FString WantedKind = bAllow ? ShuttlegentAcpConstant::PermissionKindAllowOnce : ShuttlegentAcpConstant::PermissionKindRejectOnce;
	FString OptionId;
	const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
	if (Params.IsValid() && Params->TryGetArrayField(ShuttlegentAgentServicePrivate::FieldOptions, Options))
	{
		for (const TSharedPtr<FJsonValue>& Value : *Options)
		{
			const TSharedPtr<FJsonObject> Option = Value.IsValid() ? Value->AsObject() : nullptr;
			FString Kind;
			if (!Option.IsValid() || !Option->TryGetStringField(ShuttlegentAgentServicePrivate::FieldKind, Kind) || Kind != WantedKind) continue;

			Option->TryGetStringField(ShuttlegentAgentServicePrivate::FieldOptionId, OptionId);
			break;
		}
	}

	TSharedRef<FJsonObject> Outcome = MakeShared<FJsonObject>();
	if (OptionId.IsEmpty())
	{
		Outcome->SetStringField(ShuttlegentAgentServicePrivate::FieldOutcome, ShuttlegentAgentServicePrivate::OutcomeCancelled);
	}
	else
	{
		Outcome->SetStringField(ShuttlegentAgentServicePrivate::FieldOutcome, ShuttlegentAgentServicePrivate::OutcomeSelected);
		Outcome->SetStringField(ShuttlegentAgentServicePrivate::FieldOptionId, OptionId);
	}

	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetObjectField(ShuttlegentAgentServicePrivate::FieldOutcome, Outcome);
	Connection->Respond(Id, Result);

	FString ToolCallId;
	const TSharedPtr<FJsonObject>* ToolCall = nullptr;
	if (Params.IsValid() && Params->TryGetObjectField(ShuttlegentAgentServicePrivate::FieldToolCall, ToolCall)) (*ToolCall)->TryGetStringField(ShuttlegentAgentServicePrivate::FieldToolCallId, ToolCallId);

	TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(ShuttlegentAgentServicePrivate::FieldToolCallId, ToolCallId);
	Entry->SetBoolField(ShuttlegentAgentServicePrivate::FieldAllowed, bAllow && !OptionId.IsEmpty());
	SessionStore.AppendTranscript(ActiveSessionId, ShuttlegentAgentServicePrivate::TranscriptPermission, Entry);
	UE_LOG(LogShuttlegent, Log, TEXT("Permission request for tool call %s answered: %s."), *ToolCallId, bAllow ? TEXT("allow") : TEXT("reject"));
}

void FShuttlegentAgentService::HandleRuntimeClosed(int32 ExitCode)
{
	const bool bExpected = bStopRequested;
	++TurnGeneration;
	SetState(EShuttlegentAgentState::Stopped);

	if (!bExpected) UE_LOG(LogShuttlegent, Warning, TEXT("Runtime exited unexpectedly (code %d). Start it again and resume session %s."), ExitCode, *ActiveSessionId);
	OnRuntimeExited.Broadcast(ExitCode, bExpected);
}
