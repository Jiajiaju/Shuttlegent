#include "Chat/ShuttlegentChatModel.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Chat/FShuttlegentToolPresentation.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "Modules/ModuleManager.h"
#include "Endpoint/FShuttlegentContextBreakdown.h"
#include "ShuttlegentMcpServerModule.h"
#include "QuestionGate/FShuttlegentQuestionGate.h"
#include "WriteGate/FShuttlegentWriteGate.h"
#include "Reference/FShuttlegentReferenceFinder.h"
#include "Reference/FShuttlegentReferencePrompt.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentAgentService.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentSessionStore.h"
#include "ShuttlegentSettings.h"

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

	// session/request_permission params: the session the request belongs to
	inline const TCHAR* FieldSessionId = TEXT("sessionId");

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

	// Tool call status of a failed call
	inline const TCHAR* ToolStatusFailed = TEXT("failed");

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

	/** Choices and current value of one config option among a session's config options; false when the option is absent. */
	bool ParseConfigChoices(const TArray<TSharedPtr<FJsonValue>>& ConfigOptions, const FString& ConfigId, TArray<FShuttlegentConfigChoice>& OutChoices, FString& OutCurrentValue)
	{
		for (const TSharedPtr<FJsonValue>& Value : ConfigOptions)
		{
			const TSharedPtr<FJsonObject> Option = Value.IsValid() ? Value->AsObject() : nullptr;
			FString Id;
			if (!Option.IsValid() || !Option->TryGetStringField(FieldId, Id) || Id != ConfigId) continue;

			Option->TryGetStringField(FieldCurrentValue, OutCurrentValue);

			const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
			if (!Option->TryGetArrayField(FieldOptions, Items)) return true;

			for (const TSharedPtr<FJsonValue>& ItemValue : *Items)
			{
				const TSharedPtr<FJsonObject> Item = ItemValue.IsValid() ? ItemValue->AsObject() : nullptr;
				if (!Item.IsValid()) continue;

				const TArray<TSharedPtr<FJsonValue>>* GroupItems = nullptr;
				if (!Item->TryGetArrayField(FieldOptions, GroupItems))
				{
					AddChoice(Item, FString(), OutChoices);
					continue;
				}

				FString GroupName;
				Item->TryGetStringField(FieldName, GroupName);
				for (const TSharedPtr<FJsonValue>& GroupValue : *GroupItems)
				{
					const TSharedPtr<FJsonObject> GroupItem = GroupValue.IsValid() ? GroupValue->AsObject() : nullptr;
					if (GroupItem.IsValid()) AddChoice(GroupItem, GroupName, OutChoices);
				}
			}
			return true;
		}
		return false;
	}
}

void FShuttlegentChatModel::Initialize(const TSharedRef<FShuttlegentAgentService>& InService)
{
	Service = InService;
	Current = MakeShared<FShuttlegentConversation>();
	Conversations.Add(Current);

	InService->OnStateChanged.AddSP(this, &FShuttlegentChatModel::HandleStateChanged);
	InService->OnSessionReady.AddSP(this, &FShuttlegentChatModel::HandleSessionReady);
	InService->OnSessionUpdate.AddSP(this, &FShuttlegentChatModel::HandleSessionUpdate);
	InService->OnTurnFinished.AddSP(this, &FShuttlegentChatModel::HandleTurnFinished);
	InService->OnRuntimeExited.AddSP(this, &FShuttlegentChatModel::HandleRuntimeExited);
	InService->OnError.AddSP(this, &FShuttlegentChatModel::HandleError);
	InService->OnConfigOptionsChanged.AddSP(this, &FShuttlegentChatModel::HandleConfigOptionsChanged);
	FShuttlegentMcpServerModule::Get().OnContextBreakdown().AddSP(this, &FShuttlegentChatModel::HandleContextBreakdown);

	const TSharedPtr<FShuttlegentQuestionGate> Questions = FShuttlegentMcpServerModule::Get().GetQuestionGate();
	if (Questions.IsValid())
	{
		QuestionGate = Questions;
		Questions->OnChanged.AddSP(this, &FShuttlegentChatModel::HandleQuestionsChanged);
		Questions->OnClosed.AddSP(this, &FShuttlegentChatModel::HandleQuestionClosed);
	}

	const TSharedPtr<FShuttlegentWriteGate> Gate = FShuttlegentMcpServerModule::Get().GetWriteGate();
	if (!Gate.IsValid()) return;

	WriteGate = Gate;
	Gate->OnRequested.AddSP(this, &FShuttlegentChatModel::HandleWriteRequested);
	Gate->OnClosed.AddSP(this, &FShuttlegentChatModel::HandleWriteClosed);
	Gate->OnFinished.AddSP(this, &FShuttlegentChatModel::HandleWriteFinished);
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

	FShuttlegentMcpServerModule* McpServer = FModuleManager::GetModulePtr<FShuttlegentMcpServerModule>(ShuttlegentEditorName::McpServerModule);
	if (McpServer != nullptr) McpServer->OnContextBreakdown().RemoveAll(this);

	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (Gate.IsValid())
	{
		Gate->OnRequested.RemoveAll(this);
		Gate->OnClosed.RemoveAll(this);
		Gate->OnFinished.RemoveAll(this);
	}
	WriteGate.Reset();

	const TSharedPtr<FShuttlegentQuestionGate> Questions = QuestionGate.Pin();
	if (Questions.IsValid())
	{
		Questions->OnChanged.RemoveAll(this);
		Questions->OnClosed.RemoveAll(this);
	}
	QuestionGate.Reset();

	UTransBuffer* TransBuffer = GEditor != nullptr ? Cast<UTransBuffer>(GEditor->Trans) : nullptr;
	if (IsValid(TransBuffer) && UndoBufferHandle.IsValid()) TransBuffer->OnUndoBufferChanged().Remove(UndoBufferHandle);
	if (PostUndoRedoHandle.IsValid()) FEditorDelegates::PostUndoRedo.Remove(PostUndoRedoHandle);
	UndoBufferHandle.Reset();
	PostUndoRedoHandle.Reset();
}

EShuttlegentAgentState FShuttlegentChatModel::GetState() const
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	return Pinned.IsValid() ? Pinned->GetState() : EShuttlegentAgentState::Stopped;
}

bool FShuttlegentChatModel::IsPrompting() const
{
	return IsPrompting(*Current);
}

void FShuttlegentChatModel::EnsureRuntime()
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || Pinned->GetState() != EShuttlegentAgentState::Stopped) return;

	FString Error;
	if (!Pinned->Start(Error))
	{
		RecordStartIssue(Pinned->GetStartIssue(), Error);
		ParkPendingPrompts();
	}
	OnStatusChanged.Broadcast();
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::Prewarm()
{
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	if (!IsValid(Settings) || !Settings->bPrewarmRuntime || StartIssue != EShuttlegentStartIssue::None) return;

	UE_LOG(LogShuttlegent, Log, TEXT("Prewarming the runtime."));
	EnsureRuntime();
}

void FShuttlegentChatModel::RetryStart()
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	StartIssue = EShuttlegentStartIssue::None;
	StartIssueDetail.Reset();
	AddRunLog(*Current, LOCTEXT("LogRetryStart", "重试启动运行时"), false);

	// A running runtime read its key when it started; it starts again once it has exited
	if (Pinned->GetState() != EShuttlegentAgentState::Stopped)
	{
		bRestartPending = true;
		Pinned->Stop();
		OnStatusChanged.Broadcast();
		return;
	}

	bRuntimeLost = false;
	if (!Current->SessionId.IsEmpty()) RequestSession(*Current);
	else EnsureRuntime();
	OnStatusChanged.Broadcast();
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::RetryResume()
{
	if (!Current->bResumeFailed || Current->SessionId.IsEmpty()) return;

	AddRunLog(*Current, LOCTEXT("LogRetryResume", "重试恢复会话"), false);
	RequestSession(*Current);
	NotifyStatus(*Current);
}

