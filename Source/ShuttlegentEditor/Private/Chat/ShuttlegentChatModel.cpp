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
#include "Endpoint/FShuttlegentSessionEvent.h"
#include "ShuttlegentMcpServerModule.h"
#include "QuestionGate/FShuttlegentQuestionGate.h"
#include "WriteGate/FShuttlegentWriteGate.h"
#include "Reference/FShuttlegentReferenceFinder.h"
#include "Reference/FShuttlegentReferencePrompt.h"
#include "ShuttlegentEditorConstants.h"
#include "FShuttlegentMemoryStore.h"
#include "ShuttlegentAgentService.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentSessionStore.h"
#include "ShuttlegentSettings.h"

#define LOCTEXT_NAMESPACE "ShuttlegentChatModel"

namespace ShuttlegentChatModelPrivate
{
	// session/update 字段
	inline const TCHAR* FieldSessionUpdate = TEXT("sessionUpdate");
	inline const TCHAR* FieldMessageId = TEXT("messageId");
	inline const TCHAR* FieldContent = TEXT("content");
	inline const TCHAR* FieldText = TEXT("text");
	inline const TCHAR* FieldImages = TEXT("images");

	// 只附图片、没有文字的消息发给模型时的文字
	inline const TCHAR* ImageOnlyPrompt = TEXT("（见附图）");
	inline const TCHAR* FieldToolCallId = TEXT("toolCallId");
	inline const TCHAR* FieldTitle = TEXT("title");
	inline const TCHAR* FieldStatus = TEXT("status");
	inline const TCHAR* FieldRawInput = TEXT("rawInput");
	inline const TCHAR* FieldJustification = TEXT("justification");
	inline const TCHAR* FieldUsed = TEXT("used");
	inline const TCHAR* FieldSize = TEXT("size");
	inline const TCHAR* FieldToolCall = TEXT("toolCall");

	// session/request_permission 参数：请求所属的会话
	inline const TCHAR* FieldSessionId = TEXT("sessionId");

	// session/update 类型
	inline const TCHAR* KindMessageChunk = TEXT("agent_message_chunk");
	inline const TCHAR* KindThoughtChunk = TEXT("agent_thought_chunk");
	inline const TCHAR* KindToolCall = TEXT("tool_call");
	inline const TCHAR* KindToolCallUpdate = TEXT("tool_call_update");
	inline const TCHAR* KindUsageUpdate = TEXT("usage_update");

	// 配置选项字段
	inline const TCHAR* FieldId = TEXT("id");
	inline const TCHAR* FieldName = TEXT("name");
	inline const TCHAR* FieldValue = TEXT("value");
	inline const TCHAR* FieldOptions = TEXT("options");
	inline const TCHAR* FieldCurrentValue = TEXT("currentValue");

	// 对话记录条目的字段与类型（参见 FShuttlegentSessionStore）
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

	// 一次结束的上下文压缩，回放时重建压缩条目（字段 manual、shadowedTokenCount、error）
	inline const TCHAR* TranscriptCompaction = TEXT("compaction");

	// session tap 插件推来的事件类型及其字段（见 Runtime/plugins/shuttlegent-session-tap.mjs）
	inline const TCHAR* EventTodoWrite = TEXT("todo/write");
	inline const TCHAR* EventCompactionPrefix = TEXT("compaction/");
	inline const TCHAR* EventCompactionStart = TEXT("compaction/start");
	inline const TCHAR* EventCompactionSummary = TEXT("compaction/summary");
	inline const TCHAR* EventCompactionEnd = TEXT("compaction/end");
	inline const TCHAR* FieldTodos = TEXT("todos");
	inline const TCHAR* FieldManual = TEXT("manual");
	inline const TCHAR* FieldShadowedTokenCount = TEXT("shadowedTokenCount");
	inline const TCHAR* FieldUsedTokens = TEXT("usedTokens");

	// 压缩条目的状态，写在条目的 Status 中；Title 记录手动（ShuttlegentEditorWebField::Manual）或自动
	inline const TCHAR* CompactionRunning = TEXT("running");
	inline const TCHAR* CompactionDone = TEXT("done");
	inline const TCHAR* CompactionFailed = TEXT("failed");
	inline const TCHAR* CompactionAuto = TEXT("auto");

	// 手动压缩所执行的运行时命令
	inline const TCHAR* CompactCommand = TEXT("/compact");

	// 计划模式切换事件，及一次结束的计划审批的会话记录（字段 plan、outcome、feedback）
	inline const TCHAR* EventPlanMode = TEXT("plan/mode");
	inline const TCHAR* FieldActive = TEXT("active");
	inline const TCHAR* TranscriptPlan = TEXT("plan");
	inline const TCHAR* FieldPlan = TEXT("plan");
	inline const TCHAR* FieldOutcome = TEXT("outcome");
	inline const TCHAR* FieldFeedback = TEXT("feedback");

	// 交出计划的会话不在面板中时退回给 Agent 的说明
	inline const TCHAR* PlanReviewNoConversation = TEXT("Nobody can review this plan: the session is not open in the editor panel. Stop and tell the user.");

	// 同一工具结果中各文本片段之间的分隔符
	inline const TCHAR* ToolContentJoiner = TEXT("\n");

	// 停止原因
	inline const TCHAR* StopReasonCancelled = TEXT("cancelled");
	inline const TCHAR* StopReasonEndTurn = TEXT("end_turn");

	// 失败调用的工具调用状态
	inline const TCHAR* ToolStatusFailed = TEXT("failed");

	FString ChunkText(const TSharedPtr<FJsonObject>& Update)
	{
		const TSharedPtr<FJsonObject>* Content = nullptr;
		FString Text;
		if (Update->TryGetObjectField(FieldContent, Content)) (*Content)->TryGetStringField(FieldText, Text);
		return Text;
	}

	/** tool_call(_update) 内容数组的文本：[{ type: "content", content: { type: "text", text } }]。不存在时为空。 */
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

	/** 会话配置选项中某一选项的可选值与当前值；该选项不存在时返回 false。 */
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

namespace ShuttlegentChatModelPrivate
{
	/** 新对话的写入确认方式，取自编辑器偏好设置。 */
	EShuttlegentWriteMode DefaultWriteMode()
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		return IsValid(Settings) ? Settings->DefaultWriteMode : EShuttlegentWriteMode::Manual;
	}

	/** 这条消息之后仍可撤销的写入（同步事务、尚未撤销、确有执行），按发生顺序。 */
	TArray<TSharedPtr<FShuttlegentChatEntry>> UndoableWritesAfter(const FShuttlegentConversation& Conversation, int32 Index)
	{
		TArray<TSharedPtr<FShuttlegentChatEntry>> Writes;
		for (int32 EntryIndex = Index + 1; EntryIndex < Conversation.Entries.Num(); ++EntryIndex)
		{
			const TSharedPtr<FShuttlegentChatEntry>& Entry = Conversation.Entries[EntryIndex];
			if (Entry->Kind == EShuttlegentChatEntryKind::WriteResult && Entry->WriteResult.bUndoable && !Entry->bUndone && Entry->WriteResult.TransactionId.IsValid()) Writes.Add(Entry);
		}
		return Writes;
	}

	/** 这些写入的事务（从新到旧）恰好依次占据撤销栈顶。 */
	bool AreWritesOnUndoTop(const TArray<TSharedPtr<FShuttlegentChatEntry>>& Writes)
	{
		TArray<FGuid> NewestFirst;
		for (int32 Index = Writes.Num() - 1; Index >= 0; --Index)
		{
			NewestFirst.Add(Writes[Index]->WriteResult.TransactionId);
		}
		return FShuttlegentWriteGate::AreOnUndoTop(NewestFirst);
	}

	/** 从新到旧撤销这些写入；调用前已确认它们都在撤销栈顶。 */
	void UndoWrites(const TArray<TSharedPtr<FShuttlegentChatEntry>>& Writes)
	{
		for (int32 Index = Writes.Num() - 1; Index >= 0; --Index)
		{
			if (FShuttlegentWriteGate::Undo(Writes[Index]->WriteResult.TransactionId)) Writes[Index]->bUndone = true;
		}
	}
}

