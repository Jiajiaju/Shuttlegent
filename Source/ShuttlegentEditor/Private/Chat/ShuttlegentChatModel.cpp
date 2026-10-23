#include "Chat/ShuttlegentChatModel.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "ShuttlegentAgentService.h"
#include "ShuttlegentSessionStore.h"

#define LOCTEXT_NAMESPACE "ShuttlegentChatModel"

namespace ShuttlegentChatModelPrivate
{
	// session/update fields
	inline const TCHAR* FieldSessionUpdate = TEXT("sessionUpdate");
	inline const TCHAR* FieldMessageId = TEXT("messageId");
	inline const TCHAR* FieldContent = TEXT("content");
	inline const TCHAR* FieldText = TEXT("text");
	inline const TCHAR* FieldToolCallId = TEXT("toolCallId");
	inline const TCHAR* FieldTitle = TEXT("title");
	inline const TCHAR* FieldStatus = TEXT("status");
	inline const TCHAR* FieldRawInput = TEXT("rawInput");
	inline const TCHAR* FieldJustification = TEXT("justification");
	inline const TCHAR* FieldUsed = TEXT("used");
	inline const TCHAR* FieldSize = TEXT("size");
	inline const TCHAR* FieldToolCall = TEXT("toolCall");

	// session/update kinds
	inline const TCHAR* KindMessageChunk = TEXT("agent_message_chunk");
	inline const TCHAR* KindThoughtChunk = TEXT("agent_thought_chunk");
	inline const TCHAR* KindToolCall = TEXT("tool_call");
	inline const TCHAR* KindToolCallUpdate = TEXT("tool_call_update");
	inline const TCHAR* KindUsageUpdate = TEXT("usage_update");

	// Config option fields
	inline const TCHAR* FieldId = TEXT("id");
	inline const TCHAR* FieldName = TEXT("name");
	inline const TCHAR* FieldValue = TEXT("value");
	inline const TCHAR* FieldOptions = TEXT("options");
	inline const TCHAR* FieldCurrentValue = TEXT("currentValue");

	// Transcript entry fields and kinds (see FShuttlegentSessionStore)
	inline const TCHAR* FieldTime = TEXT("t");
	inline const TCHAR* FieldKind = TEXT("kind");
	inline const TCHAR* FieldData = TEXT("data");
	inline const TCHAR* FieldStopReason = TEXT("stopReason");
	inline const TCHAR* FieldError = TEXT("error");
	inline const TCHAR* FieldAllowed = TEXT("allowed");
	inline const TCHAR* TranscriptPrompt = TEXT("prompt");
	inline const TCHAR* TranscriptUpdate = TEXT("update");
	inline const TCHAR* TranscriptTurnEnd = TEXT("turn_end");
	inline const TCHAR* TranscriptPermission = TEXT("permission");

	// Separator between the text parts of one tool result
	inline const TCHAR* ToolContentJoiner = TEXT("\n");

	// Stop reasons
	inline const TCHAR* StopReasonCancelled = TEXT("cancelled");
	inline const TCHAR* StopReasonEndTurn = TEXT("end_turn");

	FString ChunkText(const TSharedPtr<FJsonObject>& Update)
	{
		const TSharedPtr<FJsonObject>* Content = nullptr;
		FString Text;
		if (Update->TryGetObjectField(FieldContent, Content)) (*Content)->TryGetStringField(FieldText, Text);
		return Text;
	}

	/** Text of a tool_call(_update) content array: [{ type: "content", content: { type: "text", text } }]. Empty when absent. */
	FString ToolContentText(const TSharedPtr<FJsonObject>& Update)
	{
		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!Update->TryGetArrayField(FieldContent, Items) || Items == nullptr) return FString();

		TArray<FString> Parts;
		for (const TSharedPtr<FJsonValue>& Item : *Items)
		{
			const TSharedPtr<FJsonObject> Object = Item.IsValid() ? Item->AsObject() : nullptr;
			if (!Object.IsValid()) continue;

			const FString Text = ChunkText(Object);
			if (!Text.IsEmpty()) Parts.Add(Text);
		}
		return FString::Join(Parts, ToolContentJoiner);
	}

	FString CompactJson(const TSharedPtr<FJsonObject>& Object)
	{
		FString Json;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
		FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
		return Json;
	}

	void AddChoice(const TSharedPtr<FJsonObject>& Option, const FString& GroupName, TArray<FShuttlegentConfigChoice>& OutChoices)
	{
		FShuttlegentConfigChoice Choice;
		if (!Option->TryGetStringField(FieldValue, Choice.Value)) return;

		Option->TryGetStringField(FieldName, Choice.Name);
		if (Choice.Name.IsEmpty()) Choice.Name = Choice.Value;
		if (!GroupName.IsEmpty()) Choice.Name = GroupName + TEXT(" / ") + Choice.Name;
		OutChoices.Add(MoveTemp(Choice));
	}
}