void FShuttlegentChatModel::AcknowledgeUnknownWrite(int32 EntryId)
{
	const TSharedPtr<FShuttlegentChatEntry>* Found = Current->Entries.FindByPredicate([EntryId](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry.IsValid() && Entry->Id == EntryId; });
	if (Found == nullptr || (*Found)->Kind != EShuttlegentChatEntryKind::WriteUnknown || (*Found)->bAnswered) return;

	(*Found)->bAnswered = true;
	Current->bNeedsVerify = Current->Entries.ContainsByPredicate([](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry->Kind == EShuttlegentChatEntryKind::WriteUnknown && !Entry->bAnswered; });
	AddRunLog(*Current, LOCTEXT("LogWriteVerified", "已核对写入结果未知的对象"), false);
	NotifyContent(*Current);
	NotifyStatus(*Current);
}

void FShuttlegentChatModel::RecordStartIssue(EShuttlegentStartIssue Issue, const FString& Detail)
{
	StartIssue = Issue;

	// A rejected key's message quotes part of the key, so only the kind is kept
	StartIssueDetail = Issue == EShuttlegentStartIssue::AuthFailed ? FString() : Detail;
	if (Issue == EShuttlegentStartIssue::AuthFailed)
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("The model provider rejected the key."));
	}
	else
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("The runtime cannot start: %s"), *Detail);
	}
}

void FShuttlegentChatModel::ParkPendingPrompts()
{
	for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
	{
		ParkPendingPrompt(*Conversation);
	}
}

void FShuttlegentChatModel::ParkPendingPrompt(FShuttlegentConversation& Conversation)
{
	Conversation.bWantsSession = false;
	Conversation.bResuming = false;
	if (Conversation.PendingPrompt.IsEmpty()) return;

	// The text already carries its reference block, so the item has no separate references
	Conversation.Queue.Items.Insert(MakeQueued(Conversation.PendingPrompt, TArray<FShuttlegentReference>()), 0);
	Conversation.Queue.bPaused = true;
	Conversation.PendingPrompt.Reset();
	AddRunLog(Conversation, LOCTEXT("LogPromptParked", "消息未发出，已放回队列首位"), true);
	NotifyQueue(Conversation);
}

EShuttlegentSubmitResult FShuttlegentChatModel::SubmitDraft(const FString& Text, const TArray<FShuttlegentReference>& References, TArray<FString>& OutInvalidPaths)
{
	OutInvalidPaths.Reset();
	for (const FShuttlegentReference& Reference : References)
	{
		if (!FShuttlegentReferenceFinder::Exists(Reference)) OutInvalidPaths.Add(Reference.Path);
	}
	const bool bBlocked = bRuntimeLost || StartIssue != EShuttlegentStartIssue::None || Current->bNeedsVerify;
	if (Text.TrimStartAndEnd().IsEmpty() || bBlocked || !OutInvalidPaths.IsEmpty()) return EShuttlegentSubmitResult::Rejected;

	if (!IsBusy(*Current))
	{
		SubmitPrompt(*Current, FShuttlegentReferencePrompt::Compose(Text, References));
		return EShuttlegentSubmitResult::Sent;
	}

	Current->Queue.Items.Add(MakeQueued(Text, References));
	AddRunLog(*Current, LOCTEXT("LogQueued", "加入队列"), false);
	NotifyQueue(*Current);
	return EShuttlegentSubmitResult::Queued;
}

bool FShuttlegentChatModel::IsBusy() const
{
	return IsBusy(*Current);
}

FShuttlegentPromptQueue FShuttlegentChatModel::GetCurrentQueue() const
{
	return Current->Queue;
}

void FShuttlegentChatModel::RemoveQueued(int32 ItemId)
{
	if (Current->Queue.Items.RemoveAll([ItemId](const FShuttlegentQueuedPrompt& Item) { return Item.Id == ItemId; }) > 0) NotifyQueue(*Current);
}

bool FShuttlegentChatModel::TakeQueued(int32 ItemId, const FString& DraftText, const TArray<FShuttlegentReference>& DraftReferences, FShuttlegentQueuedPrompt& OutItem)
{
	TArray<FShuttlegentQueuedPrompt>& Items = Current->Queue.Items;
	const int32 Index = Items.IndexOfByPredicate([ItemId](const FShuttlegentQueuedPrompt& Item) { return Item.Id == ItemId; });
	if (Index == INDEX_NONE) return false;

	OutItem = Items[Index];

	// The draft being replaced swaps into the item's place instead of being lost
	if (!DraftText.TrimStartAndEnd().IsEmpty() || !DraftReferences.IsEmpty()) Items[Index] = MakeQueued(DraftText, DraftReferences);
	else Items.RemoveAt(Index);

	NotifyQueue(*Current);
	return true;
}

void FShuttlegentChatModel::ResumeQueue()
{
	if (!Current->Queue.bPaused) return;

	Current->Queue.bPaused = false;
	AddRunLog(*Current, LOCTEXT("LogQueueResumed", "继续队列"), false);
	NotifyQueue(*Current);
	SendNextQueued(*Current);
}

bool FShuttlegentChatModel::CanRetry() const
{
	return Current->LastOutcome == EShuttlegentTurnOutcome::Failed && !Current->LastSentPrompt.IsEmpty() && !bRuntimeLost && StartIssue == EShuttlegentStartIssue::None && !Current->bNeedsVerify && !IsBusy(*Current);
}

void FShuttlegentChatModel::Retry()
{
	if (!CanRetry()) return;

	AddRunLog(*Current, LOCTEXT("LogRetry", "重试查询"), false);
	SubmitPrompt(*Current, Current->LastSentPrompt);
}

FShuttlegentQueuedPrompt FShuttlegentChatModel::MakeQueued(const FString& Text, const TArray<FShuttlegentReference>& References)
{
	FShuttlegentQueuedPrompt Item;
	Item.Id = NextQueueId++;
	Item.Text = Text;
	Item.References = References;
	TArray<FShuttlegentConfigChoice> Choices;
	GetConfigChoices(ShuttlegentEditorConfigId::Model, Choices, Item.ModelValue);
	GetConfigChoices(ShuttlegentEditorConfigId::Reasoning, Choices, Item.ReasoningValue);
	return Item;
}

void FShuttlegentChatModel::PauseQueue(FShuttlegentConversation& Conversation)
{
	FShuttlegentPromptQueue& Queue = Conversation.Queue;
	if (Queue.bPaused || Queue.Items.IsEmpty()) return;

	Queue.bPaused = true;
	AddRunLog(Conversation, LOCTEXT("LogQueuePaused", "队列已暂停"), true);
	NotifyQueue(Conversation);
}

void FShuttlegentChatModel::SendNextQueued(FShuttlegentConversation& Conversation)
{
	if (bRuntimeLost || IsBusy(Conversation)) return;

	FShuttlegentPromptQueue& Queue = Conversation.Queue;
	if (Queue.bPaused || Queue.Items.IsEmpty()) return;

	// A reference that went away while the item waited stops the queue at that item
	const FShuttlegentQueuedPrompt& Next = Queue.Items[0];
	const bool bAllExist = Next.References.FindByPredicate([](const FShuttlegentReference& Reference) { return !FShuttlegentReferenceFinder::Exists(Reference); }) == nullptr;
	if (!bAllExist)
	{
		Queue.bPaused = true;
		AddNotice(Conversation, LOCTEXT("QueuedReferenceGone", "队列中下一条消息引用的对象已失效，队列已暂停。取回该条，移除或重新添加引用后再继续。"), true);
		NotifyQueue(Conversation);
		return;
	}

	const FShuttlegentQueuedPrompt Item = Queue.Items[0];
	Queue.Items.RemoveAt(0);

	// Send options frozen with the item win over the current pickers
	TArray<FShuttlegentConfigChoice> Choices;
	FString CurrentModel;
	FString CurrentReasoning;
	GetConfigChoices(Conversation, ShuttlegentEditorConfigId::Model, Choices, CurrentModel);
	GetConfigChoices(Conversation, ShuttlegentEditorConfigId::Reasoning, Choices, CurrentReasoning);
	if (!Item.ModelValue.IsEmpty() && Item.ModelValue != CurrentModel) SetConfigValue(Conversation, ShuttlegentEditorConfigId::Model, Item.ModelValue);
	if (!Item.ReasoningValue.IsEmpty() && Item.ReasoningValue != CurrentReasoning) SetConfigValue(Conversation, ShuttlegentEditorConfigId::Reasoning, Item.ReasoningValue);

	AddRunLog(Conversation, LOCTEXT("LogQueueNext", "发送队列中的下一条"), false);
	NotifyQueue(Conversation);
	SubmitPrompt(Conversation, FShuttlegentReferencePrompt::Compose(Item.Text, Item.References));
}