void FShuttlegentChatModel::Initialize(const TSharedRef<FShuttlegentAgentService>& InService)
{
	Service = InService;
	Current = MakeShared<FShuttlegentConversation>();
	Current->WriteMode = ShuttlegentChatModelPrivate::DefaultWriteMode();
	Conversations.Add(Current);

	InService->OnStateChanged.AddSP(this, &FShuttlegentChatModel::HandleStateChanged);
	InService->OnSessionReady.AddSP(this, &FShuttlegentChatModel::HandleSessionReady);
	InService->OnSessionUpdate.AddSP(this, &FShuttlegentChatModel::HandleSessionUpdate);
	InService->OnTurnFinished.AddSP(this, &FShuttlegentChatModel::HandleTurnFinished);
	InService->OnRuntimeExited.AddSP(this, &FShuttlegentChatModel::HandleRuntimeExited);
	InService->OnError.AddSP(this, &FShuttlegentChatModel::HandleError);
	InService->OnConfigOptionsChanged.AddSP(this, &FShuttlegentChatModel::HandleConfigOptionsChanged);
	FShuttlegentMcpServerModule::Get().OnContextBreakdown().AddSP(this, &FShuttlegentChatModel::HandleContextBreakdown);
	FShuttlegentMcpServerModule::Get().OnSessionEvent().AddSP(this, &FShuttlegentChatModel::HandleSessionEvent);
	FShuttlegentMcpServerModule::Get().OnPlanReview().AddSP(this, &FShuttlegentChatModel::HandlePlanReview);
	FShuttlegentMcpServerModule::Get().OnPlanReviewClosed().AddSP(this, &FShuttlegentChatModel::HandlePlanReviewClosed);

	const TSharedPtr<FShuttlegentQuestionGate> Questions = FShuttlegentMcpServerModule::Get().GetQuestionGate();
	if (Questions.IsValid())
	{
		QuestionGate = Questions;
		Questions->OnChanged.AddSP(this, &FShuttlegentChatModel::HandleQuestionsChanged);
		Questions->OnClosed.AddSP(this, &FShuttlegentChatModel::HandleQuestionClosed);
	}
	FShuttlegentMemoryStore::OnChanged().AddSP(this, &FShuttlegentChatModel::HandleMemoryChanged);

	const TSharedPtr<FShuttlegentWriteGate> Gate = FShuttlegentMcpServerModule::Get().GetWriteGate();
	if (!Gate.IsValid()) return;

	WriteGate = Gate;
	Gate->OnRequested.AddSP(this, &FShuttlegentChatModel::HandleWriteRequested);
	Gate->OnClosed.AddSP(this, &FShuttlegentChatModel::HandleWriteClosed);
	Gate->OnFinished.AddSP(this, &FShuttlegentChatModel::HandleWriteFinished);

	// 写入模式决定写入门是否弹确认卡；计划模式下 Harness 不限制工具，由写入门拒绝写入
	const TWeakPtr<FShuttlegentChatModel> WeakThis = AsShared();
	Gate->GetWritePolicy = [WeakThis](const FShuttlegentWriteRequest& Request)
	{
		const TSharedPtr<FShuttlegentChatModel> This = WeakThis.Pin();
		return This.IsValid() ? This->GetWritePolicy(Request) : EShuttlegentWritePolicy::Ask;
	};
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
	if (McpServer != nullptr)
	{
		McpServer->OnContextBreakdown().RemoveAll(this);
		McpServer->OnSessionEvent().RemoveAll(this);
		McpServer->OnPlanReview().RemoveAll(this);
		McpServer->OnPlanReviewClosed().RemoveAll(this);
	}

	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (Gate.IsValid())
	{
		Gate->GetWritePolicy = nullptr;
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
	FShuttlegentMemoryStore::OnChanged().RemoveAll(this);

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

	// 正在运行的运行时在启动时已读取密钥；待其退出后再次启动
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

	// 密钥被拒绝时的消息会引用部分密钥，因此只保留问题类型
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

	// 队列不保存图片：随挂起提示的图片丢弃并告知用户
	if (!Conversation.PendingImages.IsEmpty())
	{
		Conversation.PendingImages.Reset();
		AddNotice(Conversation, LOCTEXT("ImagesDropped", "消息未能发出，已放回队列；所附图片没有保留，请重新添加。"), true);
	}

	// 文本已带有其引用块，因此该条目没有单独的引用
	Conversation.Queue.Items.Insert(MakeQueued(Conversation.PendingPrompt, TArray<FShuttlegentReference>()), 0);
	Conversation.Queue.bPaused = true;
	Conversation.PendingPrompt.Reset();
	AddRunLog(Conversation, LOCTEXT("LogPromptParked", "消息未发出，已放回队列首位"), true);
	NotifyQueue(Conversation);
}

EShuttlegentSubmitResult FShuttlegentChatModel::SubmitDraft(const FString& Text, const TArray<FShuttlegentReference>& References, bool bEnterPlanMode, const TArray<FShuttlegentPromptImage>& Images, TArray<FString>& OutInvalidPaths)
{
	OutInvalidPaths.Reset();
	for (const FShuttlegentReference& Reference : References)
	{
		if (!FShuttlegentReferenceFinder::Exists(Reference)) OutInvalidPaths.Add(Reference.Path);
	}
	const bool bBlocked = bRuntimeLost || StartIssue != EShuttlegentStartIssue::None || Current->bNeedsVerify;
	if ((Text.TrimStartAndEnd().IsEmpty() && Images.IsEmpty()) || bBlocked || !OutInvalidPaths.IsEmpty()) return EShuttlegentSubmitResult::Rejected;

	// 带图片的消息只在空闲时发送：队列与挂起提示都不保存图片
	FText ImageReason;
	if (!Images.IsEmpty() && !CanAttachImages(ImageReason)) return EShuttlegentSubmitResult::Rejected;

	FText PlanReason;
	if (bEnterPlanMode && !CanSetPlanMode(PlanReason)) return EShuttlegentSubmitResult::Rejected;

	if (!IsBusy(*Current))
	{
		const FString Body = Text.TrimStartAndEnd().IsEmpty() ? FString(ShuttlegentChatModelPrivate::ImageOnlyPrompt) : Text;
		const FString Prompt = FShuttlegentReferencePrompt::Compose(Body, References);
		if (bEnterPlanMode && !Current->bPlanMode) SubmitInPlanMode(Current, Prompt, Images);
		else SubmitPrompt(*Current, Prompt, Images);
		return EShuttlegentSubmitResult::Sent;
	}

	// 回合进行中：计划模式在下一步生效，消息照常排队
	if (bEnterPlanMode) SetPlanMode(true);

	Current->Queue.Items.Add(MakeQueued(Text, References));
	AddRunLog(*Current, LOCTEXT("LogQueued", "加入队列"), false);
	NotifyQueue(*Current);
	return EShuttlegentSubmitResult::Queued;
}

bool FShuttlegentChatModel::CanAttachImages(FText& OutReason) const
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || Pinned->GetState() != EShuttlegentAgentState::Ready)
	{
		OutReason = LOCTEXT("ImagesNotReady", "运行时就绪后才能添加图片");
		return false;
	}
	if (!Pinned->SupportsImagePrompts())
	{
		OutReason = LOCTEXT("ImagesUnsupported", "当前模型不支持图片；默认模型 deepseek-flash 支持，可在偏好设置中改回");
		return false;
	}

	// 运行时只按默认模型声明图片能力；沿用旧模型的对话以运行时确认的结果为准，尚未确认时先放行
	TArray<FShuttlegentConfigChoice> Models;
	FString ModelValue;
	ShuttlegentChatModelPrivate::ParseConfigChoices(Pinned->GetConfigOptions(Current->SessionId), ShuttlegentEditorConfigId::Model, Models, ModelValue);
	const bool* bModelImage = ModelImageInput.Find(ModelValue);
	if (bModelImage != nullptr && !*bModelImage)
	{
		const FShuttlegentConfigChoice* Model = Models.FindByPredicate([&ModelValue](const FShuttlegentConfigChoice& Choice) { return Choice.Value == ModelValue; });
		const FString ModelName = Model != nullptr ? Model->Name : ModelValue;
		OutReason = FText::Format(LOCTEXT("ImagesUnsupportedBySession", "这个对话使用的模型 {0} 不支持图片；在下方切换到支持图片的模型，或新建对话"), FText::FromString(ModelName));
		return false;
	}

	if (IsBusy(*Current))
	{
		OutReason = LOCTEXT("ImagesBusy", "本轮结束后再发送带图片的消息");
		return false;
	}
	return true;
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

	// 被替换的草稿换入该条目的位置，而不是丢失
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
	SubmitPrompt(*Current, Current->LastSentPrompt, Current->LastSentImages);
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

	// 条目等待期间若有引用消失，队列在该条目处停止
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

	// 随条目固定的发送选项优先于当前选择器的取值
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