void FShuttlegentChatModel::Initialize(const TSharedRef<FShuttlegentAgentService>& InService)
{
	Service = InService;
	InService->OnStateChanged.AddSP(this, &FShuttlegentChatModel::HandleStateChanged);
	InService->OnSessionReady.AddSP(this, &FShuttlegentChatModel::HandleSessionReady);
	InService->OnSessionUpdate.AddSP(this, &FShuttlegentChatModel::HandleSessionUpdate);
	InService->OnTurnFinished.AddSP(this, &FShuttlegentChatModel::HandleTurnFinished);
	InService->OnRuntimeExited.AddSP(this, &FShuttlegentChatModel::HandleRuntimeExited);
	InService->OnError.AddSP(this, &FShuttlegentChatModel::HandleError);
	InService->OnConfigOptionsChanged.AddSP(this, &FShuttlegentChatModel::HandleConfigOptionsChanged);
}

void FShuttlegentChatModel::Shutdown()
{
	SetPermissionHandlerAttached(false);

	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (Pinned.IsValid())
	{
		Pinned->OnStateChanged.RemoveAll(this);
		Pinned->OnSessionReady.RemoveAll(this);
		Pinned->OnSessionUpdate.RemoveAll(this);
		Pinned->OnTurnFinished.RemoveAll(this);
		Pinned->OnRuntimeExited.RemoveAll(this);
		Pinned->OnError.RemoveAll(this);
		Pinned->OnConfigOptionsChanged.RemoveAll(this);
	}
	Service.Reset();
}

EShuttlegentAgentState FShuttlegentChatModel::GetState() const
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	return Pinned.IsValid() ? Pinned->GetState() : EShuttlegentAgentState::Stopped;
}

void FShuttlegentChatModel::EnsureRuntime()
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || Pinned->GetState() != EShuttlegentAgentState::Stopped) return;

	FString Error;
	if (!Pinned->Start(Error)) AddNotice(FText::Format(LOCTEXT("StartFailed", "启动失败：{0}"), FText::FromString(Error)), true);
	OnStatusChanged.Broadcast();
}

void FShuttlegentChatModel::SubmitPrompt(const FString& Text)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || Text.TrimStartAndEnd().IsEmpty()) return;

	if (Pinned->GetState() == EShuttlegentAgentState::Ready && !Pinned->GetActiveSessionId().IsEmpty())
	{
		SendPrompt(Text);
		return;
	}

	PendingPrompt = Text;
	if (Pinned->GetState() == EShuttlegentAgentState::Stopped)
	{
		EnsureRuntime();
		return;
	}
	if (Pinned->GetState() == EShuttlegentAgentState::Ready)
	{
		FString Error;
		if (!Pinned->NewSession(Error)) AddNotice(FText::Format(LOCTEXT("NewSessionFailed", "新建会话失败：{0}"), FText::FromString(Error)), true);
	}
}

void FShuttlegentChatModel::Cancel()
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (Pinned.IsValid()) Pinned->Cancel();
}

void FShuttlegentChatModel::NewConversation()
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (Pinned.IsValid()) Pinned->CloseSession();

	PendingPrompt.Reset();
	PendingResumeSessionId.Reset();
	LastSessionId.Reset();
	UsedTokens = 0;
	ContextSize = 0;
	ResetEntries();
	OnStatusChanged.Broadcast();
}

void FShuttlegentChatModel::OpenSession(const FString& SessionId)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || SessionId.IsEmpty()) return;

	Pinned->CloseSession();
	PendingPrompt.Reset();
	UsedTokens = 0;
	ContextSize = 0;
	ReplayTranscript(SessionId);

	if (Pinned->GetState() == EShuttlegentAgentState::Ready)
	{
		FString Error;
		if (!Pinned->ResumeSession(SessionId, Error)) AddNotice(FText::Format(LOCTEXT("ResumeFailed", "恢复会话失败：{0}"), FText::FromString(Error)), true);
		return;
	}

	PendingResumeSessionId = SessionId;
	EnsureRuntime();
}