const FShuttlegentWriteRequest* FShuttlegentChatModel::GetPendingWrite() const
{
	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (!Gate.IsValid() || Gate->GetPending().IsEmpty() || !OwnsWrite(*Current)) return nullptr;

	return &Gate->GetPending()[0];
}

void FShuttlegentChatModel::AnswerWrite(int32 RequestId, bool bApprove)
{
	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (Gate.IsValid()) Gate->Answer(RequestId, bApprove);
}

const FShuttlegentQuestion* FShuttlegentChatModel::GetPendingQuestion() const
{
	const TSharedPtr<FShuttlegentQuestionGate> Questions = QuestionGate.Pin();
	if (!Questions.IsValid() || Questions->GetPending().IsEmpty()) return nullptr;

	return &Questions->GetPending()[0];
}

void FShuttlegentChatModel::AnswerQuestion(int32 QuestionId, const FString& Answer, bool bSkip)
{
	const TSharedPtr<FShuttlegentQuestionGate> Questions = QuestionGate.Pin();
	if (Questions.IsValid()) Questions->Answer(QuestionId, Answer, bSkip);
}

bool FShuttlegentChatModel::CanUndoWrite(const FShuttlegentChatEntry& Entry, FText& OutReason) const
{
	const FShuttlegentWriteResult& Result = Entry.WriteResult;
	if (!Result.bUndoable)
	{
		OutReason = LOCTEXT("UndoNotSupported", "此操作不是同步事务，不能撤销。");
		return false;
	}
	if (Entry.bUndone)
	{
		OutReason = LOCTEXT("UndoDone", "本次修改已撤销。");
		return false;
	}
	if (!Result.TransactionId.IsValid())
	{
		OutReason = LOCTEXT("UndoNothing", "没有执行任何修改，无需撤销。");
		return false;
	}
	if (!FShuttlegentWriteGate::IsOnUndoTop(Result.TransactionId))
	{
		OutReason = LOCTEXT("UndoNotTop", "之后还有其他编辑，本次事务已不在撤销栈顶，不能直接撤销；请在编辑器撤销历史中核对。");
		return false;
	}
	OutReason = LOCTEXT("UndoAvailable", "本次事务在撤销栈顶，可撤销。");
	return true;
}

void FShuttlegentChatModel::UndoWrite(int32 EntryId)
{
	const TSharedPtr<FShuttlegentChatEntry>* Found = Current->Entries.FindByPredicate([EntryId](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry.IsValid() && Entry->Id == EntryId; });
	if (Found == nullptr || (*Found)->Kind != EShuttlegentChatEntryKind::WriteResult) return;

	const TSharedPtr<FShuttlegentChatEntry> Entry = *Found;
	FText Reason;
	if (!CanUndoWrite(*Entry, Reason) || !FShuttlegentWriteGate::Undo(Entry->WriteResult.TransactionId))
	{
		AddNotice(*Current, FText::Format(LOCTEXT("UndoFailed", "撤销失败：{0}"), Reason), true);
		return;
	}

	Entry->bUndone = true;
	AddRunLog(*Current, LOCTEXT("LogUndoWrite", "撤销本次修改"), false);
	NotifyContent(*Current);
}

void FShuttlegentChatModel::Cancel()
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || !IsPrompting(*Current) || Current->bStopping) return;

	Current->bStopping = true;
	Pinned->Cancel(Current->SessionId);
	AddRunLog(*Current, LOCTEXT("LogCancel", "发送停止请求"), true);
	NotifyStatus(*Current);
}

void FShuttlegentChatModel::NewConversation()
{
	LeaveCurrent();
	Current = MakeShared<FShuttlegentConversation>();
	Conversations.Add(Current);
	BroadcastAll();
}

void FShuttlegentChatModel::OpenSession(const FString& SessionId)
{
	if (!Service.IsValid() || SessionId.IsEmpty() || SessionId == Current->SessionId) return;

	LeaveCurrent();

	// A held conversation is shown as it is: still running, or idle with its entries and run log
	const TSharedPtr<FShuttlegentConversation> Held = FindConversation(SessionId);
	if (Held.IsValid())
	{
		Current = Held;
		BroadcastAll();
		return;
	}

	Current = MakeShared<FShuttlegentConversation>();
	Current->SessionId = SessionId;
	Conversations.Add(Current);
	ReplayTranscript(*Current);
	AddRunLog(*Current, LOCTEXT("LogOpenSession", "打开会话记录，恢复运行时上下文"), false);
	RequestSession(*Current);
	BroadcastAll();
}

void FShuttlegentChatModel::Reconnect()
{
	bRuntimeLost = false;
	AddRunLog(*Current, LOCTEXT("LogReconnect", "重新连接运行时"), false);

	// Other held conversations resume their sessions when they next send a prompt
	if (!Current->SessionId.IsEmpty()) RequestSession(*Current);
	else EnsureRuntime();
	OnStatusChanged.Broadcast();
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::SetPermissionHandlerAttached(bool bAttached)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || bAttached == bPermissionHandlerAttached) return;

	bPermissionHandlerAttached = bAttached;

	// Editor writes are confirmed only while the panel is there to ask; detached, the gate refuses them
	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (Gate.IsValid()) Gate->SetConfirmerAttached(bAttached);

	// Questions too: without the panel the agent asks in its reply instead
	const TSharedPtr<FShuttlegentQuestionGate> Questions = QuestionGate.Pin();
	if (Questions.IsValid()) Questions->SetAnswererAttached(bAttached);

	if (bAttached)
	{
		Pinned->PermissionHandler.BindSP(this, &FShuttlegentChatModel::HandlePermissionRequest);
		return;
	}

	Pinned->PermissionHandler.Unbind();

	// Requests the panel can no longer answer fall back to a rejection.
	for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
	{
		RejectPendingPermissions(*Conversation);
	}
}

void FShuttlegentChatModel::AnswerPermission(int32 EntryId, bool bAllow)
{
	const TSharedPtr<FShuttlegentConversation>* Owner = Conversations.FindByPredicate([EntryId](const TSharedPtr<FShuttlegentConversation>& Conversation) { return Conversation->PendingPermissions.Contains(EntryId); });
	if (Owner == nullptr) return;

	FShuttlegentConversation& Conversation = **Owner;
	TFunction<void(bool)> Respond;
	Conversation.PendingPermissions.RemoveAndCopyValue(EntryId, Respond);

	for (const TSharedPtr<FShuttlegentChatEntry>& Entry : Conversation.Entries)
	{
		if (Entry->Id != EntryId) continue;

		Entry->bAnswered = true;
		Entry->bAllowed = bAllow;
		break;
	}

	if (Respond) Respond(bAllow);
	AddRunLog(Conversation, bAllow ? LOCTEXT("LogPermissionAllowed", "允许本次运行时提权") : LOCTEXT("LogPermissionRejected", "拒绝运行时提权"), !bAllow);
	if (!bAllow) PauseQueue(Conversation);
	NotifyContent(Conversation);
	NotifyStatus(Conversation);
}

bool FShuttlegentChatModel::GetConfigChoices(const FString& ConfigId, TArray<FShuttlegentConfigChoice>& OutChoices, FString& OutCurrentValue) const
{
	return GetConfigChoices(*Current, ConfigId, OutChoices, OutCurrentValue);
}

void FShuttlegentChatModel::SetConfigValue(const FString& ConfigId, const FString& Value)
{
	SetConfigValue(*Current, ConfigId, Value);
}

bool FShuttlegentChatModel::GetConfigChoices(const FShuttlegentConversation& Conversation, const FString& ConfigId, TArray<FShuttlegentConfigChoice>& OutChoices, FString& OutCurrentValue) const
{
	OutChoices.Reset();
	OutCurrentValue.Reset();

	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return false;

	// A conversation without an open session shows the last options any session reported, with the values chosen for it
	const bool bOpen = Pinned->IsSessionOpen(Conversation.SessionId);
	const bool bFound = ShuttlegentChatModelPrivate::ParseConfigChoices(bOpen ? Pinned->GetConfigOptions(Conversation.SessionId) : LastConfigOptions, ConfigId, OutChoices, OutCurrentValue);

	const FString* Pending = Conversation.PendingConfig.Find(ConfigId);
	if (bFound && Pending != nullptr) OutCurrentValue = *Pending;
	return bFound;
}