void FShuttlegentChatModel::UndoMemory(int32 EntryId)
{
	const TSharedPtr<FShuttlegentChatEntry>* Found = Current->Entries.FindByPredicate([EntryId](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry.IsValid() && Entry->Id == EntryId; });
	if (Found == nullptr || (*Found)->Kind != EShuttlegentChatEntryKind::Memory || (*Found)->bUndone || (*Found)->Status == ShuttlegentEditorWebField::MemoryRead) return;

	const TSharedPtr<FShuttlegentChatEntry> Entry = *Found;
	FString Error;
	if (!FShuttlegentMemoryStore::Revert(Entry->MemoryChange, Error))
	{
		AddNotice(*Current, FText::Format(LOCTEXT("UndoMemoryFailed", "撤销失败：{0}"), FText::FromString(Error)), true);
		return;
	}

	Entry->bUndone = true;
	AddRunLog(*Current, LOCTEXT("LogUndoMemory", "撤销记忆修改"), false);
	NotifyContent(*Current);
}

void FShuttlegentChatModel::HandleMemoryChanged(const FShuttlegentMemoryChange& Change, bool bByAgent)
{
	if (!bByAgent) return;

	// 记忆工具的调用不带会话：归属正在运行回合的对话，没有时归属当前对话
	const TSharedPtr<FShuttlegentConversation>* Running = Conversations.FindByPredicate([this](const TSharedPtr<FShuttlegentConversation>& Conversation) { return IsPrompting(*Conversation); });
	const TSharedPtr<FShuttlegentConversation> Owner = Running != nullptr ? *Running : Current;

	const bool bRemoved = Change.Kind == EShuttlegentMemoryChangeKind::Removed;
	const TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(*Owner, EShuttlegentChatEntryKind::Memory, bRemoved ? Change.Before.Text : Change.After.Text);
	Entry->MemoryChange = Change;
	if (Change.Kind == EShuttlegentMemoryChangeKind::Added) Entry->Status = ShuttlegentEditorWebField::MemoryAdded;
	else if (Change.Kind == EShuttlegentMemoryChangeKind::Updated) Entry->Status = ShuttlegentEditorWebField::MemoryUpdated;
	else Entry->Status = ShuttlegentEditorWebField::MemoryRemoved;
	NotifyStructure(*Owner);
}

bool FShuttlegentChatModel::CanEditHistory(FText& OutReason) const
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || Current->SessionId.IsEmpty())
	{
		OutReason = LOCTEXT("HistoryNoSession", "这段对话还没有会话。");
		return false;
	}
	if (IsBusy(*Current) || !Current->Queue.Items.IsEmpty())
	{
		OutReason = LOCTEXT("HistoryBusy", "本轮还在进行或有排队的消息，结束后再操作。");
		return false;
	}
	if (bRuntimeLost || GetState() != EShuttlegentAgentState::Ready || !Pinned->IsSessionOpen(Current->SessionId))
	{
		OutReason = LOCTEXT("HistoryNotOpen", "会话尚未在运行时中就绪。");
		return false;
	}
	if (!FShuttlegentMcpServerModule::Get().CanForkSessions())
	{
		OutReason = LOCTEXT("HistoryNoControl", "运行时的会话控制插件不可用。");
		return false;
	}

	OutReason = FText::GetEmpty();
	return true;
}

void FShuttlegentChatModel::BranchFrom(int32 EntryId)
{
	int32 Index = INDEX_NONE;
	int32 Ordinal = INDEX_NONE;
	if (!FindUserMessage(*Current, EntryId, Index, Ordinal)) return;

	FText Reason;
	if (!CanEditHistory(Reason))
	{
		AddNotice(*Current, FText::Format(LOCTEXT("BranchBlocked", "不能分支：{0}"), Reason), true);
		return;
	}

	const FString SourceSessionId = Current->SessionId;
	const TWeakPtr<FShuttlegentChatModel> WeakThis = AsShared();
	AddRunLog(*Current, LOCTEXT("LogBranch", "从这条消息分支"), false);
	FShuttlegentMcpServerModule::Get().ForkSession(SourceSessionId, Ordinal, true, [WeakThis, SourceSessionId, Ordinal](const FString& NewSessionId, const FString& Error)
	{
		const TSharedPtr<FShuttlegentChatModel> This = WeakThis.Pin();
		if (!This.IsValid()) return;

		if (NewSessionId.IsEmpty())
		{
			const TSharedPtr<FShuttlegentConversation> Source = This->FindConversation(SourceSessionId);
			if (Source.IsValid()) This->AddNotice(*Source, FText::Format(LOCTEXT("BranchFailed", "分支失败：{0}"), FText::FromString(Error)), true);
			return;
		}
		This->AdoptFork(SourceSessionId, NewSessionId, Ordinal, true, false);
	});
}

void FShuttlegentChatModel::RewindTo(int32 EntryId)
{
	int32 Index = INDEX_NONE;
	int32 Ordinal = INDEX_NONE;
	if (!FindUserMessage(*Current, EntryId, Index, Ordinal)) return;

	FText Reason;
	if (!CanEditHistory(Reason))
	{
		AddNotice(*Current, FText::Format(LOCTEXT("RewindBlocked", "不能回到这里：{0}"), Reason), true);
		return;
	}

	// 用户在编辑器里另做的编辑夹在中间时，直接撤销会连带撤掉它们，因此整体拒绝
	const TArray<TSharedPtr<FShuttlegentChatEntry>> Writes = ShuttlegentChatModelPrivate::UndoableWritesAfter(*Current, Index);
	if (!ShuttlegentChatModelPrivate::AreWritesOnUndoTop(Writes))
	{
		AddNotice(*Current, LOCTEXT("RewindNotTop", "不能回到这里：这之后编辑器里还有其他编辑，直接撤销会连带撤掉它们。未做任何改动。"), true);
		return;
	}

	const FString SourceSessionId = Current->SessionId;
	// 放回输入框的是用户写的原文，不含随提问附带的引用块
	FString Draft;
	TArray<FString> ReferenceLines;
	FShuttlegentReferencePrompt::Split(Current->Entries[Index]->Text, Draft, ReferenceLines);
	AddRunLog(*Current, LOCTEXT("LogRewind", "回到这条消息之前"), false);

	// 第一条消息之前没有对话可保留：撤销写入后开始新对话
	if (Ordinal == 0)
	{
		ShuttlegentChatModelPrivate::UndoWrites(Writes);
		ArchiveSession(SourceSessionId);
		OnDraftRestored.Broadcast(Draft);
		return;
	}

	// 先分叉（可能失败），成功后再撤销写入，失败时编辑器与对话都保持原样
	const TWeakPtr<FShuttlegentChatModel> WeakThis = AsShared();
	FShuttlegentMcpServerModule::Get().ForkSession(SourceSessionId, Ordinal, false, [WeakThis, SourceSessionId, Ordinal, Draft, Writes](const FString& NewSessionId, const FString& Error)
	{
		const TSharedPtr<FShuttlegentChatModel> This = WeakThis.Pin();
		if (!This.IsValid()) return;

		const TSharedPtr<FShuttlegentConversation> Source = This->FindConversation(SourceSessionId);
		if (NewSessionId.IsEmpty())
		{
			if (Source.IsValid()) This->AddNotice(*Source, FText::Format(LOCTEXT("RewindFailed", "回退失败：{0}。未做任何改动。"), FText::FromString(Error)), true);
			return;
		}
		if (!ShuttlegentChatModelPrivate::AreWritesOnUndoTop(Writes))
		{
			if (Source.IsValid()) This->AddNotice(*Source, LOCTEXT("RewindRaced", "不能回到这里：等待期间编辑器里又有了其他编辑。未做任何改动。"), true);
			return;
		}

		ShuttlegentChatModelPrivate::UndoWrites(Writes);
		This->AdoptFork(SourceSessionId, NewSessionId, Ordinal, false, true);
		This->OnDraftRestored.Broadcast(Draft);
	});
}