void FShuttlegentChatModel::Reconnect()
{
	bRuntimeLost = false;
	PendingResumeSessionId = LastSessionId;
	EnsureRuntime();
	OnStatusChanged.Broadcast();
}

void FShuttlegentChatModel::SetPermissionHandlerAttached(bool bAttached)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || bAttached == bPermissionHandlerAttached) return;

	bPermissionHandlerAttached = bAttached;
	if (bAttached)
	{
		Pinned->PermissionHandler.BindSP(this, &FShuttlegentChatModel::HandlePermissionRequest);
		return;
	}

	Pinned->PermissionHandler.Unbind();

	// Requests the panel can no longer answer fall back to a rejection.
	TMap<int32, TFunction<void(bool)>> Pending = MoveTemp(PendingPermissions);
	PendingPermissions.Reset();
	for (TPair<int32, TFunction<void(bool)>>& Entry : Pending)
	{
		if (Entry.Value) Entry.Value(false);
	}
}

void FShuttlegentChatModel::AnswerPermission(int32 EntryId, bool bAllow)
{
	TFunction<void(bool)> Respond;
	if (!PendingPermissions.RemoveAndCopyValue(EntryId, Respond)) return;

	for (const TSharedPtr<FShuttlegentChatEntry>& Entry : Entries)
	{
		if (Entry->Id != EntryId) continue;

		Entry->bAnswered = true;
		Entry->bAllowed = bAllow;
		break;
	}

	if (Respond) Respond(bAllow);
	OnContentChanged.Broadcast();
}

bool FShuttlegentChatModel::GetConfigChoices(const FString& ConfigId, TArray<FShuttlegentConfigChoice>& OutChoices, FString& OutCurrentValue) const
{
	OutChoices.Reset();
	OutCurrentValue.Reset();

	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return false;

	for (const TSharedPtr<FJsonValue>& Value : Pinned->GetConfigOptions())
	{
		const TSharedPtr<FJsonObject> Option = Value.IsValid() ? Value->AsObject() : nullptr;
		FString Id;
		if (!Option.IsValid() || !Option->TryGetStringField(ShuttlegentChatModelPrivate::FieldId, Id) || Id != ConfigId) continue;

		Option->TryGetStringField(ShuttlegentChatModelPrivate::FieldCurrentValue, OutCurrentValue);

		const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
		if (!Option->TryGetArrayField(ShuttlegentChatModelPrivate::FieldOptions, Items)) return true;

		for (const TSharedPtr<FJsonValue>& ItemValue : *Items)
		{
			const TSharedPtr<FJsonObject> Item = ItemValue.IsValid() ? ItemValue->AsObject() : nullptr;
			if (!Item.IsValid()) continue;

			const TArray<TSharedPtr<FJsonValue>>* GroupItems = nullptr;
			if (!Item->TryGetArrayField(ShuttlegentChatModelPrivate::FieldOptions, GroupItems))
			{
				ShuttlegentChatModelPrivate::AddChoice(Item, FString(), OutChoices);
				continue;
			}

			FString GroupName;
			Item->TryGetStringField(ShuttlegentChatModelPrivate::FieldName, GroupName);
			for (const TSharedPtr<FJsonValue>& GroupValue : *GroupItems)
			{
				const TSharedPtr<FJsonObject> GroupItem = GroupValue.IsValid() ? GroupValue->AsObject() : nullptr;
				if (GroupItem.IsValid()) ShuttlegentChatModelPrivate::AddChoice(GroupItem, GroupName, OutChoices);
			}
		}
		return true;
	}
	return false;
}

void FShuttlegentChatModel::SetConfigValue(const FString& ConfigId, const FString& Value)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	FString Error;
	if (!Pinned->SetConfigOption(ConfigId, Value, Error)) AddNotice(FText::Format(LOCTEXT("ConfigFailed", "切换失败：{0}"), FText::FromString(Error)), true);
}