void FShuttlegentChatModel::SetConfigValue(FShuttlegentConversation& Conversation, const FString& ConfigId, const FString& Value)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	if (!Pinned->IsSessionOpen(Conversation.SessionId))
	{
		Conversation.PendingConfig.Add(ConfigId, Value);
		NotifyStatus(Conversation);
		return;
	}

	FString Error;
	if (!Pinned->SetConfigOption(Conversation.SessionId, ConfigId, Value, Error)) AddNotice(Conversation, FText::Format(LOCTEXT("ConfigFailed", "切换失败：{0}"), FText::FromString(Error)), true);
}

EShuttlegentChatStatus FShuttlegentChatModel::GetStatus() const
{
	return GetStatus(*Current);
}

bool FShuttlegentChatModel::GetSessionActivity(const FString& SessionId, EShuttlegentChatStatus& OutStatus, int32& OutQueued) const
{
	const TSharedPtr<FShuttlegentConversation> Conversation = FindConversation(SessionId);
	if (!Conversation.IsValid()) return false;

	OutStatus = GetStatus(*Conversation);
	OutQueued = Conversation->Queue.Items.Num();
	return true;
}

TSharedPtr<FShuttlegentChatEntry> FShuttlegentChatModel::GetPendingPermission() const
{
	return GetPendingPermission(*Current);
}

TSharedPtr<FShuttlegentChatEntry> FShuttlegentChatModel::FindToolCallOf(const TSharedPtr<FShuttlegentChatEntry>& Permission) const
{
	if (!Permission.IsValid() || Permission->ToolCallId.IsEmpty()) return nullptr;

	return FindToolCall(*Current, Permission->ToolCallId);
}

TArray<FShuttlegentSessionRecord> FShuttlegentChatModel::GetSessions(bool bArchived) const
{
	TArray<FShuttlegentSessionRecord> Sessions;
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return Sessions;

	for (const FShuttlegentSessionRecord& Record : Pinned->GetSessionStore().GetRecords())
	{
		if (Record.bArchived == bArchived) Sessions.Add(Record);
	}
	return Sessions;
}

FString FShuttlegentChatModel::GetCurrentTitle() const
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || Current->SessionId.IsEmpty()) return FString();

	const FShuttlegentSessionRecord* Record = Pinned->GetSessionStore().FindRecord(Current->SessionId);
	return Record != nullptr ? Record->Title : FString();
}

bool FShuttlegentChatModel::CanArchive(const FString& SessionId) const
{
	if (SessionId.IsEmpty()) return false;

	const TSharedPtr<FShuttlegentConversation> Conversation = FindConversation(SessionId);
	if (!Conversation.IsValid()) return true;

	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	const bool bWriteOpen = OwnsWrite(*Conversation) && (bWriting || (Gate.IsValid() && !Gate->GetPending().IsEmpty()));
	return !IsPrompting(*Conversation) && !GetPendingPermission(*Conversation).IsValid() && !bWriteOpen;
}

bool FShuttlegentChatModel::RenameSession(const FString& SessionId, const FString& Title)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || !Pinned->GetSessionStore().RenameRecord(SessionId, Title)) return false;

	OnSessionsChanged.Broadcast();
	return true;
}

bool FShuttlegentChatModel::ArchiveSession(const FString& SessionId)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || !CanArchive(SessionId) || !Pinned->GetSessionStore().SetArchived(SessionId, true)) return false;

	if (SessionId == Current->SessionId)
	{
		NewConversation();
		return true;
	}

	// An archived session is not kept running in the background; its queue goes with it
	const TSharedPtr<FShuttlegentConversation> Held = FindConversation(SessionId);
	if (Held.IsValid()) DropConversation(*Held);
	OnSessionsChanged.Broadcast();
	return true;
}

void FShuttlegentChatModel::RestoreSession(const FString& SessionId)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (Pinned.IsValid() && Pinned->GetSessionStore().SetArchived(SessionId, false)) OnSessionsChanged.Broadcast();
}

bool FShuttlegentChatModel::DeleteSession(const FString& SessionId)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || !CanArchive(SessionId) || !Pinned->GetSessionStore().DeleteRecord(SessionId)) return false;

	if (SessionId == Current->SessionId)
	{
		NewConversation();
		return true;
	}

	const TSharedPtr<FShuttlegentConversation> Held = FindConversation(SessionId);
	if (Held.IsValid()) DropConversation(*Held);
	OnSessionsChanged.Broadcast();
	return true;
}

void FShuttlegentChatModel::HandleContextBreakdown(const FString& SessionId, const FShuttlegentContextBreakdown& Breakdown)
{
	const TSharedPtr<FShuttlegentConversation> Conversation = FindConversation(SessionId);
	if (!Conversation.IsValid()) return;

	Conversation->ContextBreakdown = Breakdown;
	if (IsCurrent(*Conversation)) OnStatusChanged.Broadcast();
}

TSharedPtr<FShuttlegentConversation> FShuttlegentChatModel::FindConversation(const FString& SessionId) const
{
	if (SessionId.IsEmpty()) return nullptr;

	const TSharedPtr<FShuttlegentConversation>* Found = Conversations.FindByPredicate([&SessionId](const TSharedPtr<FShuttlegentConversation>& Conversation) { return Conversation->SessionId == SessionId; });
	return Found != nullptr ? *Found : nullptr;
}

bool FShuttlegentChatModel::IsPrompting(const FShuttlegentConversation& Conversation) const
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	return Pinned.IsValid() && Pinned->IsPrompting(Conversation.SessionId);
}

bool FShuttlegentChatModel::IsBusy(const FShuttlegentConversation& Conversation) const
{
	return IsPrompting(Conversation) || GetState() == EShuttlegentAgentState::Starting || Conversation.bResuming || Conversation.bStopping || !Conversation.PendingPrompt.IsEmpty();
}

EShuttlegentChatStatus FShuttlegentChatModel::GetStatus(const FShuttlegentConversation& Conversation) const
{
	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (OwnsWrite(Conversation) && Gate.IsValid() && !Gate->GetPending().IsEmpty()) return EShuttlegentChatStatus::AwaitingWriteConfirmation;
	if (OwnsWrite(Conversation) && bWriting) return EShuttlegentChatStatus::Writing;
	if (StartIssue != EShuttlegentStartIssue::None) return EShuttlegentChatStatus::StartBlocked;
	if (bRuntimeLost) return EShuttlegentChatStatus::Lost;
	if (Conversation.bResuming) return EShuttlegentChatStatus::Resuming;
	if (Conversation.bResumeFailed) return EShuttlegentChatStatus::ResumeFailed;

	if (IsPrompting(Conversation))
	{
		if (GetPendingPermission(Conversation).IsValid()) return EShuttlegentChatStatus::AwaitingPermission;
		return Conversation.bStopping ? EShuttlegentChatStatus::Stopping : EShuttlegentChatStatus::Querying;
	}

	switch (GetState())
	{
	case EShuttlegentAgentState::Starting:
		{
			return EShuttlegentChatStatus::Connecting;
		}
	case EShuttlegentAgentState::Ready:
		{
			// The first prompt waits for session/new
			if (!Conversation.PendingPrompt.IsEmpty()) return EShuttlegentChatStatus::Connecting;
			if (Conversation.LastOutcome == EShuttlegentTurnOutcome::Stopped) return EShuttlegentChatStatus::Stopped;
			if (Conversation.LastOutcome == EShuttlegentTurnOutcome::Failed) return EShuttlegentChatStatus::Failed;
			return EShuttlegentChatStatus::Connected;
		}
	case EShuttlegentAgentState::Stopped:
	default:
		{
			return EShuttlegentChatStatus::NotConnected;
		}
	}
}