bool FShuttlegentChatModel::FindUserMessage(const FShuttlegentConversation& Conversation, int32 EntryId, int32& OutIndex, int32& OutOrdinal) const
{
	int32 Ordinal = INDEX_NONE;
	for (int32 Index = 0; Index < Conversation.Entries.Num(); ++Index)
	{
		const TSharedPtr<FShuttlegentChatEntry>& Entry = Conversation.Entries[Index];
		if (Entry->Kind != EShuttlegentChatEntryKind::User) continue;

		++Ordinal;
		if (Entry->Id != EntryId) continue;

		OutIndex = Index;
		OutOrdinal = Ordinal;
		return true;
	}
	return false;
}

void FShuttlegentChatModel::AdoptFork(const FString& SourceSessionId, const FString& NewSessionId, int32 Ordinal, bool bThroughTurn, bool bRewind)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	// 会话记录截在与运行时相同的位置：第 Ordinal 条提问之前，或该轮结束之后
	FShuttlegentSessionStore& Store = Pinned->GetSessionStore();
	const TArray<TSharedPtr<FJsonObject>> Items = Store.LoadTranscript(SourceSessionId);
	int32 Keep = Items.Num();
	int32 Seen = INDEX_NONE;
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		FString Kind;
		Items[Index]->TryGetStringField(ShuttlegentChatModelPrivate::FieldKind, Kind);
		if (Kind == ShuttlegentChatModelPrivate::TranscriptPrompt && ++Seen == Ordinal && !bThroughTurn)
		{
			Keep = Index;
			break;
		}
		if (bThroughTurn && Seen == Ordinal && Kind == ShuttlegentChatModelPrivate::TranscriptTurnEnd)
		{
			Keep = Index + 1;
			break;
		}
	}
	Store.WriteTranscript(NewSessionId, TArray<TSharedPtr<FJsonObject>>(Items.GetData(), Keep));

	FShuttlegentSessionRecord Record;
	const FShuttlegentSessionRecord* Source = Store.FindRecord(SourceSessionId);
	if (Source != nullptr) Record = *Source;
	Record.SessionId = NewSessionId;
	Record.bArchived = false;
	if (!bRewind) Record.Title = FText::Format(LOCTEXT("BranchTitle", "{0}（分支）"), FText::FromString(Record.Title)).ToString();
	Store.UpsertRecord(Record);

	// 回退后原会话归档保留，可从“已归档”恢复
	if (bRewind) ArchiveSession(SourceSessionId);
	OpenSession(NewSessionId);
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
	Current->WriteMode = ShuttlegentChatModelPrivate::DefaultWriteMode();
	Conversations.Add(Current);
	BroadcastAll();
}