TArray<TPair<FString, FString>> FShuttlegentChatModel::GetRecentSessions() const
{
	TArray<TPair<FString, FString>> Sessions;
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return Sessions;

	for (const FShuttlegentSessionRecord& Record : Pinned->GetSessionStore().GetRecords())
	{
		const FString Title = Record.Title.IsEmpty() ? Record.SessionId : Record.Title;
		Sessions.Emplace(Record.SessionId, FString::Printf(TEXT("%s  (%s)"), *Title, *Record.UpdatedAt.ToString(TEXT("%Y-%m-%d %H:%M"))));
	}
	return Sessions;
}

TSharedPtr<FShuttlegentChatEntry> FShuttlegentChatModel::AddEntry(EShuttlegentChatEntryKind Kind, const FString& Text)
{
	TSharedPtr<FShuttlegentChatEntry> Entry = MakeShared<FShuttlegentChatEntry>();
	Entry->Kind = Kind;
	Entry->Text = Text;
	Entry->Id = NextEntryId++;
	Entries.Add(Entry);
	return Entry;
}

TSharedPtr<FShuttlegentChatEntry> FShuttlegentChatModel::FindToolCall(const FString& ToolCallId) const
{
	for (int32 Index = Entries.Num() - 1; Index >= 0; --Index)
	{
		const TSharedPtr<FShuttlegentChatEntry>& Entry = Entries[Index];
		if (Entry->Kind == EShuttlegentChatEntryKind::ToolCall && Entry->ToolCallId == ToolCallId) return Entry;
	}
	return nullptr;
}

void FShuttlegentChatModel::AddNotice(const FText& Text, bool bError)
{
	TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(EShuttlegentChatEntryKind::Notice, Text.ToString());
	Entry->bError = bError;
	OnStructureChanged.Broadcast();
}

void FShuttlegentChatModel::ResetEntries()
{
	// Requests whose rows disappear are rejected rather than left hanging.
	TMap<int32, TFunction<void(bool)>> Pending = MoveTemp(PendingPermissions);
	PendingPermissions.Reset();
	for (TPair<int32, TFunction<void(bool)>>& Entry : Pending)
	{
		if (Entry.Value) Entry.Value(false);
	}

	Entries.Reset();
	OnStructureChanged.Broadcast();
}

void FShuttlegentChatModel::SendPrompt(const FString& Text)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	FString Error;
	if (!Pinned->Prompt(Text, Error))
	{
		AddNotice(FText::Format(LOCTEXT("PromptFailed", "发送失败：{0}"), FText::FromString(Error)), true);
		return;
	}

	AddEntry(EShuttlegentChatEntryKind::User, Text);
	OnStructureChanged.Broadcast();
	OnStatusChanged.Broadcast();
}