TSharedPtr<FShuttlegentChatEntry> FShuttlegentChatModel::GetPendingPermission(const FShuttlegentConversation& Conversation) const
{
	for (const TSharedPtr<FShuttlegentChatEntry>& Entry : Conversation.Entries)
	{
		if (Entry->Kind == EShuttlegentChatEntryKind::Permission && Conversation.PendingPermissions.Contains(Entry->Id)) return Entry;
	}
	return nullptr;
}

bool FShuttlegentChatModel::OwnsWrite(const FShuttlegentConversation& Conversation) const
{
	const TSharedPtr<FShuttlegentConversation> Owner = WriteOwner.Pin();
	return Owner.IsValid() ? Owner.Get() == &Conversation : IsCurrent(Conversation);
}

void FShuttlegentChatModel::LeaveCurrent()
{
	if (CanDrop(*Current)) DropConversation(*Current);
}

bool FShuttlegentChatModel::CanDrop(const FShuttlegentConversation& Conversation) const
{
	const TSharedPtr<FShuttlegentConversation> Owner = WriteOwner.Pin();
	return !IsBusy(Conversation) && Conversation.Queue.Items.IsEmpty() && Conversation.PendingPermissions.IsEmpty() && Owner.Get() != &Conversation;
}

void FShuttlegentChatModel::DropConversation(FShuttlegentConversation& Conversation)
{
	// Held until the end of this call, whoever else lets go of it
	const TSharedRef<FShuttlegentConversation> Dropped = Conversation.AsShared();

	RejectPendingPermissions(Conversation);
	AwaitingNewSession.RemoveAll([&Conversation](const TWeakPtr<FShuttlegentConversation>& Awaiting) { return Awaiting.Pin().Get() == &Conversation; });
	Conversations.RemoveAll([&Conversation](const TSharedPtr<FShuttlegentConversation>& Held) { return Held.Get() == &Conversation; });

	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (Pinned.IsValid() && Pinned->IsSessionOpen(Conversation.SessionId)) Pinned->CloseSession(Conversation.SessionId);
}

void FShuttlegentChatModel::BroadcastAll()
{
	OnStructureChanged.Broadcast();
	OnStatusChanged.Broadcast();
	OnSessionsChanged.Broadcast();
	OnRunLogChanged.Broadcast();
	OnQueueChanged.Broadcast();
}

void FShuttlegentChatModel::NotifyStructure(const FShuttlegentConversation& Conversation)
{
	if (IsCurrent(Conversation)) OnStructureChanged.Broadcast();
}

void FShuttlegentChatModel::NotifyContent(const FShuttlegentConversation& Conversation)
{
	if (IsCurrent(Conversation)) OnContentChanged.Broadcast();
}

void FShuttlegentChatModel::NotifyStatus(const FShuttlegentConversation& Conversation)
{
	if (IsCurrent(Conversation)) OnStatusChanged.Broadcast();

	// The session list shows every held conversation's status
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::NotifyQueue(const FShuttlegentConversation& Conversation)
{
	if (IsCurrent(Conversation)) OnQueueChanged.Broadcast();

	// The session list shows every held conversation's queued count
	OnSessionsChanged.Broadcast();
}

TSharedPtr<FShuttlegentChatEntry> FShuttlegentChatModel::AddEntry(FShuttlegentConversation& Conversation, EShuttlegentChatEntryKind Kind, const FString& Text)
{
	TSharedPtr<FShuttlegentChatEntry> Entry = MakeShared<FShuttlegentChatEntry>();
	Entry->Kind = Kind;
	Entry->Text = Text;
	Entry->Id = NextEntryId++;
	Conversation.Entries.Add(Entry);
	return Entry;
}

TSharedPtr<FShuttlegentChatEntry> FShuttlegentChatModel::FindToolCall(const FShuttlegentConversation& Conversation, const FString& ToolCallId) const
{
	for (int32 Index = Conversation.Entries.Num() - 1; Index >= 0; --Index)
	{
		const TSharedPtr<FShuttlegentChatEntry>& Entry = Conversation.Entries[Index];
		if (Entry->Kind == EShuttlegentChatEntryKind::ToolCall && Entry->ToolCallId == ToolCallId) return Entry;
	}
	return nullptr;
}

void FShuttlegentChatModel::AddNotice(FShuttlegentConversation& Conversation, const FText& Text, bool bError)
{
	TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(Conversation, EShuttlegentChatEntryKind::Notice, Text.ToString());
	Entry->bError = bError;
	NotifyStructure(Conversation);
}

void FShuttlegentChatModel::RejectPendingPermissions(FShuttlegentConversation& Conversation)
{
	TMap<int32, TFunction<void(bool)>> Pending = MoveTemp(Conversation.PendingPermissions);
	Conversation.PendingPermissions.Reset();
	for (TPair<int32, TFunction<void(bool)>>& Request : Pending)
	{
		for (const TSharedPtr<FShuttlegentChatEntry>& Entry : Conversation.Entries)
		{
			if (Entry->Id != Request.Key) continue;

			Entry->bAnswered = true;
			Entry->bAllowed = false;
			break;
		}

		if (Request.Value) Request.Value(false);
	}
}

void FShuttlegentChatModel::AddRunLog(FShuttlegentConversation& Conversation, const FText& Text, bool bWarning)
{
	FShuttlegentRunLogEntry& Entry = Conversation.RunLog.AddDefaulted_GetRef();
	Entry.Time = FDateTime::UtcNow();
	Entry.Text = Text;
	Entry.bWarning = bWarning;
	if (IsCurrent(Conversation)) OnRunLogChanged.Broadcast();
}

void FShuttlegentChatModel::SubmitPrompt(FShuttlegentConversation& Conversation, const FString& Text)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || Text.TrimStartAndEnd().IsEmpty()) return;

	if (Pinned->GetState() == EShuttlegentAgentState::Ready && Pinned->IsSessionOpen(Conversation.SessionId))
	{
		SendPrompt(Conversation, Text);
		return;
	}

	Conversation.PendingPrompt = Text;
	RequestSession(Conversation);
	NotifyStatus(Conversation);
}

void FShuttlegentChatModel::SendPrompt(FShuttlegentConversation& Conversation, const FString& Text)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	FString Error;
	if (!Pinned->Prompt(Conversation.SessionId, Text, Error))
	{
		AddNotice(Conversation, FText::Format(LOCTEXT("PromptFailed", "发送失败：{0}"), FText::FromString(Error)), true);
		return;
	}

	Conversation.LastOutcome = EShuttlegentTurnOutcome::None;
	Conversation.bStopping = false;
	Conversation.LastSentPrompt = Text;
	AddEntry(Conversation, EShuttlegentChatEntryKind::User, Text);
	AddRunLog(Conversation, LOCTEXT("LogTurnStart", "开始本轮"), false);
	NotifyStructure(Conversation);
	NotifyStatus(Conversation);
}

void FShuttlegentChatModel::RequestSession(FShuttlegentConversation& Conversation)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	const bool bResume = !Conversation.SessionId.IsEmpty();
	if (bResume)
	{
		Conversation.bResuming = true;
		Conversation.bResumeFailed = false;
	}

	if (Pinned->GetState() != EShuttlegentAgentState::Ready)
	{
		Conversation.bWantsSession = true;
		EnsureRuntime();
		return;
	}

	Conversation.bWantsSession = false;
	FString Error;
	if (!bResume)
	{
		if (Pinned->NewSession(Error))
		{
			AwaitingNewSession.Add(Conversation.AsShared());
			return;
		}
		ParkPendingPrompt(Conversation);
		AddNotice(Conversation, FText::Format(LOCTEXT("NewSessionFailed", "新建会话失败：{0}"), FText::FromString(Error)), true);
		return;
	}

	if (Pinned->ResumeSession(Conversation.SessionId, Error)) return;

	ParkPendingPrompt(Conversation);
	Conversation.bResumeFailed = true;
	AddNotice(Conversation, FText::Format(LOCTEXT("ResumeFailed", "恢复会话失败：{0}"), FText::FromString(Error)), true);
}