void FShuttlegentChatModel::OpenSession(const FString& SessionId)
{
	if (!Service.IsValid() || SessionId.IsEmpty() || SessionId == Current->SessionId) return;

	LeaveCurrent();

	// 已持有的对话按原样显示：仍在运行，或处于空闲并带有其条目与运行日志
	const TSharedPtr<FShuttlegentConversation> Held = FindConversation(SessionId);
	if (Held.IsValid())
	{
		Current = Held;
		BroadcastAll();
		return;
	}

	Current = MakeShared<FShuttlegentConversation>();
	Current->SessionId = SessionId;
	Current->WriteMode = ShuttlegentChatModelPrivate::DefaultWriteMode();
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

	// 其他已持有的对话在下次发送提示时恢复其会话
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

	// 编辑器写入仅在面板可供询问时才进行确认；解除挂接后，写入门会拒绝它们
	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (Gate.IsValid()) Gate->SetConfirmerAttached(bAttached);

	// 问题同理：没有面板时，Agent 改为在回复中提问
	const TSharedPtr<FShuttlegentQuestionGate> Questions = QuestionGate.Pin();
	if (Questions.IsValid()) Questions->SetAnswererAttached(bAttached);

	if (bAttached)
	{
		Pinned->PermissionHandler.BindSP(this, &FShuttlegentChatModel::HandlePermissionRequest);
		return;
	}

	Pinned->PermissionHandler.Unbind();

	// 面板已无法答复的请求一律按拒绝处理。
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

	// 没有已打开会话的对话显示最近任一会话上报的选项，以及为该对话选择的值
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

EShuttlegentAttention FShuttlegentChatModel::GetSessionAttention(const FString& SessionId) const
{
	const TSharedPtr<FShuttlegentConversation> Conversation = FindConversation(SessionId);
	return Conversation.IsValid() ? GetAttention(*Conversation) : EShuttlegentAttention::None;
}

EShuttlegentAttention FShuttlegentChatModel::GetOverallAttention() const
{
	EShuttlegentAttention Overall = EShuttlegentAttention::None;
	for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
	{
		const EShuttlegentAttention Attention = GetAttention(*Conversation);
		if (Attention == EShuttlegentAttention::Input) return Attention;
		if (Attention == EShuttlegentAttention::Done) Overall = Attention;
	}
	return Overall;
}

void FShuttlegentChatModel::SetPanelVisible(bool bVisible)
{
	if (bPanelVisible == bVisible) return;

	bPanelVisible = bVisible;
	if (MarkCurrentSeen()) OnSessionsChanged.Broadcast();
}

bool FShuttlegentChatModel::NeedsInput(const FShuttlegentConversation& Conversation) const
{
	const EShuttlegentChatStatus Status = GetStatus(Conversation);
	if (Status == EShuttlegentChatStatus::AwaitingWriteConfirmation || Status == EShuttlegentChatStatus::AwaitingPermission || Status == EShuttlegentChatStatus::AwaitingPlanReview) return true;

	return GetPendingQuestion() != nullptr && IsPrompting(Conversation);
}

EShuttlegentAttention FShuttlegentChatModel::GetAttention(const FShuttlegentConversation& Conversation) const
{
	if (NeedsInput(Conversation)) return EShuttlegentAttention::Input;
	return Conversation.bUnseenFinish ? EShuttlegentAttention::Done : EShuttlegentAttention::None;
}

bool FShuttlegentChatModel::MarkCurrentSeen()
{
	if (!bPanelVisible || !Current->bUnseenFinish) return false;

	Current->bUnseenFinish = false;
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

	// 已归档的会话不在后台继续运行；其队列随之一并丢弃
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

void FShuttlegentChatModel::HandleSessionEvent(const FShuttlegentSessionEvent& Event)
{
	const TSharedPtr<FShuttlegentConversation> Conversation = FindConversation(Event.SessionId);
	if (!Conversation.IsValid()) return;

	if (Event.Type == ShuttlegentChatModelPrivate::EventTodoWrite)
	{
		ApplyTodos(*Conversation, Event.Data);
	}
	else if (Event.Type.StartsWith(ShuttlegentChatModelPrivate::EventCompactionPrefix))
	{
		ApplyCompaction(*Conversation, Event.Type, Event.Data);
	}
	else if (Event.Type == ShuttlegentChatModelPrivate::EventPlanMode && Event.Data.IsValid())
	{
		bool bActive = false;
		Event.Data->TryGetBoolField(ShuttlegentChatModelPrivate::FieldActive, bActive);
		if (Conversation->bPlanMode == bActive) return;

		Conversation->bPlanMode = bActive;
		AddRunLog(*Conversation, bActive ? LOCTEXT("LogPlanOn", "进入计划模式") : LOCTEXT("LogPlanOff", "退出计划模式"), false);
		NotifyStatus(*Conversation);
	}
}

bool FShuttlegentChatModel::CanSetPlanMode(FText& OutReason) const
{
	if (Current->bResuming)
	{
		OutReason = LOCTEXT("PlanResuming", "会话正在恢复。");
		return false;
	}
	if (!Current->PendingPlanReview.ReviewId.IsEmpty())
	{
		OutReason = LOCTEXT("PlanReviewing", "先审批 Agent 交出的计划。");
		return false;
	}

	// 会话未创建时记下选择，就绪后应用；已创建的会话需要运行时的 session control 插件
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	const bool bOpen = Pinned.IsValid() && !Current->SessionId.IsEmpty() && Pinned->IsSessionOpen(Current->SessionId);
	if (bOpen && !FShuttlegentMcpServerModule::Get().CanForkSessions())
	{
		OutReason = LOCTEXT("PlanNoControl", "运行时的会话控制插件不可用。");
		return false;
	}
	if (!Current->SessionId.IsEmpty() && !bOpen)
	{
		OutReason = LOCTEXT("PlanNotOpen", "会话尚未在运行时中就绪。");
		return false;
	}

	OutReason = FText::GetEmpty();
	return true;
}

void FShuttlegentChatModel::SetPlanMode(bool bActive)
{
	FText Reason;
	if (!CanSetPlanMode(Reason) || Current->bPlanMode == bActive) return;

	ApplyPlanMode(Current, bActive, nullptr);
}

void FShuttlegentChatModel::ApplyPlanMode(const TSharedPtr<FShuttlegentConversation>& Conversation, bool bActive, TFunction<void(bool)> OnApplied)
{
	Conversation->bPlanMode = bActive;
	NotifyStatus(*Conversation);
	if (Conversation->SessionId.IsEmpty())
	{
		if (OnApplied) OnApplied(true);
		return;
	}

	// 先按用户的选择显示；运行时拒绝时退回，回合进行中则以之后的 plan/mode 事件为准
	const TWeakPtr<FShuttlegentChatModel> WeakThis = AsShared();
	const TWeakPtr<FShuttlegentConversation> WeakConversation = Conversation;
	FShuttlegentMcpServerModule::Get().SetPlanMode(Conversation->SessionId, bActive, [WeakThis, WeakConversation, bActive, OnApplied](bool, const FString& Error)
	{
		const TSharedPtr<FShuttlegentChatModel> This = WeakThis.Pin();
		const TSharedPtr<FShuttlegentConversation> Held = WeakConversation.Pin();
		if (!This.IsValid() || !Held.IsValid()) return;

		if (!Error.IsEmpty())
		{
			Held->bPlanMode = !bActive;
			This->AddNotice(*Held, FText::Format(LOCTEXT("PlanSetFailed", "切换计划模式失败：{0}"), FText::FromString(Error)), true);
			This->NotifyStatus(*Held);
		}
		if (OnApplied) OnApplied(Error.IsEmpty());
	});
}

void FShuttlegentChatModel::SubmitInPlanMode(const TSharedPtr<FShuttlegentConversation>& Conversation, const FString& Text, const TArray<FShuttlegentPromptImage>& Images)
{
	// 会话未创建：只记下选择，会话就绪后先应用计划模式再发送
	if (Conversation->SessionId.IsEmpty())
	{
		ApplyPlanMode(Conversation, true, nullptr);
		SubmitPrompt(*Conversation, Text, Images);
		return;
	}

	// 计划模式走控制端口、提示走 ACP，两者没有先后保证；提示先挂起（对话随之忙碌，新草稿排队），运行时确认后再发出
	Conversation->PendingPrompt = Text;
	Conversation->PendingImages = Images;
	const TWeakPtr<FShuttlegentChatModel> WeakThis = AsShared();
	const TWeakPtr<FShuttlegentConversation> WeakConversation = Conversation;
	ApplyPlanMode(Conversation, true, [WeakThis, WeakConversation](bool bApplied)
	{
		const TSharedPtr<FShuttlegentChatModel> This = WeakThis.Pin();
		const TSharedPtr<FShuttlegentConversation> Held = WeakConversation.Pin();
		if (!This.IsValid() || !Held.IsValid()) return;

		if (bApplied) This->SendPendingPrompt(*Held);
		else This->ParkPendingPrompt(*Held);
	});
}

void FShuttlegentChatModel::HandlePlanReview(const FShuttlegentPlanReview& Review)
{
	const TSharedPtr<FShuttlegentConversation> Conversation = FindConversation(Review.SessionId);
	if (!Conversation.IsValid())
	{
		// 不属于面板的会话无人审批，退回让 Agent 不再等待
		FShuttlegentMcpServerModule::Get().AnswerPlanReview(Review.ReviewId, false, ShuttlegentChatModelPrivate::PlanReviewNoConversation, [](const FString&) {});
		return;
	}

	Conversation->PendingPlanReview = Review;
	const TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(*Conversation, EShuttlegentChatEntryKind::Plan, Review.Plan);
	Entry->Status = ShuttlegentEditorWebField::PlanPending;
	Conversation->PlanEntryId = Entry->Id;
	AddRunLog(*Conversation, LOCTEXT("LogPlanReview", "Agent 交出计划，等待审批"), true);
	NotifyStructure(*Conversation);
	NotifyStatus(*Conversation);
}

void FShuttlegentChatModel::HandlePlanReviewClosed(const FString& SessionId, const FString& ReviewId)
{
	const TSharedPtr<FShuttlegentConversation> Conversation = FindConversation(SessionId);
	if (!Conversation.IsValid() || Conversation->PendingPlanReview.ReviewId != ReviewId) return;

	SettlePlanReview(*Conversation, ShuttlegentEditorWebField::PlanCancelled, FString());
}

void FShuttlegentChatModel::AnswerPlanReview(bool bApprove, const FString& Feedback, bool bAcceptEdits)
{
	const TSharedPtr<FShuttlegentConversation> Conversation = Current;
	const FString ReviewId = Conversation->PendingPlanReview.ReviewId;
	if (ReviewId.IsEmpty()) return;

	// 批准时选定执行阶段的写入确认方式，在 Agent 开始写入之前生效
	if (bApprove) Conversation->WriteMode = bAcceptEdits ? EShuttlegentWriteMode::AcceptEdits : EShuttlegentWriteMode::Manual;

	const FString Trimmed = Feedback.TrimStartAndEnd();
	SettlePlanReview(*Conversation, bApprove ? ShuttlegentEditorWebField::PlanApproved : ShuttlegentEditorWebField::PlanRevised, bApprove ? FString() : Trimmed);

	const TWeakPtr<FShuttlegentChatModel> WeakThis = AsShared();
	const TWeakPtr<FShuttlegentConversation> WeakConversation = Conversation;
	FShuttlegentMcpServerModule::Get().AnswerPlanReview(ReviewId, bApprove, bApprove ? FString() : Trimmed, [WeakThis, WeakConversation](const FString& Error)
	{
		const TSharedPtr<FShuttlegentChatModel> This = WeakThis.Pin();
		const TSharedPtr<FShuttlegentConversation> Held = WeakConversation.Pin();
		if (!This.IsValid() || !Held.IsValid() || Error.IsEmpty()) return;

		This->AddNotice(*Held, FText::Format(LOCTEXT("PlanAnswerFailed", "审批结果未送达运行时：{0}"), FText::FromString(Error)), true);
	});
}

void FShuttlegentChatModel::SettlePlanReview(FShuttlegentConversation& Conversation, const TCHAR* Outcome, const FString& Feedback)
{
	TSharedPtr<FShuttlegentChatEntry>* Found = Conversation.Entries.FindByPredicate([&Conversation](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry->Id == Conversation.PlanEntryId; });
	if (Found != nullptr)
	{
		(*Found)->Status = Outcome;
		(*Found)->Detail = Feedback;

		const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
		if (Pinned.IsValid())
		{
			const TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
			Record->SetStringField(ShuttlegentChatModelPrivate::FieldPlan, (*Found)->Text);
			Record->SetStringField(ShuttlegentChatModelPrivate::FieldOutcome, Outcome);
			if (!Feedback.IsEmpty()) Record->SetStringField(ShuttlegentChatModelPrivate::FieldFeedback, Feedback);
			Pinned->GetSessionStore().AppendTranscript(Conversation.SessionId, ShuttlegentChatModelPrivate::TranscriptPlan, Record);
		}
	}

	Conversation.PendingPlanReview = FShuttlegentPlanReview();
	Conversation.PlanEntryId = 0;
	AddRunLog(Conversation, FText::Format(LOCTEXT("LogPlanSettled", "计划审批：{0}"), FText::FromString(Outcome)), false);
	NotifyStructure(Conversation);
	NotifyStatus(Conversation);
}

bool FShuttlegentChatModel::IsPlanTurnRunning() const
{
	return Conversations.ContainsByPredicate([this](const TSharedPtr<FShuttlegentConversation>& Conversation) { return Conversation->bPlanMode && IsBusy(*Conversation); });
}

EShuttlegentWritePolicy FShuttlegentChatModel::GetWritePolicy(const FShuttlegentWriteRequest& Request) const
{
	if (IsPlanTurnRunning()) return EShuttlegentWritePolicy::Block;
	if (Request.bDestructive) return EShuttlegentWritePolicy::Ask;

	// 写入请求不带会话：取正在运行的对话中最严格的模式（枚举按从严到宽排列）
	TOptional<EShuttlegentWriteMode> Mode;
	for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
	{
		if (!IsBusy(*Conversation)) continue;

		if (!Mode.IsSet() || static_cast<uint8>(Conversation->WriteMode) < static_cast<uint8>(Mode.GetValue())) Mode = Conversation->WriteMode;
	}
	if (!Mode.IsSet()) return EShuttlegentWritePolicy::Ask;

	if (Mode.GetValue() == EShuttlegentWriteMode::Auto) return EShuttlegentWritePolicy::Approve;
	if (Mode.GetValue() == EShuttlegentWriteMode::AcceptEdits && Request.bUndoable) return EShuttlegentWritePolicy::Approve;
	return EShuttlegentWritePolicy::Ask;
}

void FShuttlegentChatModel::SetWriteMode(EShuttlegentWriteMode Mode)
{
	FText Reason;
	if (Current->bPlanMode && CanSetPlanMode(Reason)) ApplyPlanMode(Current, false, nullptr);

	if (Current->WriteMode == Mode) return;

	Current->WriteMode = Mode;
	NotifyStatus(*Current);
}

void FShuttlegentChatModel::ApplyTodos(FShuttlegentConversation& Conversation, const TSharedPtr<FJsonObject>& Data)
{
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Data.IsValid() || !Data->TryGetArrayField(ShuttlegentChatModelPrivate::FieldTodos, Items)) return;

	Conversation.Todos.Reset();
	for (const TSharedPtr<FJsonValue>& Value : *Items)
	{
		const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Item.IsValid()) continue;

		FShuttlegentTodoItem Todo;
		Item->TryGetStringField(ShuttlegentChatModelPrivate::FieldContent, Todo.Content);
		Item->TryGetStringField(ShuttlegentChatModelPrivate::FieldStatus, Todo.Status);
		if (!Todo.Content.IsEmpty()) Conversation.Todos.Add(MoveTemp(Todo));
	}
	NotifyStatus(Conversation);
}