bool FShuttlegentChatModel::ApplyUpdate(const TSharedPtr<FJsonObject>& Update, const FDateTime& At)
{
	FString Kind;
	Update->TryGetStringField(ShuttlegentChatModelPrivate::FieldSessionUpdate, Kind);

	if (Kind == ShuttlegentChatModelPrivate::KindMessageChunk || Kind == ShuttlegentChatModelPrivate::KindThoughtChunk)
	{
		FString MessageId;
		Update->TryGetStringField(ShuttlegentChatModelPrivate::FieldMessageId, MessageId);

		const TSharedPtr<FShuttlegentChatEntry> Last = Entries.Num() > 0 ? Entries.Last() : nullptr;
		const bool bContinues = Last.IsValid() && Last->Kind == EShuttlegentChatEntryKind::Assistant && Last->MessageId == MessageId;
		const TSharedPtr<FShuttlegentChatEntry> Target = bContinues ? Last : AddEntry(EShuttlegentChatEntryKind::Assistant, FString());
		Target->MessageId = MessageId;

		const FString Chunk = ShuttlegentChatModelPrivate::ChunkText(Update);
		if (Kind == ShuttlegentChatModelPrivate::KindMessageChunk)
		{
			Target->Text.Append(Chunk);
		}
		else
		{
			if (Target->Thought.IsEmpty()) Target->ThoughtStartUtc = At;
			Target->ThoughtEndUtc = At;
			Target->Thought.Append(Chunk);
		}
		return !bContinues;
	}

	if (Kind == ShuttlegentChatModelPrivate::KindToolCall || Kind == ShuttlegentChatModelPrivate::KindToolCallUpdate)
	{
		FString ToolCallId;
		Update->TryGetStringField(ShuttlegentChatModelPrivate::FieldToolCallId, ToolCallId);

		TSharedPtr<FShuttlegentChatEntry> Target = FindToolCall(ToolCallId);
		const bool bCreated = !Target.IsValid();
		if (bCreated)
		{
			Target = AddEntry(EShuttlegentChatEntryKind::ToolCall, FString());
			Target->ToolCallId = ToolCallId;
		}

		FString Title;
		FString Status;
		if (Update->TryGetStringField(ShuttlegentChatModelPrivate::FieldTitle, Title)) Target->Title = Title;
		if (Update->TryGetStringField(ShuttlegentChatModelPrivate::FieldStatus, Status)) Target->Status = Status;

		const TSharedPtr<FJsonObject>* RawInput = nullptr;
		if (Update->TryGetObjectField(ShuttlegentChatModelPrivate::FieldRawInput, RawInput))
		{
			FString Justification;
			if ((*RawInput)->TryGetStringField(ShuttlegentChatModelPrivate::FieldJustification, Justification)) Target->Detail = Justification;
			Target->Input = ShuttlegentChatModelPrivate::CompactJson(*RawInput);
		}

		const FString Result = ShuttlegentChatModelPrivate::ToolContentText(Update);
		if (!Result.IsEmpty()) Target->Result = Result;
		return bCreated;
	}

	if (Kind == ShuttlegentChatModelPrivate::KindUsageUpdate)
	{
		double Used = 0.0;
		double Size = 0.0;
		Update->TryGetNumberField(ShuttlegentChatModelPrivate::FieldUsed, Used);
		Update->TryGetNumberField(ShuttlegentChatModelPrivate::FieldSize, Size);
		UsedTokens = static_cast<int64>(Used);
		ContextSize = static_cast<int64>(Size);
		OnStatusChanged.Broadcast();
	}
	return false;
}

void FShuttlegentChatModel::ApplyTurnEnd(const FString& StopReason, const FString& ErrorMessage)
{
	if (!ErrorMessage.IsEmpty())
	{
		TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(EShuttlegentChatEntryKind::TurnEnd, ErrorMessage);
		Entry->bError = true;
		return;
	}
	if (StopReason == ShuttlegentChatModelPrivate::StopReasonCancelled)
	{
		AddEntry(EShuttlegentChatEntryKind::TurnEnd, LOCTEXT("Cancelled", "已取消").ToString());
		return;
	}
	if (!StopReason.IsEmpty() && StopReason != ShuttlegentChatModelPrivate::StopReasonEndTurn)
	{
		AddEntry(EShuttlegentChatEntryKind::TurnEnd, FText::Format(LOCTEXT("StoppedFor", "已停止：{0}"), FText::FromString(StopReason)).ToString());
	}
}

void FShuttlegentChatModel::ReplayTranscript(const FString& SessionId)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	ResetEntries();
	for (const TSharedPtr<FJsonObject>& Item : Pinned->GetSessionStore().LoadTranscript(SessionId))
	{
		FString Kind;
		const TSharedPtr<FJsonObject>* Data = nullptr;
		if (!Item->TryGetStringField(ShuttlegentChatModelPrivate::FieldKind, Kind) || !Item->TryGetObjectField(ShuttlegentChatModelPrivate::FieldData, Data)) continue;

		if (Kind == ShuttlegentChatModelPrivate::TranscriptPrompt)
		{
			FString Text;
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldText, Text);
			AddEntry(EShuttlegentChatEntryKind::User, Text);
		}
		else if (Kind == ShuttlegentChatModelPrivate::TranscriptUpdate)
		{
			FString Time;
			FDateTime At;
			if (!Item->TryGetStringField(ShuttlegentChatModelPrivate::FieldTime, Time) || !FDateTime::ParseIso8601(*Time, At)) At = FDateTime();
			ApplyUpdate(*Data, At);
		}
		else if (Kind == ShuttlegentChatModelPrivate::TranscriptTurnEnd)
		{
			FString StopReason;
			FString Error;
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldStopReason, StopReason);
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldError, Error);
			ApplyTurnEnd(StopReason, Error);
		}
		else if (Kind == ShuttlegentChatModelPrivate::TranscriptPermission)
		{
			TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(EShuttlegentChatEntryKind::Permission, FString());
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldToolCallId, Entry->ToolCallId);
			(*Data)->TryGetBoolField(ShuttlegentChatModelPrivate::FieldAllowed, Entry->bAllowed);
			Entry->bAnswered = true;

			const TSharedPtr<FShuttlegentChatEntry> ToolCall = FindToolCall(Entry->ToolCallId);
			if (ToolCall.IsValid()) Entry->Title = ToolCall->Title;
		}
	}

	LastSessionId = SessionId;
	OnStructureChanged.Broadcast();
}