bool FShuttlegentChatModel::ApplyUpdate(FShuttlegentConversation& Conversation, const TSharedPtr<FJsonObject>& Update, const FDateTime& At)
{
	FString Kind;
	Update->TryGetStringField(ShuttlegentChatModelPrivate::FieldSessionUpdate, Kind);

	if (Kind == ShuttlegentChatModelPrivate::KindMessageChunk || Kind == ShuttlegentChatModelPrivate::KindThoughtChunk)
	{
		FString MessageId;
		Update->TryGetStringField(ShuttlegentChatModelPrivate::FieldMessageId, MessageId);

		const TSharedPtr<FShuttlegentChatEntry> Last = Conversation.Entries.Num() > 0 ? Conversation.Entries.Last() : nullptr;
		const bool bContinues = Last.IsValid() && Last->Kind == EShuttlegentChatEntryKind::Assistant && Last->MessageId == MessageId;
		const TSharedPtr<FShuttlegentChatEntry> Target = bContinues ? Last : AddEntry(Conversation, EShuttlegentChatEntryKind::Assistant, FString());
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

		TSharedPtr<FShuttlegentChatEntry> Target = FindToolCall(Conversation, ToolCallId);
		const bool bCreated = !Target.IsValid();
		if (bCreated)
		{
			Target = AddEntry(Conversation, EShuttlegentChatEntryKind::ToolCall, FString());
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
		Conversation.UsedTokens = static_cast<int64>(Used);
		Conversation.ContextSize = static_cast<int64>(Size);
		if (IsCurrent(Conversation)) OnStatusChanged.Broadcast();
	}
	return false;
}

void FShuttlegentChatModel::ApplyTurnEnd(FShuttlegentConversation& Conversation, const FString& StopReason, const FString& ErrorMessage)
{
	if (!ErrorMessage.IsEmpty())
	{
		TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(Conversation, EShuttlegentChatEntryKind::TurnEnd, ErrorMessage);
		Entry->bError = true;
		return;
	}
	if (StopReason == ShuttlegentChatModelPrivate::StopReasonCancelled)
	{
		AddEntry(Conversation, EShuttlegentChatEntryKind::TurnEnd, LOCTEXT("Cancelled", "已取消").ToString());
		return;
	}
	if (!StopReason.IsEmpty() && StopReason != ShuttlegentChatModelPrivate::StopReasonEndTurn)
	{
		AddEntry(Conversation, EShuttlegentChatEntryKind::TurnEnd, FText::Format(LOCTEXT("StoppedFor", "已停止：{0}"), FText::FromString(StopReason)).ToString());
	}
}

void FShuttlegentChatModel::ReplayTranscript(FShuttlegentConversation& Conversation)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	for (const TSharedPtr<FJsonObject>& Item : Pinned->GetSessionStore().LoadTranscript(Conversation.SessionId))
	{
		FString Kind;
		const TSharedPtr<FJsonObject>* Data = nullptr;
		if (!Item->TryGetStringField(ShuttlegentChatModelPrivate::FieldKind, Kind) || !Item->TryGetObjectField(ShuttlegentChatModelPrivate::FieldData, Data)) continue;

		if (Kind == ShuttlegentChatModelPrivate::TranscriptPrompt)
		{
			FString Text;
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldText, Text);
			AddEntry(Conversation, EShuttlegentChatEntryKind::User, Text);
		}
		else if (Kind == ShuttlegentChatModelPrivate::TranscriptUpdate)
		{
			FString Time;
			FDateTime At;
			if (!Item->TryGetStringField(ShuttlegentChatModelPrivate::FieldTime, Time) || !FDateTime::ParseIso8601(*Time, At)) At = FDateTime();
			ApplyUpdate(Conversation, *Data, At);
		}
		else if (Kind == ShuttlegentChatModelPrivate::TranscriptTurnEnd)
		{
			FString StopReason;
			FString Error;
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldStopReason, StopReason);
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldError, Error);
			ApplyTurnEnd(Conversation, StopReason, Error);
		}
		else if (Kind == ShuttlegentChatModelPrivate::TranscriptPermission)
		{
			TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(Conversation, EShuttlegentChatEntryKind::Permission, FString());
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldToolCallId, Entry->ToolCallId);
			(*Data)->TryGetBoolField(ShuttlegentChatModelPrivate::FieldAllowed, Entry->bAllowed);
			Entry->bAnswered = true;

			const TSharedPtr<FShuttlegentChatEntry> ToolCall = FindToolCall(Conversation, Entry->ToolCallId);
			if (ToolCall.IsValid()) Entry->Title = ToolCall->Title;
		}
	}
	NotifyStructure(Conversation);
}