void FShuttlegentChatModel::ApplyCompaction(FShuttlegentConversation& Conversation, const FString& Type, const TSharedPtr<FJsonObject>& Data)
{
	if (!Data.IsValid()) return;

	if (Type == ShuttlegentChatModelPrivate::EventCompactionStart)
	{
		bool bManual = false;
		Data->TryGetBoolField(ShuttlegentChatModelPrivate::FieldManual, bManual);

		const TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(Conversation, EShuttlegentChatEntryKind::Compaction, FString());
		Entry->Status = ShuttlegentChatModelPrivate::CompactionRunning;
		Entry->Title = bManual ? ShuttlegentEditorWebField::Manual : ShuttlegentChatModelPrivate::CompactionAuto;
		Conversation.CompactionEntryId = Entry->Id;
		Conversation.bCompactionStarted = true;
		AddRunLog(Conversation, bManual ? LOCTEXT("LogCompactManual", "开始手动压缩上下文") : LOCTEXT("LogCompactAuto", "上下文接近上限，开始自动压缩"), false);
		NotifyStructure(Conversation);
		return;
	}

	TSharedPtr<FShuttlegentChatEntry>* Found = Conversation.Entries.FindByPredicate([&Conversation](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry->Id == Conversation.CompactionEntryId; });
	const TSharedPtr<FShuttlegentChatEntry> Entry = Found != nullptr ? *Found : nullptr;
	if (!Entry.IsValid()) return;

	if (Type == ShuttlegentChatModelPrivate::EventCompactionSummary)
	{
		int64 Shadowed = 0;
		if (Data->TryGetNumberField(ShuttlegentChatModelPrivate::FieldShadowedTokenCount, Shadowed)) Entry->Detail = LexToString(Shadowed);
		NotifyContent(Conversation);
		return;
	}
	if (Type != ShuttlegentChatModelPrivate::EventCompactionEnd) return;

	FString Error;
	Data->TryGetStringField(ShuttlegentChatModelPrivate::FieldError, Error);
	Entry->Status = Error.IsEmpty() ? ShuttlegentChatModelPrivate::CompactionDone : ShuttlegentChatModelPrivate::CompactionFailed;
	Entry->Text = Error;
	Entry->bError = !Error.IsEmpty();
	Conversation.CompactionEntryId = 0;

	// 压缩后的用量要等下一条助手消息的 usage_update 才会更新；插件随结束事件报来，先行显示
	int64 Used = 0;
	if (Data->TryGetNumberField(ShuttlegentChatModelPrivate::FieldUsedTokens, Used) && Used > 0) Conversation.UsedTokens = Used;

	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (Pinned.IsValid())
	{
		const TSharedRef<FJsonObject> Record = MakeShared<FJsonObject>();
		Record->SetBoolField(ShuttlegentChatModelPrivate::FieldManual, Entry->Title == ShuttlegentEditorWebField::Manual);
		Record->SetStringField(ShuttlegentChatModelPrivate::FieldShadowedTokenCount, Entry->Detail);
		if (!Error.IsEmpty()) Record->SetStringField(ShuttlegentChatModelPrivate::FieldError, Error);
		Pinned->GetSessionStore().AppendTranscript(Conversation.SessionId, ShuttlegentChatModelPrivate::TranscriptCompaction, Record);
	}

	AddRunLog(Conversation, Error.IsEmpty() ? LOCTEXT("LogCompactDone", "上下文压缩完成") : FText::Format(LOCTEXT("LogCompactFailed", "上下文压缩失败：{0}"), FText::FromString(Error)), !Error.IsEmpty());
	NotifyStructure(Conversation);
	NotifyStatus(Conversation);
}

bool FShuttlegentChatModel::CanCompact(FText& OutReason) const
{
	if (Current->bCompacting)
	{
		OutReason = LOCTEXT("CompactRunning", "正在压缩。");
		return false;
	}
	if (!CanEditHistory(OutReason)) return false;

	OutReason = FText::GetEmpty();
	return true;
}