void FShuttlegentChatModel::HandleStateChanged(EShuttlegentAgentState NewState)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (Pinned.IsValid() && NewState == EShuttlegentAgentState::Ready && Pinned->GetActiveSessionId().IsEmpty())
	{
		FString Error;
		if (!PendingResumeSessionId.IsEmpty())
		{
			const FString SessionId = MoveTemp(PendingResumeSessionId);
			PendingResumeSessionId.Reset();
			if (!Pinned->ResumeSession(SessionId, Error)) AddNotice(FText::Format(LOCTEXT("ResumeFailedOnStart", "恢复会话失败：{0}"), FText::FromString(Error)), true);
		}
		else if (!PendingPrompt.IsEmpty())
		{
			if (!Pinned->NewSession(Error)) AddNotice(FText::Format(LOCTEXT("NewSessionFailedOnStart", "新建会话失败：{0}"), FText::FromString(Error)), true);
		}
	}

	OnStatusChanged.Broadcast();
}

void FShuttlegentChatModel::HandleSessionReady(const FString& SessionId)
{
	LastSessionId = SessionId;
	bRuntimeLost = false;

	if (!PendingPrompt.IsEmpty())
	{
		const FString Text = MoveTemp(PendingPrompt);
		PendingPrompt.Reset();
		SendPrompt(Text);
	}
	OnStatusChanged.Broadcast();
}

void FShuttlegentChatModel::HandleSessionUpdate(const FString& SessionId, const TSharedPtr<FJsonObject>& Update)
{
	if (!Update.IsValid()) return;

	if (ApplyUpdate(Update, FDateTime::UtcNow()))
	{
		OnStructureChanged.Broadcast();
		return;
	}
	OnContentChanged.Broadcast();
}

void FShuttlegentChatModel::HandleTurnFinished(const FString& SessionId, const FString& StopReason, const FString& ErrorMessage)
{
	const bool bCompleted = ErrorMessage.IsEmpty() && (StopReason.IsEmpty() || StopReason == ShuttlegentChatModelPrivate::StopReasonEndTurn);

	ApplyTurnEnd(StopReason, ErrorMessage);
	OnTurnFinished.Broadcast(bCompleted);
	OnStructureChanged.Broadcast();
	OnStatusChanged.Broadcast();
}

void FShuttlegentChatModel::HandleRuntimeExited(int32 ExitCode, bool bExpected)
{
	if (!bExpected)
	{
		bRuntimeLost = true;
		AddNotice(FText::Format(LOCTEXT("RuntimeLost", "运行时意外退出（代码 {0}）。"), FText::AsNumber(ExitCode)), true);
	}
	OnStatusChanged.Broadcast();
}

void FShuttlegentChatModel::HandleError(const FString& Message)
{
	AddNotice(FText::FromString(Message), true);
}

void FShuttlegentChatModel::HandleConfigOptionsChanged()
{
	OnStatusChanged.Broadcast();
}

void FShuttlegentChatModel::HandlePermissionRequest(const TSharedPtr<FJsonObject>& Params, TFunction<void(bool)> Respond)
{
	TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(EShuttlegentChatEntryKind::Permission, FString());

	const TSharedPtr<FJsonObject>* ToolCall = nullptr;
	if (Params.IsValid() && Params->TryGetObjectField(ShuttlegentChatModelPrivate::FieldToolCall, ToolCall)) (*ToolCall)->TryGetStringField(ShuttlegentChatModelPrivate::FieldToolCallId, Entry->ToolCallId);

	const TSharedPtr<FShuttlegentChatEntry> Source = FindToolCall(Entry->ToolCallId);
	if (Source.IsValid())
	{
		Entry->Title = Source->Title;
		Entry->Detail = Source->Detail;
	}

	PendingPermissions.Add(Entry->Id, MoveTemp(Respond));
	OnStructureChanged.Broadcast();
}

#undef LOCTEXT_NAMESPACE