void FShuttlegentChatModel::HandleStateChanged(EShuttlegentAgentState NewState)
{
	if (NewState == EShuttlegentAgentState::Ready)
	{
		// A runtime that came up has its executable and key; a rejected key shows again on the next failed turn
		StartIssue = EShuttlegentStartIssue::None;
		StartIssueDetail.Reset();

		// Every conversation that asked for a session while the runtime was coming up gets it now
		const TArray<TSharedPtr<FShuttlegentConversation>> Held = Conversations;
		for (const TSharedPtr<FShuttlegentConversation>& Conversation : Held)
		{
			if (Conversation->bWantsSession) RequestSession(*Conversation);
		}
	}

	OnStatusChanged.Broadcast();
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::HandleSessionReady(const FString& SessionId)
{
	TSharedPtr<FShuttlegentConversation> Conversation = FindConversation(SessionId);

	// A session/new answer belongs to the oldest conversation waiting for one; a session nobody waits for is not the panel's
	while (!Conversation.IsValid() && !AwaitingNewSession.IsEmpty())
	{
		Conversation = AwaitingNewSession[0].Pin();
		AwaitingNewSession.RemoveAt(0);
	}
	if (!Conversation.IsValid()) return;

	if (Conversation->bResuming) AddRunLog(*Conversation, LOCTEXT("LogResumed", "会话已恢复"), false);
	Conversation->SessionId = SessionId;
	Conversation->bResuming = false;
	Conversation->bResumeFailed = false;
	bRuntimeLost = false;

	const TMap<FString, FString> PendingConfig = MoveTemp(Conversation->PendingConfig);
	Conversation->PendingConfig.Reset();
	for (const TPair<FString, FString>& Pair : PendingConfig)
	{
		SetConfigValue(*Conversation, Pair.Key, Pair.Value);
	}

	if (!Conversation->PendingPrompt.IsEmpty())
	{
		const FString Text = MoveTemp(Conversation->PendingPrompt);
		Conversation->PendingPrompt.Reset();
		SendPrompt(*Conversation, Text);
	}
	OnStatusChanged.Broadcast();
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::HandleSessionUpdate(const FString& SessionId, const TSharedPtr<FJsonObject>& Update)
{
	const TSharedPtr<FShuttlegentConversation> Owner = FindConversation(SessionId);
	if (!Update.IsValid() || !Owner.IsValid()) return;

	FShuttlegentConversation& Conversation = *Owner;
	FString ToolCallId;
	FString Status;
	Update->TryGetStringField(ShuttlegentChatModelPrivate::FieldToolCallId, ToolCallId);
	Update->TryGetStringField(ShuttlegentChatModelPrivate::FieldStatus, Status);
	const TSharedPtr<FShuttlegentChatEntry> Existing = ToolCallId.IsEmpty() ? nullptr : FindToolCall(Conversation, ToolCallId);
	const FString PreviousStatus = Existing.IsValid() ? Existing->Status : FString();

	const bool bAdded = ApplyUpdate(Conversation, Update, FDateTime::UtcNow());

	// Run log: a tool call's first update and its first failure
	const TSharedPtr<FShuttlegentChatEntry> ToolCall = ToolCallId.IsEmpty() ? nullptr : FindToolCall(Conversation, ToolCallId);
	if (ToolCall.IsValid() && !Existing.IsValid()) AddRunLog(Conversation, FText::Format(LOCTEXT("LogToolCall", "工具调用：{0}"), FShuttlegentToolPresentation::GetDisplayName(ToolCall->Title)), false);
	if (ToolCall.IsValid() && Status == ShuttlegentChatModelPrivate::ToolStatusFailed && PreviousStatus != Status) AddRunLog(Conversation, FText::Format(LOCTEXT("LogToolFailed", "工具失败：{0}"), FShuttlegentToolPresentation::GetDisplayName(ToolCall->Title)), true);

	if (bAdded)
	{
		NotifyStructure(Conversation);
		return;
	}
	NotifyContent(Conversation);
}

void FShuttlegentChatModel::HandleTurnFinished(const FString& SessionId, const FString& StopReason, const FString& ErrorMessage)
{
	const TSharedPtr<FShuttlegentConversation> Owner = FindConversation(SessionId);
	if (!Owner.IsValid()) return;

	FShuttlegentConversation& Conversation = *Owner;
	const bool bCompleted = ErrorMessage.IsEmpty() && (StopReason.IsEmpty() || StopReason == ShuttlegentChatModelPrivate::StopReasonEndTurn);

	Conversation.bStopping = false;
	if (bCompleted) Conversation.LastOutcome = EShuttlegentTurnOutcome::Completed;
	else Conversation.LastOutcome = ErrorMessage.IsEmpty() ? EShuttlegentTurnOutcome::Stopped : EShuttlegentTurnOutcome::Failed;

	// A missing or rejected key fails every turn, so it is shown as the start guide card rather than as one failed turn
	const EShuttlegentStartIssue KeyIssue = ErrorMessage.IsEmpty() ? EShuttlegentStartIssue::None : FShuttlegentAgentService::ClassifyError(ErrorMessage);
	if (KeyIssue == EShuttlegentStartIssue::MissingKey || KeyIssue == EShuttlegentStartIssue::AuthFailed) RecordStartIssue(KeyIssue, ErrorMessage);

	if (Conversation.LastOutcome == EShuttlegentTurnOutcome::Completed) AddRunLog(Conversation, LOCTEXT("LogTurnCompleted", "本轮结束"), false);
	else if (Conversation.LastOutcome == EShuttlegentTurnOutcome::Stopped) AddRunLog(Conversation, LOCTEXT("LogTurnStopped", "本轮已停止"), true);
	else AddRunLog(Conversation, LOCTEXT("LogTurnFailed", "本轮查询失败"), true);

	// A request still open when its turn ends can no longer be acted on. Write requests do not name their session,
	// so they expire only when no session runs a turn any more.
	RejectPendingPermissions(Conversation);
	const bool bAnyPrompting = Conversations.ContainsByPredicate([this](const TSharedPtr<FShuttlegentConversation>& Held) { return IsPrompting(*Held); });
	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (Gate.IsValid() && !bAnyPrompting) Gate->ExpireAll();
	const TSharedPtr<FShuttlegentQuestionGate> Questions = QuestionGate.Pin();
	if (Questions.IsValid() && !bAnyPrompting) Questions->ExpireAll();

	ApplyTurnEnd(Conversation, StopReason, ErrorMessage);
	if (IsCurrent(Conversation)) OnTurnFinished.Broadcast(bCompleted);
	NotifyStructure(Conversation);
	NotifyStatus(Conversation);

	if (bCompleted) SendNextQueued(Conversation);
	else PauseQueue(Conversation);
}

void FShuttlegentChatModel::HandleRuntimeExited(int32 ExitCode, bool bExpected)
{
	AwaitingNewSession.Reset();
	if (!bExpected) bRuntimeLost = true;
	ParkPendingPrompts();

	// A confirmed write whose result has not arrived may or may not have happened; the user verifies its objects
	if (bWriting)
	{
		bWriting = false;
		const TSharedPtr<FShuttlegentConversation> Owner = WriteOwner.IsValid() ? WriteOwner.Pin() : Current;
		TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(*Owner, EShuttlegentChatEntryKind::WriteUnknown, ApprovedWrite.Action);
		Entry->WriteResult.RequestId = ApprovedWrite.RequestId;
		Entry->WriteResult.Action = ApprovedWrite.Action;
		Entry->WriteResult.Objects = ApprovedWrite.Objects;
		Owner->bNeedsVerify = true;
		AddRunLog(*Owner, LOCTEXT("LogWriteUnknown", "编辑器写入结果未知"), true);
		NotifyStructure(*Owner);

		const TSharedPtr<FShuttlegentWriteGate> PendingGate = WriteGate.Pin();
		if (!PendingGate.IsValid() || PendingGate->GetPending().IsEmpty()) WriteOwner.Reset();
	}

	// One runtime serves every session, so every conversation stops with it
	for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
	{
		Conversation->bStopping = false;
		RejectPendingPermissions(*Conversation);
		if (bExpected) continue;

		if (!Conversation->Queue.Items.IsEmpty()) Conversation->Queue.bPaused = true;
		AddRunLog(*Conversation, LOCTEXT("LogRuntimeLost", "运行时断开"), true);
		AddNotice(*Conversation, FText::Format(LOCTEXT("RuntimeLost", "运行时意外退出（代码 {0}）。"), FText::AsNumber(ExitCode)), true);
	}

	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (Gate.IsValid()) Gate->ExpireAll();
	const TSharedPtr<FShuttlegentQuestionGate> Questions = QuestionGate.Pin();
	if (Questions.IsValid()) Questions->ExpireAll();

	OnStatusChanged.Broadcast();
	OnQueueChanged.Broadcast();
	OnSessionsChanged.Broadcast();

	// A retry stopped the runtime so it reads a fixed key or executable; it starts again now
	if (bRestartPending)
	{
		bRestartPending = false;
		if (!Current->SessionId.IsEmpty()) RequestSession(*Current);
		else EnsureRuntime();
	}
}

void FShuttlegentChatModel::HandleError(const FString& SessionId, const FString& Message)
{
	// An error without a session is a failed session/new (the oldest waiting conversation's) or a failed start (the current one's)
	TSharedPtr<FShuttlegentConversation> Conversation = FindConversation(SessionId);
	while (!Conversation.IsValid() && SessionId.IsEmpty() && !AwaitingNewSession.IsEmpty())
	{
		Conversation = AwaitingNewSession[0].Pin();
		AwaitingNewSession.RemoveAt(0);
	}
	// A failed initialize belongs to no conversation; the start guide card shows it
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Conversation.IsValid() && SessionId.IsEmpty() && Pinned.IsValid() && Pinned->GetStartIssue() != EShuttlegentStartIssue::None)
	{
		RecordStartIssue(Pinned->GetStartIssue(), Message);
		OnStatusChanged.Broadcast();
		return;
	}
	if (!Conversation.IsValid()) Conversation = Current;

	// The local record stays; the banner offers to resume again or to start a new session without its context
	if (Conversation->bResuming) Conversation->bResumeFailed = true;
	ParkPendingPrompt(*Conversation);
	AddNotice(*Conversation, FText::FromString(Message), true);
	NotifyStatus(*Conversation);
}

void FShuttlegentChatModel::HandleConfigOptionsChanged(const FString& SessionId)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (Pinned.IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>> Options = Pinned->GetConfigOptions(SessionId);
		if (!Options.IsEmpty()) LastConfigOptions = Options;
	}

	OnStatusChanged.Broadcast();
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::HandlePermissionRequest(const TSharedPtr<FJsonObject>& Params, TFunction<void(bool)> Respond)
{
	FString SessionId;
	if (Params.IsValid()) Params->TryGetStringField(ShuttlegentChatModelPrivate::FieldSessionId, SessionId);

	// A request of a session the panel does not hold has nobody to answer it
	const TSharedPtr<FShuttlegentConversation> Owner = FindConversation(SessionId);
	if (!Owner.IsValid())
	{
		if (Respond) Respond(false);
		return;
	}

	FShuttlegentConversation& Conversation = *Owner;
	TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(Conversation, EShuttlegentChatEntryKind::Permission, FString());

	const TSharedPtr<FJsonObject>* ToolCall = nullptr;
	if (Params->TryGetObjectField(ShuttlegentChatModelPrivate::FieldToolCall, ToolCall)) (*ToolCall)->TryGetStringField(ShuttlegentChatModelPrivate::FieldToolCallId, Entry->ToolCallId);

	const TSharedPtr<FShuttlegentChatEntry> Source = FindToolCall(Conversation, Entry->ToolCallId);
	if (Source.IsValid())
	{
		Entry->Title = Source->Title;
		Entry->Detail = Source->Detail;
	}

	// With escalation switched off the request is answered at once and leaves a readable record (interaction spec section 3)
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	if (IsValid(Settings) && !Settings->bAllowElevation)
	{
		Entry->bAnswered = true;
		Entry->bAllowed = false;
		if (Respond) Respond(false);

		AddNotice(Conversation, FText::Format(LOCTEXT("ElevationAutoRejected", "设置中已关闭运行时提权，已自动拒绝：{0}"), FShuttlegentToolPresentation::GetDisplayName(Entry->Title)), false);
		AddRunLog(Conversation, LOCTEXT("LogPermissionAutoRejected", "按设置自动拒绝运行时提权"), true);
		PauseQueue(Conversation);
		NotifyStructure(Conversation);
		NotifyStatus(Conversation);
		return;
	}

	Conversation.PendingPermissions.Add(Entry->Id, MoveTemp(Respond));
	AddRunLog(Conversation, LOCTEXT("LogPermissionRequest", "运行时请求额外权限"), true);
	NotifyStructure(Conversation);
	NotifyStatus(Conversation);
}

void FShuttlegentChatModel::HandleWriteRequested(const FShuttlegentWriteRequest& Request)
{
	// Write requests do not name their session; they go to the conversation shown when the first of them arrives
	if (!WriteOwner.IsValid()) WriteOwner = Current;
	OpenWriteRequests.Add(Request.RequestId, Request);

	const TSharedPtr<FShuttlegentConversation> Owner = WriteOwner.Pin();
	AddRunLog(*Owner, FText::Format(LOCTEXT("LogWriteRequested", "编辑器写入请求：{0}"), FText::FromString(Request.Action)), true);
	NotifyStatus(*Owner);
}

void FShuttlegentChatModel::HandleWriteClosed(int32 RequestId, EShuttlegentWriteDecision Decision)
{
	const TSharedPtr<FShuttlegentConversation> Owner = WriteOwner.IsValid() ? WriteOwner.Pin() : Current;
	FShuttlegentWriteRequest Closed;
	OpenWriteRequests.RemoveAndCopyValue(RequestId, Closed);
	if (Decision == EShuttlegentWriteDecision::Approved)
	{
		ApprovedWrite = MoveTemp(Closed);
		bWriting = true;
		AddRunLog(*Owner, LOCTEXT("LogWriteApproved", "确认执行编辑器写入"), false);
	}
	else
	{
		// Nothing was written; like a stop, the queue waits for the user
		const bool bExpired = Decision == EShuttlegentWriteDecision::Expired;
		const bool bTimedOut = Decision == EShuttlegentWriteDecision::TimedOut;
		FText Notice = LOCTEXT("WriteRejected", "已取消本次编辑器写入，未执行修改。");
		if (bExpired) Notice = LOCTEXT("WriteExpired", "编辑器写入请求已失效（关卡切换、连接断开或本轮结束），未执行修改。");
		if (bTimedOut) Notice = LOCTEXT("WriteTimedOut", "编辑器写入请求超过 55 秒未确认，已失效，未执行修改。");
		AddNotice(*Owner, Notice, false);
		AddRunLog(*Owner, bExpired || bTimedOut ? LOCTEXT("LogWriteExpired", "编辑器写入请求失效") : LOCTEXT("LogWriteRejected", "取消编辑器写入"), true);
		PauseQueue(*Owner);

		const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
		if (!bWriting && (!Gate.IsValid() || Gate->GetPending().IsEmpty())) WriteOwner.Reset();
	}
	NotifyStatus(*Owner);
}

void FShuttlegentChatModel::HandleWriteFinished(const FShuttlegentWriteResult& Result)
{
	// A result that arrives after the runtime was lost replaces its unknown entry
	for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
	{
		const TSharedPtr<FShuttlegentChatEntry>* Unknown = Conversation->Entries.FindByPredicate([&Result](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry->Kind == EShuttlegentChatEntryKind::WriteUnknown && Entry->WriteResult.RequestId == Result.RequestId; });
		if (Unknown == nullptr) continue;

		EnsureUndoListener();
		(*Unknown)->Kind = EShuttlegentChatEntryKind::WriteResult;
		(*Unknown)->WriteResult = Result;
		(*Unknown)->bAnswered = false;
		Conversation->bNeedsVerify = Conversation->Entries.ContainsByPredicate([](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry->Kind == EShuttlegentChatEntryKind::WriteUnknown && !Entry->bAnswered; });
		AddRunLog(*Conversation, LOCTEXT("LogWriteLateResult", "编辑器写入结果已回报"), false);
		NotifyStructure(*Conversation);
		NotifyStatus(*Conversation);
		return;
	}

	const TSharedPtr<FShuttlegentConversation> Owner = WriteOwner.IsValid() ? WriteOwner.Pin() : Current;
	bWriting = false;
	EnsureUndoListener();

	TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(*Owner, EShuttlegentChatEntryKind::WriteResult, Result.Action);
	Entry->WriteResult = Result;

	// A batch that did not complete every object stops the queue (interaction spec section 4.3)
	const bool bAllCompleted = !Result.Objects.ContainsByPredicate([](const FShuttlegentWriteObject& Object) { return Object.Status != EShuttlegentWriteObjectStatus::Completed; });
	AddRunLog(*Owner, bAllCompleted ? LOCTEXT("LogWriteCompleted", "编辑器写入完成") : LOCTEXT("LogWritePartial", "编辑器写入未全部完成"), !bAllCompleted);
	if (!bAllCompleted) PauseQueue(*Owner);

	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (!Gate.IsValid() || Gate->GetPending().IsEmpty()) WriteOwner.Reset();

	NotifyStructure(*Owner);
	NotifyStatus(*Owner);
}

void FShuttlegentChatModel::HandleUndoBufferChanged()
{
	// Any new transaction or undo can change whether a result is still on top
	const bool bHasWriteResults = Current->Entries.ContainsByPredicate([](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry.IsValid() && Entry->Kind == EShuttlegentChatEntryKind::WriteResult; });
	if (bHasWriteResults) OnContentChanged.Broadcast();
}

void FShuttlegentChatModel::HandleQuestionsChanged()
{
	// The panel shows the first waiting question in place of the composer
	NotifyStatus(*Current);
}

void FShuttlegentChatModel::HandleQuestionClosed(const FShuttlegentQuestion& Question, EShuttlegentQuestionOutcome Outcome, const FString& Answer)
{
	if (Question.Question.IsEmpty()) return;

	// The transcript keeps what was asked and how it ended
	FText Notice;
	switch (Outcome)
	{
	case EShuttlegentQuestionOutcome::Answered:
		{
			Notice = FText::Format(LOCTEXT("QuestionAnswered", "Agent 提问：{0}　你的回答：{1}"), FText::FromString(Question.Question), FText::FromString(Answer));
			break;
		}
	case EShuttlegentQuestionOutcome::Skipped:
		{
			Notice = FText::Format(LOCTEXT("QuestionSkipped", "Agent 提问：{0}　已跳过"), FText::FromString(Question.Question));
			break;
		}
	case EShuttlegentQuestionOutcome::TimedOut:
		{
			Notice = FText::Format(LOCTEXT("QuestionTimedOut", "Agent 提问：{0}　超过 55 秒未回答，Agent 将改用文字提问"), FText::FromString(Question.Question));
			break;
		}
	default:
		{
			Notice = FText::Format(LOCTEXT("QuestionExpired", "Agent 提问：{0}　已失效"), FText::FromString(Question.Question));
			break;
		}
	}
	AddNotice(*Current, Notice, Outcome != EShuttlegentQuestionOutcome::Answered);
	NotifyStructure(*Current);
}

void FShuttlegentChatModel::EnsureUndoListener()
{
	UTransBuffer* TransBuffer = GEditor != nullptr ? Cast<UTransBuffer>(GEditor->Trans) : nullptr;
	if (IsValid(TransBuffer) && !UndoBufferHandle.IsValid()) UndoBufferHandle = TransBuffer->OnUndoBufferChanged().AddSP(this, &FShuttlegentChatModel::HandleUndoBufferChanged);
	if (!PostUndoRedoHandle.IsValid()) PostUndoRedoHandle = FEditorDelegates::PostUndoRedo.AddSP(this, &FShuttlegentChatModel::HandleUndoBufferChanged);
}

#undef LOCTEXT_NAMESPACE