void FShuttlegentChatModel::CompactContext()
{
	FText Reason;
	if (!CanCompact(Reason)) return;

	const TSharedPtr<FShuttlegentConversation> Conversation = Current;
	Conversation->bCompacting = true;
	Conversation->bCompactionStarted = false;
	NotifyStatus(*Conversation);

	const TWeakPtr<FShuttlegentChatModel> WeakThis = AsShared();
	const TWeakPtr<FShuttlegentConversation> WeakConversation = Conversation;
	FShuttlegentMcpServerModule::Get().RunCommand(Conversation->SessionId, ShuttlegentChatModelPrivate::CompactCommand, [WeakThis, WeakConversation](bool bSucceeded, const FString& Text)
	{
		const TSharedPtr<FShuttlegentChatModel> This = WeakThis.Pin();
		const TSharedPtr<FShuttlegentConversation> Held = WeakConversation.Pin();
		if (!This.IsValid() || !Held.IsValid()) return;

		Held->bCompacting = false;

		// 运行时没有报来压缩开始，说明没有可压缩的内容或命令失败；把命令的说明显示出来
		if (!bSucceeded)
		{
			This->AddNotice(*Held, FText::Format(LOCTEXT("CompactFailed", "压缩失败：{0}"), FText::FromString(Text)), true);
		}
		else if (!Held->bCompactionStarted)
		{
			This->AddNotice(*Held, Text.IsEmpty() ? LOCTEXT("CompactNothing", "没有可压缩的内容。") : FText::FromString(Text), false);
		}
		This->NotifyStatus(*Held);
	});
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
		if (!Conversation.PendingPlanReview.ReviewId.IsEmpty()) return EShuttlegentChatStatus::AwaitingPlanReview;
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
			// 第一条提示等待 session/new 完成
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
	// 无论其他持有者是否释放，都保持到本次调用结束
	const TSharedRef<FShuttlegentConversation> Dropped = Conversation.AsShared();

	RejectPendingPermissions(Conversation);
	AwaitingNewSession.RemoveAll([&Conversation](const TWeakPtr<FShuttlegentConversation>& Awaiting) { return Awaiting.Pin().Get() == &Conversation; });
	Conversations.RemoveAll([&Conversation](const TSharedPtr<FShuttlegentConversation>& Held) { return Held.Get() == &Conversation; });

	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (Pinned.IsValid() && Pinned->IsSessionOpen(Conversation.SessionId)) Pinned->CloseSession(Conversation.SessionId);
}

void FShuttlegentChatModel::BroadcastAll()
{
	// 切换到的对话此刻就在用户眼前
	MarkCurrentSeen();

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

	// 会话列表显示每个已持有对话的状态
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::NotifyQueue(const FShuttlegentConversation& Conversation)
{
	if (IsCurrent(Conversation)) OnQueueChanged.Broadcast();

	// 会话列表显示每个已持有对话的排队数量
	OnSessionsChanged.Broadcast();
}

TSharedPtr<FShuttlegentChatEntry> FShuttlegentChatModel::AddEntry(FShuttlegentConversation& Conversation, EShuttlegentChatEntryKind Kind, const FString& Text)
{
	TSharedPtr<FShuttlegentChatEntry> Entry = MakeShared<FShuttlegentChatEntry>();
	Entry->Kind = Kind;
	Entry->Text = Text;
	Entry->Id = NextEntryId++;
	Entry->TimeUtc = FDateTime::UtcNow();
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

void FShuttlegentChatModel::SubmitPrompt(FShuttlegentConversation& Conversation, const FString& Text, const TArray<FShuttlegentPromptImage>& Images)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || Text.TrimStartAndEnd().IsEmpty()) return;

	if (Pinned->GetState() == EShuttlegentAgentState::Ready && Pinned->IsSessionOpen(Conversation.SessionId))
	{
		SendPrompt(Conversation, Text, Images);
		return;
	}

	Conversation.PendingPrompt = Text;
	Conversation.PendingImages = Images;
	RequestSession(Conversation);
	NotifyStatus(Conversation);
}

void FShuttlegentChatModel::SendPrompt(FShuttlegentConversation& Conversation, const FString& Text, const TArray<FShuttlegentPromptImage>& Images)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	FString Error;
	TArray<FString> ImageFiles;
	if (!Pinned->Prompt(Conversation.SessionId, Text, Images, ImageFiles, Error))
	{
		AddNotice(Conversation, FText::Format(LOCTEXT("PromptFailed", "发送失败：{0}"), FText::FromString(Error)), true);
		return;
	}

	Conversation.LastOutcome = EShuttlegentTurnOutcome::None;
	Conversation.bStopping = false;
	Conversation.LastSentPrompt = Text;
	Conversation.LastSentImages = Images;

	// 运行时的待办在新回合开始时清空（todos 投影），面板随之清空，等 Agent 为本轮重新列出
	Conversation.Todos.Reset();
	const TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(Conversation, EShuttlegentChatEntryKind::User, Text);
	Entry->Images = MoveTemp(ImageFiles);

	// 运行时每次请求都带上记忆；每个对话第一次发送时提示一次带上了几条
	const int32 MemoryCount = Conversation.bMemoryNoticeShown ? 0 : FShuttlegentMemoryStore::Load().Num();
	Conversation.bMemoryNoticeShown = true;
	if (MemoryCount > 0)
	{
		const TSharedPtr<FShuttlegentChatEntry> Notice = AddEntry(Conversation, EShuttlegentChatEntryKind::Memory, FString::FromInt(MemoryCount));
		Notice->Status = ShuttlegentEditorWebField::MemoryRead;
	}

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
			const TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(Conversation, EShuttlegentChatEntryKind::User, Text);
			(*Data)->TryGetStringArrayField(ShuttlegentChatModelPrivate::FieldImages, Entry->Images);

			FString Time;
			FDateTime At;
			if (Item->TryGetStringField(ShuttlegentChatModelPrivate::FieldTime, Time) && FDateTime::ParseIso8601(*Time, At)) Entry->TimeUtc = At;
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
		else if (Kind == ShuttlegentChatModelPrivate::TranscriptCompaction)
		{
			bool bManual = false;
			(*Data)->TryGetBoolField(ShuttlegentChatModelPrivate::FieldManual, bManual);

			const TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(Conversation, EShuttlegentChatEntryKind::Compaction, FString());
			Entry->Title = bManual ? ShuttlegentEditorWebField::Manual : ShuttlegentChatModelPrivate::CompactionAuto;
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldShadowedTokenCount, Entry->Detail);
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldError, Entry->Text);
			Entry->bError = !Entry->Text.IsEmpty();
			Entry->Status = Entry->bError ? ShuttlegentChatModelPrivate::CompactionFailed : ShuttlegentChatModelPrivate::CompactionDone;
		}
		else if (Kind == ShuttlegentChatModelPrivate::TranscriptPlan)
		{
			FString Plan;
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldPlan, Plan);

			const TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(Conversation, EShuttlegentChatEntryKind::Plan, Plan);
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldOutcome, Entry->Status);
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldFeedback, Entry->Detail);
		}
	}
	NotifyStructure(Conversation);
}

void FShuttlegentChatModel::HandleStateChanged(EShuttlegentAgentState NewState)
{
	if (NewState == EShuttlegentAgentState::Ready)
	{
		// 已就绪的运行时已具备可执行文件与密钥；被拒绝的密钥会在下一个失败的回合再次显示
		StartIssue = EShuttlegentStartIssue::None;
		StartIssueDetail.Reset();

		// 运行时就绪过程中请求过会话的每个对话现在都获得会话
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

	// session/new 的答复归属最早一个等待它的对话；无人等待的会话不属于面板
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

	// 计划模式：新会话先应用用户在会话创建前的选择，再发送提示，第一步就按计划模式运行；恢复的会话以运行时记录为准
	if (FShuttlegentMcpServerModule::Get().CanForkSessions())
	{
		const TOptional<bool> Wanted = Conversation->bPlanMode ? TOptional<bool>(true) : TOptional<bool>();
		const TWeakPtr<FShuttlegentChatModel> WeakThis = AsShared();
		const TWeakPtr<FShuttlegentConversation> WeakConversation = Conversation;
		FShuttlegentMcpServerModule::Get().SetPlanMode(SessionId, Wanted, [WeakThis, WeakConversation](bool bActiveNow, const FString& Error)
		{
			const TSharedPtr<FShuttlegentChatModel> This = WeakThis.Pin();
			const TSharedPtr<FShuttlegentConversation> Held = WeakConversation.Pin();
			if (!This.IsValid() || !Held.IsValid()) return;

			if (Error.IsEmpty()) Held->bPlanMode = bActiveNow;
			This->SendPendingPrompt(*Held);
			This->NotifyStatus(*Held);
		});
	}
	else
	{
		SendPendingPrompt(*Conversation);
	}
	OnStatusChanged.Broadcast();
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::SendPendingPrompt(FShuttlegentConversation& Conversation)
{
	if (Conversation.PendingPrompt.IsEmpty()) return;

	const FString Text = MoveTemp(Conversation.PendingPrompt);
	const TArray<FShuttlegentPromptImage> Images = MoveTemp(Conversation.PendingImages);
	Conversation.PendingPrompt.Reset();
	Conversation.PendingImages.Reset();
	SendPrompt(Conversation, Text, Images);
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

	// 运行日志：工具调用的首次更新及其首次失败
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

	// 缺少密钥或密钥被拒绝会导致每个回合都失败，因此以启动引导卡片显示，而不是显示为某一个失败的回合
	const EShuttlegentStartIssue KeyIssue = ErrorMessage.IsEmpty() ? EShuttlegentStartIssue::None : FShuttlegentAgentService::ClassifyError(ErrorMessage);
	if (KeyIssue == EShuttlegentStartIssue::MissingKey || KeyIssue == EShuttlegentStartIssue::AuthFailed) RecordStartIssue(KeyIssue, ErrorMessage);

	if (Conversation.LastOutcome == EShuttlegentTurnOutcome::Completed) AddRunLog(Conversation, LOCTEXT("LogTurnCompleted", "本轮结束"), false);
	else if (Conversation.LastOutcome == EShuttlegentTurnOutcome::Stopped) AddRunLog(Conversation, LOCTEXT("LogTurnStopped", "本轮已停止"), true);
	else AddRunLog(Conversation, LOCTEXT("LogTurnFailed", "本轮查询失败"), true);

	// 回合结束时仍处于打开状态的请求已无法再处理。写入请求不指明其会话，
	// 因此只有在不再有任何会话运行回合时才会过期。
	RejectPendingPermissions(Conversation);
	const bool bAnyPrompting = Conversations.ContainsByPredicate([this](const TSharedPtr<FShuttlegentConversation>& Held) { return IsPrompting(*Held); });
	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (Gate.IsValid() && !bAnyPrompting) Gate->ExpireAll();
	const TSharedPtr<FShuttlegentQuestionGate> Questions = QuestionGate.Pin();
	if (Questions.IsValid() && !bAnyPrompting) Questions->ExpireAll();

	ApplyTurnEnd(Conversation, StopReason, ErrorMessage);

	// 用户没在看这个对话时留下“完成未看”；用户自己停止的回合不提醒
	const bool bSeen = IsCurrent(Conversation) && bPanelVisible;
	if (Conversation.LastOutcome != EShuttlegentTurnOutcome::Stopped && !bSeen) Conversation.bUnseenFinish = true;

	if (IsCurrent(Conversation)) OnTurnFinished.Broadcast(bCompleted);
	NotifyStructure(Conversation);
	NotifyStatus(Conversation);

	if (bCompleted) SendNextQueued(Conversation);
	else PauseQueue(Conversation);
}

void FShuttlegentChatModel::HandleRuntimeExited(int32 ExitCode, bool bExpected)
{
	AwaitingNewSession.Reset();
	ModelImageInput.Reset();
	if (!bExpected) bRuntimeLost = true;
	ParkPendingPrompts();

	// 已确认但尚未收到结果的写入可能已执行也可能未执行；由用户核对其对象
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

	// 同一个运行时服务所有会话，因此所有对话随之一并停止
	for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
	{
		Conversation->bStopping = false;
		RejectPendingPermissions(*Conversation);

		// 运行时退出后等待中的计划无人接收审批
		if (!Conversation->PendingPlanReview.ReviewId.IsEmpty()) SettlePlanReview(*Conversation, ShuttlegentEditorWebField::PlanCancelled, FString());
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

	// 重试已停止运行时，以便其读取修正后的密钥或可执行文件；现在再次启动
	if (bRestartPending)
	{
		bRestartPending = false;
		if (!Current->SessionId.IsEmpty()) RequestSession(*Current);
		else EnsureRuntime();
	}
}

void FShuttlegentChatModel::HandleError(const FString& SessionId, const FString& Message)
{
	// 不带会话的错误是失败的 session/new（属于最早一个等待的对话）或失败的启动（属于当前对话）
	TSharedPtr<FShuttlegentConversation> Conversation = FindConversation(SessionId);
	while (!Conversation.IsValid() && SessionId.IsEmpty() && !AwaitingNewSession.IsEmpty())
	{
		Conversation = AwaitingNewSession[0].Pin();
		AwaitingNewSession.RemoveAt(0);
	}
	// 失败的 initialize 不属于任何对话；由启动引导卡片显示
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Conversation.IsValid() && SessionId.IsEmpty() && Pinned.IsValid() && Pinned->GetStartIssue() != EShuttlegentStartIssue::None)
	{
		RecordStartIssue(Pinned->GetStartIssue(), Message);
		OnStatusChanged.Broadcast();
		return;
	}
	if (!Conversation.IsValid()) Conversation = Current;

	// 本地记录保留；横幅提供再次恢复，或开始一个不带其上下文的新会话
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

		TArray<FShuttlegentConfigChoice> Models;
		FString ModelValue;
		ShuttlegentChatModelPrivate::ParseConfigChoices(Options, ShuttlegentEditorConfigId::Model, Models, ModelValue);
		RequestImageInput(ModelValue);
	}

	OnStatusChanged.Broadcast();
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::RequestImageInput(const FString& ModelValue)
{
	if (ModelValue.IsEmpty() || ModelImageInput.Contains(ModelValue) || PendingImageQueries.Contains(ModelValue)) return;

	PendingImageQueries.Add(ModelValue);
	const TWeakPtr<FShuttlegentChatModel> WeakThis = AsShared();
	FShuttlegentMcpServerModule::Get().QueryImageInput(ModelValue, [WeakThis, ModelValue](TOptional<bool> bImage)
	{
		const TSharedPtr<FShuttlegentChatModel> This = WeakThis.Pin();
		if (!This.IsValid()) return;

		// 请求失败（如控制端口尚未报来）不记结果，下次配置变化时重新确认
		This->PendingImageQueries.Remove(ModelValue);
		if (!bImage.IsSet()) return;

		This->ModelImageInput.Add(ModelValue, bImage.GetValue());
		This->OnStatusChanged.Broadcast();
	});
}

void FShuttlegentChatModel::HandlePermissionRequest(const TSharedPtr<FJsonObject>& Params, TFunction<void(bool)> Respond)
{
	FString SessionId;
	if (Params.IsValid()) Params->TryGetStringField(ShuttlegentChatModelPrivate::FieldSessionId, SessionId);

	// 面板未持有的会话所发出的请求无人可答复
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

	// 关闭权限提升时，请求立即得到答复并留下可读记录（交互规范第 3 节）
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
	// 写入请求不指明其会话；它们归属第一个请求到达时所显示的对话
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
		AddRunLog(*Owner, ApprovedWrite.bAutoApproved ? LOCTEXT("LogWriteAutoApproved", "按写入模式直接执行编辑器写入") : LOCTEXT("LogWriteApproved", "确认执行编辑器写入"), false);
	}
	else
	{
		// 未写入任何内容；与停止相同，队列等待用户处理
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
	// 运行时丢失后才到达的结果会替换其未知条目
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
	Entry->bAutoApproved = ApprovedWrite.RequestId == Result.RequestId && ApprovedWrite.bAutoApproved;

	// 未完成全部对象的批次会使队列停止（交互规范第 4.3 节）
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
	// 任何新的事务或撤销都可能改变某个结果是否仍位于顶部
	const bool bHasWriteResults = Current->Entries.ContainsByPredicate([](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry.IsValid() && Entry->Kind == EShuttlegentChatEntryKind::WriteResult; });
	if (bHasWriteResults) OnContentChanged.Broadcast();
}

void FShuttlegentChatModel::HandleQuestionsChanged()
{
	// 面板在输入区的位置显示第一个等待中的问题
	NotifyStatus(*Current);
}

void FShuttlegentChatModel::HandleQuestionClosed(const FShuttlegentQuestion& Question, EShuttlegentQuestionOutcome Outcome, const FString& Answer)
{
	if (Question.Question.IsEmpty()) return;

	// 对话记录保留所提的问题及其结束方式
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
