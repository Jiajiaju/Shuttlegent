#include "Chat/ShuttlegentChatModel.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Chat/FShuttlegentChoiceJson.h"
#include "Chat/FShuttlegentToolPresentation.h"
#include "Editor.h"
#include "Editor/TransBuffer.h"
#include "Misc/Base64.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
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
#include "FShuttlegentSessionTitler.h"
#include "ShuttlegentAgentService.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentSessionStore.h"
#include "ShuttlegentSettings.h"
#include "EShuttlegentConnectionStatus.h"
#include "FShuttlegentConnection.h"
#include "FShuttlegentConnectionTester.h"
#include "FShuttlegentModelRoutes.h"
#include "ShuttlegentConstants.h"
#include "Settings/FShuttlegentSettingsChoices.h"

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

	// 选择卡（ask_choices）的会话记录；内容见 FShuttlegentChoiceJson，同一张卡片可有多条，回放时以最后一条为准
	inline const TCHAR* TranscriptChoice = TEXT("choice");

	// 提交选择卡后发出的用户消息：标题行、选中项（{0} 标题、{1} id）、没有选中、逐项评语、整体意见
	inline const TCHAR* ChoiceMessageHeader = TEXT("关于“{0}”，我的选择：");
	inline const TCHAR* ChoiceMessageSelected = TEXT("\n- 选中：");
	inline const TCHAR* ChoiceMessageItem = TEXT("{0}（id: {1}）");
	inline const TCHAR* ChoiceMessageItemSeparator = TEXT("、");
	inline const TCHAR* ChoiceMessageNoneSelected = TEXT("\n- 没有选中任何一项");
	inline const TCHAR* ChoiceMessageNote = TEXT("\n- {0}（id: {1}）的评语：{2}");
	inline const TCHAR* ChoiceMessageOverall = TEXT("\n- 整体意见：{0}");

	/** 选择卡提交后作为用户消息发给 Agent 的文字：选中项与评语都带上选项 id。 */
	FString ComposeChoiceMessage(const FShuttlegentChoiceRequest& Request, const FShuttlegentChoiceAnswer& Answer)
	{
		FString Message = FString::Format(ChoiceMessageHeader, { Request.Question });

		TArray<FString> Picked;
		for (const FShuttlegentChoiceOption& Option : Request.Options)
		{
			if (Answer.Selected.Contains(Option.Id)) Picked.Add(FString::Format(ChoiceMessageItem, { Option.Title, Option.Id }));
		}
		Message += Picked.IsEmpty() ? FString(ChoiceMessageNoneSelected) : ChoiceMessageSelected + FString::Join(Picked, ChoiceMessageItemSeparator);

		for (const FShuttlegentChoiceOption& Option : Request.Options)
		{
			const FString* Note = Answer.ItemNotes.Find(Option.Id);
			if (Note != nullptr) Message += FString::Format(ChoiceMessageNote, { Option.Title, Option.Id, *Note });
		}
		if (!Answer.OverallNote.IsEmpty()) Message += FString::Format(ChoiceMessageOverall, { Answer.OverallNote });
		return Message;
	}

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
	/** 对话的 Agent 已锁定：已有会话（发出过消息或打开的是已有会话），或已有条目。 */
	bool IsAgentLocked(const FShuttlegentConversation& Conversation)
	{
		return !Conversation.SessionId.IsEmpty() || !Conversation.Entries.IsEmpty();
	}

	/** 已启用的 Agent，按设置中的顺序。 */
	TArray<FString> EnabledAgents()
	{
		TArray<FString> Ids;
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		if (!IsValid(Settings)) return Ids;

		for (const FShuttlegentAgentEntry& Entry : Settings->Agents)
		{
			if (Entry.bEnabled) Ids.Add(Entry.AgentId);
		}
		return Ids;
	}

	/** 新对话默认使用的 Agent：默认值中的 Agent 已启用时用它，否则为默认 Agent。 */
	FString DefaultAgent()
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		const FString Wanted = IsValid(Settings) ? Settings->DefaultAgent : FString();
		return EnabledAgents().Contains(Wanted) ? Wanted : FString(ShuttlegentAgentConstant::DefaultAgentId);
	}

	/** 模型选项的取值（["路由", "模型 ID"] 的 JSON 文本）拆成路由与模型 ID；不是这种形状时返回 false。 */
	bool ParseModelValue(const FString& Value, FString& OutRoute, FString& OutModelId)
	{
		TArray<TSharedPtr<FJsonValue>> Parts;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Value);
		if (!FJsonSerializer::Deserialize(Reader, Parts) || Parts.Num() < 2 || !Parts[0].IsValid() || !Parts[1].IsValid()) return false;

		OutRoute = Parts[0]->AsString();
		OutModelId = Parts[1]->AsString();
		return true;
	}

	/** 模型选项中路由与模型 ID 都对上的那一项的取值；没有时为空。 */
	FString FindModelChoice(const TArray<FShuttlegentConfigChoice>& Choices, const FString& Route, const FString& ModelId)
	{
		for (const FShuttlegentConfigChoice& Choice : Choices)
		{
			FString ChoiceRoute;
			FString ChoiceModel;
			if (ParseModelValue(Choice.Value, ChoiceRoute, ChoiceModel) && ChoiceRoute == Route && ChoiceModel == ModelId) return Choice.Value;
		}
		return FString();
	}

	/** 模型选项的取值：["路由", "模型 ID"] 的 JSON 文本，与运行时上报的形状一致。 */
	FString MakeModelValue(const FString& Route, const FString& ModelId)
	{
		const TArray<TSharedPtr<FJsonValue>> Parts = { MakeShared<FJsonValueString>(Route), MakeShared<FJsonValueString>(ModelId) };
		FString Value;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Value);
		FJsonSerializer::Serialize(Parts, Writer);
		return Value;
	}

	/** 模型的显示名：内置模型用运行时目录中的显示名，其余即模型 ID。 */
	FString GetModelName(const FShuttlegentModelRoute& Route, const FString& ModelId)
	{
		if (!Route.bBuiltin) return ModelId;

		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ShuttlegentBuiltinModel::Ids); ++Index)
		{
			if (ModelId == ShuttlegentBuiltinModel::Ids[Index]) return ShuttlegentBuiltinModel::Names[Index];
		}
		return ModelId;
	}

	/**
	 * 按个人设置列出可选模型：已连通连接的全部模型，当前值为默认模型（第一条已连通连接的第一个模型）。
	 * 会话尚未打开的对话用它显示，不必等运行时上报，设置一改就能看到。
	 */
	void BuildModelChoices(TArray<FShuttlegentConfigChoice>& OutChoices, FString& OutDefaultValue)
	{
		OutChoices.Reset();
		OutDefaultValue.Reset();
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		if (!IsValid(Settings)) return;

		for (const FShuttlegentModelRoute& Route : FShuttlegentModelRoutes::Build(*Settings))
		{
			const bool bConnected = Settings->Connections.IsValidIndex(Route.ConnectionIndex) && Settings->Connections[Route.ConnectionIndex].Status == EShuttlegentConnectionStatus::Connected;
			if (!bConnected) continue;

			for (const FString& ModelId : Route.ModelIds)
			{
				OutChoices.Add(FShuttlegentConfigChoice { MakeModelValue(Route.Route, ModelId), GetModelName(Route, ModelId) });
			}
		}

		FShuttlegentModelRoute DefaultRoute;
		FString DefaultModel;
		if (FShuttlegentModelRoutes::FindDefaultModel(*Settings, DefaultRoute, DefaultModel)) OutDefaultValue = MakeModelValue(DefaultRoute.Route, DefaultModel);
	}

	/**
	 * 运行时启动时从个人设置读到的模型配置的签名：各路由的名称、协议、地址、密钥（CRC）与模型，以及默认模型。
	 * 签名变了说明运行时手里的路由已过时，需要重启。
	 */
	FString MakeModelSignature()
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		if (!IsValid(Settings)) return FString();

		TStringBuilder<512> Builder;
		for (const FShuttlegentModelRoute& Route : FShuttlegentModelRoutes::Build(*Settings))
		{
			Builder << Route.Route << TEXT('|') << FShuttlegentModelRoutes::GetApiName(Route.Api) << TEXT('|') << Route.BaseUrl << TEXT('|');
			Builder.Appendf(TEXT("%08x"), FCrc::StrCrc32(*Route.ApiKey));
			Builder << TEXT('|') << FString::Join(Route.ModelIds, TEXT(",")) << TEXT(';');
		}

		FShuttlegentModelRoute DefaultRoute;
		FString DefaultModel;
		if (FShuttlegentModelRoutes::FindDefaultModel(*Settings, DefaultRoute, DefaultModel)) Builder << TEXT("default=") << DefaultRoute.Route << TEXT('/') << DefaultModel;
		return Builder.ToString();
	}

	/** Agent 指定的默认模型的选项取值（见 MakeModelValue）；Agent 没有指定、或模型不在已连通的连接中时为空。 */
	FString AgentModelValue(const FString& AgentId)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		if (!IsValid(Settings)) return FString();

		const FShuttlegentAgentEntry* Agent = Settings->Agents.FindByPredicate([&AgentId](const FShuttlegentAgentEntry& Entry) { return Entry.AgentId == AgentId; });
		FShuttlegentModelRoute Route;
		FString ModelId;
		if (Agent == nullptr || !FShuttlegentModelRoutes::FindModel(*Settings, Agent->DefaultModel, Route, ModelId)) return FString();
		return MakeModelValue(Route.Route, ModelId);
	}

	/** 新对话的写入确认方式，取自编辑器偏好设置。 */
	EShuttlegentWriteMode DefaultWriteMode()
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		return IsValid(Settings) ? Settings->DefaultWriteMode : EShuttlegentWriteMode::Manual;
	}

	/** 会话记录里提问所附图片的文件（按内容保存在会话图片目录）读回成可重发的图片；读不到或类型不认识的跳过，bOutAll 随之为 false。 */
	TArray<FShuttlegentPromptImage> LoadPromptImages(const TArray<FString>& Files, bool& bOutAll)
	{
		static const TMap<FString, FString> MimeByExtension = {
			{ TEXT("png"), TEXT("image/png") }, { TEXT("jpg"), TEXT("image/jpeg") }, { TEXT("jpeg"), TEXT("image/jpeg") },
			{ TEXT("webp"), TEXT("image/webp") }, { TEXT("gif"), TEXT("image/gif") } };

		bOutAll = true;
		TArray<FShuttlegentPromptImage> Images;
		for (const FString& File : Files)
		{
			const FString* MimeType = MimeByExtension.Find(FPaths::GetExtension(File).ToLower());
			TArray<uint8> Bytes;
			if (MimeType == nullptr || !FFileHelper::LoadFileToArray(Bytes, *File) || Bytes.IsEmpty())
			{
				bOutAll = false;
				continue;
			}

			FShuttlegentPromptImage Image;
			Image.MimeType = *MimeType;
			Image.Data = FBase64::Encode(Bytes);
			Images.Add(MoveTemp(Image));
		}
		return Images;
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
		Questions->OnChoicePresented.AddSP(this, &FShuttlegentChatModel::HandleChoicePresented);
	}
	FShuttlegentMemoryStore::OnChanged().AddSP(this, &FShuttlegentChatModel::HandleMemoryChanged);

	// 模型连接增删、测通后，未打开会话的对话重新按设置显示模型，运行时在空闲时重启
	UShuttlegentSettings* Settings = GetMutableDefault<UShuttlegentSettings>();
	if (IsValid(Settings)) SettingsChangedHandle = Settings->OnSettingChanged().AddSP(this, &FShuttlegentChatModel::HandleSettingChanged);
	ConnectionTestedHandle = FShuttlegentConnectionTester::OnTested().AddSP(this, &FShuttlegentChatModel::HandleConnectionTested);

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
	PermissionHandlerCount = 0;
	ApplyPermissionHandler(false);

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
		Questions->OnChoicePresented.RemoveAll(this);
	}
	QuestionGate.Reset();
	FShuttlegentMemoryStore::OnChanged().RemoveAll(this);

	UShuttlegentSettings* Settings = GetMutableDefault<UShuttlegentSettings>();
	if (IsValid(Settings) && SettingsChangedHandle.IsValid()) Settings->OnSettingChanged().Remove(SettingsChangedHandle);
	SettingsChangedHandle.Reset();
	FShuttlegentConnectionTester::OnTested().Remove(ConnectionTestedHandle);
	ConnectionTestedHandle.Reset();

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
	NotifyAllStatus();
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::Prewarm()
{
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	if (!IsValid(Settings) || !Settings->bPrewarmRuntime || StartIssue != EShuttlegentStartIssue::None) return;

	UE_LOG(LogShuttlegent, Log, TEXT("Prewarming the runtime."));
	EnsureRuntime();
}

void FShuttlegentChatModel::RetryStart(FShuttlegentConversation& Conversation)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return;

	StartIssue = EShuttlegentStartIssue::None;
	StartIssueDetail.Reset();
	AddRunLog(Conversation, LOCTEXT("LogRetryStart", "重试启动运行时"), false);

	// 正在运行的运行时在启动时已读取密钥；待其退出后再次启动
	if (Pinned->GetState() != EShuttlegentAgentState::Stopped)
	{
		bRestartPending = true;
		Pinned->Stop();
		NotifyAllStatus();
		return;
	}

	bRuntimeLost = false;
	if (!Conversation.SessionId.IsEmpty()) RequestSession(Conversation);
	else EnsureRuntime();
	NotifyAllStatus();
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::RetryResume(FShuttlegentConversation& Conversation)
{
	if (!Conversation.bResumeFailed || Conversation.SessionId.IsEmpty()) return;

	AddRunLog(Conversation, LOCTEXT("LogRetryResume", "重试恢复会话"), false);
	RequestSession(Conversation);
	NotifyStatus(Conversation);
}

void FShuttlegentChatModel::AcknowledgeUnknownWrite(FShuttlegentConversation& Conversation, int32 EntryId)
{
	const TSharedPtr<FShuttlegentChatEntry>* Found = Conversation.Entries.FindByPredicate([EntryId](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry.IsValid() && Entry->Id == EntryId; });
	if (Found == nullptr || (*Found)->Kind != EShuttlegentChatEntryKind::WriteUnknown || (*Found)->bAnswered) return;

	(*Found)->bAnswered = true;
	Conversation.bNeedsVerify = Conversation.Entries.ContainsByPredicate([](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry->Kind == EShuttlegentChatEntryKind::WriteUnknown && !Entry->bAnswered; });
	AddRunLog(Conversation, LOCTEXT("LogWriteVerified", "已核对写入结果未知的对象"), false);
	NotifyContent(Conversation);
	NotifyStatus(Conversation);
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
	Conversation.Queue.Items.Insert(MakeQueued(Conversation, Conversation.PendingPrompt, TArray<FShuttlegentReference>()), 0);
	Conversation.Queue.bPaused = true;
	Conversation.PendingPrompt.Reset();
	AddRunLog(Conversation, LOCTEXT("LogPromptParked", "消息未发出，已放回队列首位"), true);
	NotifyQueue(Conversation);
}

EShuttlegentSubmitResult FShuttlegentChatModel::SubmitDraft(FShuttlegentConversation& Conversation, const FString& Text, const TArray<FShuttlegentReference>& References, bool bEnterPlanMode, const TArray<FShuttlegentPromptImage>& Images, TArray<FString>& OutInvalidPaths)
{
	OutInvalidPaths.Reset();
	for (const FShuttlegentReference& Reference : References)
	{
		if (!FShuttlegentReferenceFinder::Exists(Reference)) OutInvalidPaths.Add(Reference.Path);
	}
	const bool bBlocked = bRuntimeLost || StartIssue != EShuttlegentStartIssue::None || Conversation.bNeedsVerify;
	if ((Text.TrimStartAndEnd().IsEmpty() && Images.IsEmpty()) || bBlocked || !OutInvalidPaths.IsEmpty()) return EShuttlegentSubmitResult::Rejected;

	// 带图片的消息只在空闲时发送：队列与挂起提示都不保存图片
	FText ImageReason;
	if (!Images.IsEmpty() && !CanAttachImages(Conversation, ImageReason)) return EShuttlegentSubmitResult::Rejected;

	FText PlanReason;
	if (bEnterPlanMode && !CanSetPlanMode(Conversation, PlanReason)) return EShuttlegentSubmitResult::Rejected;

	// 用户没在卡片上作答而是直接发了消息：卡片不再等待
	SettleChoice(Conversation, ShuttlegentEditorWebField::ChoiceUnanswered, FShuttlegentChoiceAnswer());

	if (!IsBusy(Conversation))
	{
		const FString Body = Text.TrimStartAndEnd().IsEmpty() ? FString(ShuttlegentChatModelPrivate::ImageOnlyPrompt) : Text;
		const FString Prompt = FShuttlegentReferencePrompt::Compose(Body, References);
		if (bEnterPlanMode && !Conversation.bPlanMode) SubmitInPlanMode(Conversation.AsShared(), Prompt, Images);
		else SubmitPrompt(Conversation, Prompt, Images);
		return EShuttlegentSubmitResult::Sent;
	}

	// 回合进行中：计划模式在下一步生效，消息照常排队
	if (bEnterPlanMode) SetPlanMode(Conversation, true);

	Conversation.Queue.Items.Add(MakeQueued(Conversation, Text, References));
	AddRunLog(Conversation, LOCTEXT("LogQueued", "加入队列"), false);
	NotifyQueue(Conversation);
	return EShuttlegentSubmitResult::Queued;
}

bool FShuttlegentChatModel::CanAttachImages(const FShuttlegentConversation& Conversation, FText& OutReason) const
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
	ShuttlegentChatModelPrivate::ParseConfigChoices(Pinned->GetConfigOptions(Conversation.SessionId), ShuttlegentEditorConfigId::Model, Models, ModelValue);
	const bool* bModelImage = ModelImageInput.Find(ModelValue);
	if (bModelImage != nullptr && !*bModelImage)
	{
		const FShuttlegentConfigChoice* Model = Models.FindByPredicate([&ModelValue](const FShuttlegentConfigChoice& Choice) { return Choice.Value == ModelValue; });
		const FString ModelName = Model != nullptr ? Model->Name : ModelValue;
		OutReason = FText::Format(LOCTEXT("ImagesUnsupportedBySession", "这个对话使用的模型 {0} 不支持图片；在下方切换到支持图片的模型，或新建对话"), FText::FromString(ModelName));
		return false;
	}

	if (IsBusy(Conversation))
	{
		OutReason = LOCTEXT("ImagesBusy", "本轮结束后再发送带图片的消息");
		return false;
	}
	return true;
}

void FShuttlegentChatModel::RemoveQueued(FShuttlegentConversation& Conversation, int32 ItemId)
{
	if (Conversation.Queue.Items.RemoveAll([ItemId](const FShuttlegentQueuedPrompt& Item) { return Item.Id == ItemId; }) > 0) NotifyQueue(Conversation);
}

bool FShuttlegentChatModel::TakeQueued(FShuttlegentConversation& Conversation, int32 ItemId, const FString& DraftText, const TArray<FShuttlegentReference>& DraftReferences, FShuttlegentQueuedPrompt& OutItem)
{
	TArray<FShuttlegentQueuedPrompt>& Items = Conversation.Queue.Items;
	const int32 Index = Items.IndexOfByPredicate([ItemId](const FShuttlegentQueuedPrompt& Item) { return Item.Id == ItemId; });
	if (Index == INDEX_NONE) return false;

	OutItem = Items[Index];

	// 被替换的草稿换入该条目的位置，而不是丢失
	if (!DraftText.TrimStartAndEnd().IsEmpty() || !DraftReferences.IsEmpty()) Items[Index] = MakeQueued(Conversation, DraftText, DraftReferences);
	else Items.RemoveAt(Index);

	NotifyQueue(Conversation);
	return true;
}

void FShuttlegentChatModel::ResumeQueue(FShuttlegentConversation& Conversation)
{
	if (!Conversation.Queue.bPaused) return;

	Conversation.Queue.bPaused = false;
	AddRunLog(Conversation, LOCTEXT("LogQueueResumed", "继续队列"), false);
	NotifyQueue(Conversation);
	SendNextQueued(Conversation);
}

bool FShuttlegentChatModel::CanRetry(const FShuttlegentConversation& Conversation) const
{
	return Conversation.LastOutcome == EShuttlegentTurnOutcome::Failed && !Conversation.LastSentPrompt.IsEmpty() && !bRuntimeLost && StartIssue == EShuttlegentStartIssue::None && !Conversation.bNeedsVerify && !IsBusy(Conversation);
}

void FShuttlegentChatModel::Retry(FShuttlegentConversation& Conversation)
{
	if (!CanRetry(Conversation)) return;

	AddRunLog(Conversation, LOCTEXT("LogRetry", "重试查询"), false);
	SubmitPrompt(Conversation, Conversation.LastSentPrompt, Conversation.LastSentImages);
}

FShuttlegentQueuedPrompt FShuttlegentChatModel::MakeQueued(const FShuttlegentConversation& Conversation, const FString& Text, const TArray<FShuttlegentReference>& References)
{
	FShuttlegentQueuedPrompt Item;
	Item.Id = NextQueueId++;
	Item.Text = Text;
	Item.References = References;
	TArray<FShuttlegentConfigChoice> Choices;
	GetConfigChoices(Conversation, ShuttlegentEditorConfigId::Model, Choices, Item.ModelValue);
	GetConfigChoices(Conversation, ShuttlegentEditorConfigId::Reasoning, Choices, Item.ReasoningValue);
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

const FShuttlegentWriteRequest* FShuttlegentChatModel::GetPendingWrite(const FShuttlegentConversation& Conversation) const
{
	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (!Gate.IsValid()) return nullptr;

	// 其他对话发起的请求留在各自的对话里
	for (const FShuttlegentWriteRequest& Request : Gate->GetPending())
	{
		const TWeakPtr<FShuttlegentConversation>* Owner = WriteOwners.Find(Request.RequestId);
		if (Owner != nullptr && Owner->Pin().Get() == &Conversation) return &Request;
	}
	return nullptr;
}

void FShuttlegentChatModel::AnswerWrite(int32 RequestId, bool bApprove)
{
	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (Gate.IsValid()) Gate->Answer(RequestId, bApprove);
}

const FShuttlegentQuestion* FShuttlegentChatModel::GetPendingQuestion(const FShuttlegentConversation& Conversation) const
{
	const TSharedPtr<FShuttlegentQuestionGate> Questions = QuestionGate.Pin();
	if (!Questions.IsValid()) return nullptr;

	// 其他对话提出的问题留在各自的对话里
	for (const FShuttlegentQuestion& Question : Questions->GetPending())
	{
		if (FindQuestionOwner(Question).Get() == &Conversation) return &Question;
	}
	return nullptr;
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

void FShuttlegentChatModel::UndoWrite(FShuttlegentConversation& Conversation, int32 EntryId)
{
	const TSharedPtr<FShuttlegentChatEntry>* Found = Conversation.Entries.FindByPredicate([EntryId](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry.IsValid() && Entry->Id == EntryId; });
	if (Found == nullptr || (*Found)->Kind != EShuttlegentChatEntryKind::WriteResult) return;

	const TSharedPtr<FShuttlegentChatEntry> Entry = *Found;
	FText Reason;
	if (!CanUndoWrite(*Entry, Reason) || !FShuttlegentWriteGate::Undo(Entry->WriteResult.TransactionId))
	{
		AddNotice(Conversation, FText::Format(LOCTEXT("UndoFailed", "撤销失败：{0}"), Reason), true);
		return;
	}

	Entry->bUndone = true;
	AddRunLog(Conversation, LOCTEXT("LogUndoWrite", "撤销本次修改"), false);
	NotifyContent(Conversation);
}

void FShuttlegentChatModel::UndoMemory(FShuttlegentConversation& Conversation, int32 EntryId)
{
	const TSharedPtr<FShuttlegentChatEntry>* Found = Conversation.Entries.FindByPredicate([EntryId](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry.IsValid() && Entry->Id == EntryId; });
	if (Found == nullptr || (*Found)->Kind != EShuttlegentChatEntryKind::Memory || (*Found)->bUndone || (*Found)->Status == ShuttlegentEditorWebField::MemoryRead) return;

	const TSharedPtr<FShuttlegentChatEntry> Entry = *Found;
	FString Error;
	if (!FShuttlegentMemoryStore::Revert(Entry->MemoryChange, Error))
	{
		AddNotice(Conversation, FText::Format(LOCTEXT("UndoMemoryFailed", "撤销失败：{0}"), FText::FromString(Error)), true);
		return;
	}

	Entry->bUndone = true;
	AddRunLog(Conversation, LOCTEXT("LogUndoMemory", "撤销记忆修改"), false);
	NotifyContent(Conversation);
}

void FShuttlegentChatModel::HandleMemoryChanged(const FShuttlegentMemoryChange& Change, bool bByAgent)
{
	if (!bByAgent) return;

	// 记忆工具的调用不带会话：归属正在运行回合的对话，没有时归属获得焦点的对话
	const TSharedPtr<FShuttlegentConversation>* Running = Conversations.FindByPredicate([this](const TSharedPtr<FShuttlegentConversation>& Conversation) { return IsPrompting(*Conversation); });
	const TSharedPtr<FShuttlegentConversation> Owner = Running != nullptr ? *Running : GetFocused();
	if (!Owner.IsValid()) return;

	const bool bRemoved = Change.Kind == EShuttlegentMemoryChangeKind::Removed;
	const TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(*Owner, EShuttlegentChatEntryKind::Memory, bRemoved ? Change.Before.Text : Change.After.Text);
	Entry->MemoryChange = Change;
	if (Change.Kind == EShuttlegentMemoryChangeKind::Added) Entry->Status = ShuttlegentEditorWebField::MemoryAdded;
	else if (Change.Kind == EShuttlegentMemoryChangeKind::Updated) Entry->Status = ShuttlegentEditorWebField::MemoryUpdated;
	else Entry->Status = ShuttlegentEditorWebField::MemoryRemoved;
	NotifyStructure(*Owner);
}

bool FShuttlegentChatModel::CanEditHistory(const FShuttlegentConversation& Conversation, FText& OutReason) const
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || Conversation.SessionId.IsEmpty())
	{
		OutReason = LOCTEXT("HistoryNoSession", "这段对话还没有会话。");
		return false;
	}
	if (IsBusy(Conversation) || !Conversation.Queue.Items.IsEmpty())
	{
		OutReason = LOCTEXT("HistoryBusy", "本轮还在进行或有排队的消息，结束后再操作。");
		return false;
	}
	if (bRuntimeLost || GetState() != EShuttlegentAgentState::Ready || !Pinned->IsSessionOpen(Conversation.SessionId))
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

void FShuttlegentChatModel::BranchFrom(FShuttlegentConversation& Conversation, int32 EntryId)
{
	int32 Index = INDEX_NONE;
	int32 Ordinal = INDEX_NONE;
	if (!FindUserMessage(Conversation, EntryId, Index, Ordinal)) return;

	FText Reason;
	if (!CanEditHistory(Conversation, Reason))
	{
		AddNotice(Conversation, FText::Format(LOCTEXT("BranchBlocked", "不能分支：{0}"), Reason), true);
		return;
	}

	const FString SourceSessionId = Conversation.SessionId;
	const TWeakPtr<FShuttlegentChatModel> WeakThis = AsShared();
	AddRunLog(Conversation, LOCTEXT("LogBranch", "从这条消息分支"), false);
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

void FShuttlegentChatModel::RewindTo(FShuttlegentConversation& Conversation, int32 EntryId)
{
	int32 Index = INDEX_NONE;
	int32 Ordinal = INDEX_NONE;
	if (!FindUserMessage(Conversation, EntryId, Index, Ordinal)) return;

	FText Reason;
	if (!CanEditHistory(Conversation, Reason))
	{
		AddNotice(Conversation, FText::Format(LOCTEXT("RewindBlocked", "不能回到这里：{0}"), Reason), true);
		return;
	}

	// 用户在编辑器里另做的编辑夹在中间时，直接撤销会连带撤掉它们，因此整体拒绝
	const TArray<TSharedPtr<FShuttlegentChatEntry>> Writes = ShuttlegentChatModelPrivate::UndoableWritesAfter(Conversation, Index);
	if (!ShuttlegentChatModelPrivate::AreWritesOnUndoTop(Writes))
	{
		AddNotice(Conversation, LOCTEXT("RewindNotTop", "不能回到这里：这之后编辑器里还有其他编辑，直接撤销会连带撤掉它们。未做任何改动。"), true);
		return;
	}

	const FString SourceSessionId = Conversation.SessionId;
	// 放回输入框的是用户写的原文，不含随提问附带的引用块
	FString Draft;
	TArray<FString> ReferenceLines;
	FShuttlegentReferencePrompt::Split(Conversation.Entries[Index]->Text, Draft, ReferenceLines);
	AddRunLog(Conversation, LOCTEXT("LogRewind", "回到这条消息之前"), false);

	// 第一条消息之前没有对话可保留：撤销写入后开始新对话
	if (Ordinal == 0)
	{
		ShuttlegentChatModelPrivate::UndoWrites(Writes);
		OnDraftRestored.Broadcast(SourceSessionId, Draft);
		ArchiveSession(SourceSessionId);
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
		This->OnDraftRestored.Broadcast(NewSessionId, Draft);
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

	// 显示源会话的视图改为显示新会话；回退后原会话归档保留，可从“已归档”恢复
	OnForkAdopted.Broadcast(SourceSessionId, NewSessionId);
	if (bRewind) ArchiveSession(SourceSessionId);
}

void FShuttlegentChatModel::Cancel(FShuttlegentConversation& Conversation)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || !IsPrompting(Conversation) || Conversation.bStopping) return;

	Conversation.bStopping = true;
	Pinned->Cancel(Conversation.SessionId);
	AddRunLog(Conversation, LOCTEXT("LogCancel", "发送停止请求"), true);
	NotifyStatus(Conversation);
}

TSharedRef<FShuttlegentConversation> FShuttlegentChatModel::CreateConversation()
{
	const TSharedRef<FShuttlegentConversation> Created = MakeShared<FShuttlegentConversation>();
	Created->WriteMode = ShuttlegentChatModelPrivate::DefaultWriteMode();
	SetAgent(*Created, ShuttlegentChatModelPrivate::DefaultAgent());
	Conversations.Add(Created);
	return Created;
}

TSharedPtr<FShuttlegentConversation> FShuttlegentChatModel::HoldConversation(const FString& SessionId, bool bDeferResume)
{
	if (!Service.IsValid() || SessionId.IsEmpty()) return nullptr;

	// 已持有的对话按原样返回：仍在运行，或处于空闲并带有其条目与运行日志
	const TSharedPtr<FShuttlegentConversation> Held = FindConversation(SessionId);
	if (Held.IsValid()) return Held;

	// 已有会话的 Agent 与写入方式以会话索引的记录为准，没有记录的旧会话属于默认 Agent、按偏好设置的默认写入方式
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	const FShuttlegentSessionRecord* Record = Pinned.IsValid() ? Pinned->GetSessionStore().FindRecord(SessionId) : nullptr;
	const TSharedRef<FShuttlegentConversation> Opened = MakeShared<FShuttlegentConversation>();
	Opened->SessionId = SessionId;
	Opened->WriteMode = Record != nullptr && Record->WriteMode.IsSet() ? Record->WriteMode.GetValue() : ShuttlegentChatModelPrivate::DefaultWriteMode();
	Opened->AgentId = Record != nullptr && !Record->AgentId.IsEmpty() ? Record->AgentId : FString(ShuttlegentAgentConstant::DefaultAgentId);
	Conversations.Add(Opened);
	ReplayTranscript(*Opened);

	// 上次关闭编辑器时没发出的排队提示：放回队列并暂停，由用户点“继续”，重启后不自动发送
	for (FShuttlegentQueuedPrompt& Item : Drafts.GetQueue(SessionId))
	{
		Item.Id = NextQueueId++;
		Opened->Queue.Items.Add(MoveTemp(Item));
	}
	if (!Opened->Queue.Items.IsEmpty())
	{
		Opened->Queue.bPaused = true;
		AddRunLog(*Opened, FText::Format(LOCTEXT("LogQueueRestored", "放回上次未发出的 {0} 条排队消息，队列已暂停"), FText::AsNumber(Opened->Queue.Items.Num())), false);
		UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent session %s: %d queued prompt(s) restored, queue paused."), *SessionId, Opened->Queue.Items.Num());
	}

	if (bDeferResume)
	{
		Opened->bResumeDeferred = true;
		AddRunLog(*Opened, LOCTEXT("LogOpenSessionDeferred", "打开会话记录，首次显示时恢复运行时上下文"), false);
		return Opened;
	}

	AddRunLog(*Opened, LOCTEXT("LogOpenSession", "打开会话记录，恢复运行时上下文"), false);
	RequestSession(*Opened);
	return Opened;
}

void FShuttlegentChatModel::ResumeDeferred(FShuttlegentConversation& Conversation)
{
	if (!Conversation.bResumeDeferred) return;

	UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent session %s: deferred resume starts."), *Conversation.SessionId);
	RequestSession(Conversation);
	NotifyStatus(Conversation);
}

bool FShuttlegentChatModel::IsSessionAvailable(const FString& SessionId) const
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || SessionId.IsEmpty()) return false;

	const FShuttlegentSessionRecord* Record = Pinned->GetSessionStore().FindRecord(SessionId);
	return Record != nullptr && !Record->bArchived;
}

bool FShuttlegentChatModel::HasSessionRecord(const FString& SessionId) const
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	return Pinned.IsValid() && !SessionId.IsEmpty() && Pinned->GetSessionStore().FindRecord(SessionId) != nullptr;
}

TArray<FString> FShuttlegentChatModel::GetBusyTitles() const
{
	TArray<FString> Titles;
	for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
	{
		// 只算真正会丢失的回合：正在回答，或已交出、等会话就绪才发的提问；运行时启动中本身不算
		if (!IsPrompting(*Conversation) && Conversation->PendingPrompt.IsEmpty()) continue;

		const FString Title = GetTitle(*Conversation);
		Titles.Add(Title.IsEmpty() ? LOCTEXT("BusyUntitled", "新对话").ToString() : Title);
	}
	return Titles;
}

void FShuttlegentChatModel::SettleBeforeExit()
{
	for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
	{
		if (!Conversation->PendingPlanReview.ReviewId.IsEmpty()) SettlePlanReview(*Conversation, ShuttlegentEditorWebField::PlanCancelled, FString());
		SettleChoice(*Conversation, ShuttlegentEditorWebField::ChoiceUnanswered, FShuttlegentChoiceAnswer());
		RejectPendingPermissions(*Conversation);
	}
	UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent: the editor is closing during a turn; pending plans, choices and permission requests were settled."));
}

void FShuttlegentChatModel::ReleaseConversation(FShuttlegentConversation& Conversation)
{
	if (CanDrop(Conversation)) DropConversation(Conversation);
}

void FShuttlegentChatModel::SetDisplayed(FShuttlegentConversation& Conversation, bool bShown, bool bVisible)
{
	Conversation.DisplayCount = FMath::Max(0, Conversation.DisplayCount + (bShown ? 1 : -1));
	if (bVisible) Conversation.VisibleCount = FMath::Max(0, Conversation.VisibleCount + (bShown ? 1 : -1));

	// 切到的对话此刻就在用户眼前；会话列表上“在别的页签里”的标记随显示情况变化
	MarkSeen(Conversation);
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::SetFocused(FShuttlegentConversation& Conversation)
{
	Focused = Conversation.AsShared();
}

TSharedPtr<FShuttlegentConversation> FShuttlegentChatModel::GetFocused() const
{
	const TSharedPtr<FShuttlegentConversation> Pinned = Focused.Pin();
	if (Pinned.IsValid()) return Pinned;

	return Conversations.IsEmpty() ? nullptr : Conversations[0];
}

void FShuttlegentChatModel::Reconnect(FShuttlegentConversation& Conversation)
{
	bRuntimeLost = false;
	AddRunLog(Conversation, LOCTEXT("LogReconnect", "重新连接运行时"), false);

	// 其他已持有的对话在下次发送提示时恢复其会话
	if (!Conversation.SessionId.IsEmpty()) RequestSession(Conversation);
	else EnsureRuntime();
	NotifyAllStatus();
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::SetPermissionHandlerAttached(bool bAttached)
{
	// 每个打开的面板挂接一次；还有面板开着时仍由面板答复
	PermissionHandlerCount = FMath::Max(0, PermissionHandlerCount + (bAttached ? 1 : -1));
	ApplyPermissionHandler(PermissionHandlerCount > 0);
}

void FShuttlegentChatModel::ApplyPermissionHandler(bool bAttached)
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

bool FShuttlegentChatModel::IsAgentLocked(const FShuttlegentConversation& Conversation) const
{
	return ShuttlegentChatModelPrivate::IsAgentLocked(Conversation);
}

void FShuttlegentChatModel::SetAgent(FShuttlegentConversation& Conversation, const FString& AgentId)
{
	if (ShuttlegentChatModelPrivate::IsAgentLocked(Conversation) || !ShuttlegentChatModelPrivate::EnabledAgents().Contains(AgentId)) return;

	// 带出 Agent 的默认模型；Agent 没有指定时新会话用运行时的默认模型，撤掉等待中的模型选择
	Conversation.AgentId = AgentId;
	const FString ModelValue = ShuttlegentChatModelPrivate::AgentModelValue(AgentId);
	if (ModelValue.IsEmpty()) Conversation.PendingConfig.Remove(ShuttlegentEditorConfigId::Model);
	else Conversation.PendingConfig.Add(ShuttlegentEditorConfigId::Model, ModelValue);
}

bool FShuttlegentChatModel::GetConfigChoices(const FShuttlegentConversation& Conversation, const FString& ConfigId, TArray<FShuttlegentConfigChoice>& OutChoices, FString& OutCurrentValue) const
{
	OutChoices.Reset();
	OutCurrentValue.Reset();

	// Agent 由面板自己管理：已启用的 Agent 与该对话的 Agent
	if (ConfigId == ShuttlegentEditorConfigId::Agent)
	{
		for (const FString& AgentId : ShuttlegentChatModelPrivate::EnabledAgents())
		{
			OutChoices.Add(FShuttlegentConfigChoice { AgentId, FShuttlegentSettingsChoices::GetAgentLabel(AgentId).ToString() });
		}
		OutCurrentValue = Conversation.AgentId.IsEmpty() ? FString(ShuttlegentAgentConstant::DefaultAgentId) : Conversation.AgentId;
		return true;
	}

	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid()) return false;

	const bool bOpen = Pinned->IsSessionOpen(Conversation.SessionId);
	const FString* Pending = Conversation.PendingConfig.Find(ConfigId);

	// 没有已打开会话的对话：模型按个人设置列出（运行时上报的可能已过时），当前值为为该对话选择的模型，否则为默认模型
	if (!bOpen && ConfigId == ShuttlegentEditorConfigId::Model)
	{
		ShuttlegentChatModelPrivate::BuildModelChoices(OutChoices, OutCurrentValue);
		FString Route;
		FString ModelId;
		if (Pending != nullptr && ShuttlegentChatModelPrivate::ParseModelValue(*Pending, Route, ModelId))
		{
			const FString Matched = ShuttlegentChatModelPrivate::FindModelChoice(OutChoices, Route, ModelId);
			if (!Matched.IsEmpty()) OutCurrentValue = Matched;
		}
		return !OutChoices.IsEmpty();
	}

	// 其余选项：没有已打开会话的对话显示最近任一会话上报的选项，以及为该对话选择的值
	const bool bFound = ShuttlegentChatModelPrivate::ParseConfigChoices(bOpen ? Pinned->GetConfigOptions(Conversation.SessionId) : LastConfigOptions, ConfigId, OutChoices, OutCurrentValue);

	if (bFound && Pending != nullptr) OutCurrentValue = *Pending;
	return bFound;
}

void FShuttlegentChatModel::SetConfigValue(FShuttlegentConversation& Conversation, const FString& ConfigId, const FString& Value)
{
	if (ConfigId == ShuttlegentEditorConfigId::Agent)
	{
		SetAgent(Conversation, Value);
		NotifyStatus(Conversation);
		return;
	}

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

EShuttlegentAttention FShuttlegentChatModel::GetAttentionOutside(const FShuttlegentConversation& Shown) const
{
	EShuttlegentAttention Overall = EShuttlegentAttention::None;
	for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
	{
		// 别的页签显示的对话由那个页签自己提醒
		if (Conversation.Get() != &Shown && Conversation->DisplayCount > 0) continue;

		const EShuttlegentAttention Attention = GetAttention(*Conversation);
		if (Attention == EShuttlegentAttention::Input) return Attention;
		if (Attention == EShuttlegentAttention::Done) Overall = Attention;
	}
	return Overall;
}

int32 FShuttlegentChatModel::GetDisplayCount(const FString& SessionId) const
{
	const TSharedPtr<FShuttlegentConversation> Conversation = FindConversation(SessionId);
	return Conversation.IsValid() ? Conversation->DisplayCount : 0;
}

bool FShuttlegentChatModel::NeedsInput(const FShuttlegentConversation& Conversation) const
{
	const EShuttlegentChatStatus Status = GetStatus(Conversation);
	if (Status == EShuttlegentChatStatus::AwaitingWriteConfirmation || Status == EShuttlegentChatStatus::AwaitingPermission || Status == EShuttlegentChatStatus::AwaitingPlanReview) return true;

	// 回合结束后仍在等待的选择卡同样等用户作答
	if (Conversation.ChoiceEntryId != 0 && !IsPrompting(Conversation)) return true;

	const TSharedPtr<FShuttlegentQuestionGate> Questions = QuestionGate.Pin();
	if (!Questions.IsValid() || !IsPrompting(Conversation)) return false;

	return Questions->GetPending().ContainsByPredicate([this, &Conversation](const FShuttlegentQuestion& Question) { return FindQuestionOwner(Question).Get() == &Conversation; });
}

EShuttlegentAttention FShuttlegentChatModel::GetAttention(const FShuttlegentConversation& Conversation) const
{
	if (NeedsInput(Conversation)) return EShuttlegentAttention::Input;
	return Conversation.bUnseenFinish ? EShuttlegentAttention::Done : EShuttlegentAttention::None;
}

bool FShuttlegentChatModel::MarkSeen(FShuttlegentConversation& Conversation)
{
	if (Conversation.VisibleCount == 0 || !Conversation.bUnseenFinish) return false;

	Conversation.bUnseenFinish = false;
	return true;
}

TSharedPtr<FShuttlegentChatEntry> FShuttlegentChatModel::FindToolCallOf(const FShuttlegentConversation& Conversation, const TSharedPtr<FShuttlegentChatEntry>& Permission) const
{
	if (!Permission.IsValid() || Permission->ToolCallId.IsEmpty()) return nullptr;

	return FindToolCall(Conversation, Permission->ToolCallId);
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

FString FShuttlegentChatModel::GetTitle(const FShuttlegentConversation& Conversation) const
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || Conversation.SessionId.IsEmpty()) return FString();

	const FShuttlegentSessionRecord* Record = Pinned->GetSessionStore().FindRecord(Conversation.SessionId);
	return Record != nullptr ? Record->Title : FString();
}

bool FShuttlegentChatModel::CanArchive(const FString& SessionId) const
{
	if (SessionId.IsEmpty()) return false;

	const TSharedPtr<FShuttlegentConversation> Conversation = FindConversation(SessionId);
	if (!Conversation.IsValid()) return true;

	const bool bWriteOpen = HasPendingWrite(*Conversation) || IsWritingFor(*Conversation);
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

	// 显示它的视图改为显示新对话；已归档的会话不在后台继续运行，其队列随之一并丢弃
	OnSessionClosed.Broadcast(SessionId);
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

	Drafts.Remove(SessionId);
	OnSessionClosed.Broadcast(SessionId);
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
	OnStatusChanged.Broadcast(*Conversation);
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

bool FShuttlegentChatModel::CanSetPlanMode(const FShuttlegentConversation& Conversation, FText& OutReason) const
{
	if (Conversation.bResuming)
	{
		OutReason = LOCTEXT("PlanResuming", "会话正在恢复。");
		return false;
	}
	if (!Conversation.PendingPlanReview.ReviewId.IsEmpty())
	{
		OutReason = LOCTEXT("PlanReviewing", "先审批 Agent 交出的计划。");
		return false;
	}

	// 会话未创建时记下选择，就绪后应用；已创建的会话需要运行时的 session control 插件
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	const bool bOpen = Pinned.IsValid() && !Conversation.SessionId.IsEmpty() && Pinned->IsSessionOpen(Conversation.SessionId);
	if (bOpen && !FShuttlegentMcpServerModule::Get().CanForkSessions())
	{
		OutReason = LOCTEXT("PlanNoControl", "运行时的会话控制插件不可用。");
		return false;
	}
	if (!Conversation.SessionId.IsEmpty() && !bOpen)
	{
		OutReason = LOCTEXT("PlanNotOpen", "会话尚未在运行时中就绪。");
		return false;
	}

	OutReason = FText::GetEmpty();
	return true;
}

void FShuttlegentChatModel::SetPlanMode(FShuttlegentConversation& Conversation, bool bActive)
{
	FText Reason;
	if (!CanSetPlanMode(Conversation, Reason) || Conversation.bPlanMode == bActive) return;

	ApplyPlanMode(Conversation.AsShared(), bActive, nullptr);
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

void FShuttlegentChatModel::AnswerPlanReview(FShuttlegentConversation& Target, bool bApprove, const FString& Feedback, bool bAcceptEdits)
{
	const TSharedPtr<FShuttlegentConversation> Conversation = Target.AsShared();
	const FString ReviewId = Conversation->PendingPlanReview.ReviewId;
	if (ReviewId.IsEmpty()) return;

	// 批准时选定执行阶段的写入确认方式，在 Agent 开始写入之前生效
	if (bApprove)
	{
		Conversation->WriteMode = bAcceptEdits ? EShuttlegentWriteMode::AcceptEdits : EShuttlegentWriteMode::Manual;
		RecordWriteMode(*Conversation);
	}

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

void FShuttlegentChatModel::HandleChoicePresented(const FShuttlegentChoiceRequest& Request)
{
	// 带对话标识的卡片归属交出它的对话；未带时归属正在运行回合的对话，获得焦点的对话优先
	TSharedPtr<FShuttlegentConversation> Owner = FindConversationByKey(Request.ConversationKey);
	if (!Owner.IsValid())
	{
		Owner = GetFocused();
		if (!Owner.IsValid() || !IsPrompting(*Owner))
		{
			const TSharedPtr<FShuttlegentConversation>* Prompting = Conversations.FindByPredicate([this](const TSharedPtr<FShuttlegentConversation>& Held) { return IsPrompting(*Held); });
			if (Prompting != nullptr) Owner = *Prompting;
		}
	}
	if (!Owner.IsValid()) return;

	// 同一对话只保留一张等待中的卡片
	SettleChoice(*Owner, ShuttlegentEditorWebField::ChoiceUnanswered, FShuttlegentChoiceAnswer());

	const TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(*Owner, EShuttlegentChatEntryKind::Choice, Request.Question);
	Entry->Choice = Request;
	Entry->Status = ShuttlegentEditorWebField::ChoicePending;
	Owner->ChoiceEntryId = Entry->Id;
	RecordChoice(*Owner, *Entry);

	AddRunLog(*Owner, FText::Format(LOCTEXT("LogChoicePresented", "Agent 交出选择卡：{0}"), FText::FromString(Request.Question)), false);
	NotifyStructure(*Owner);
	NotifyStatus(*Owner);
	OnSessionsChanged.Broadcast();
}

bool FShuttlegentChatModel::SubmitChoice(FShuttlegentConversation& Conversation, int32 EntryId, const FString& AnswerJson, FText& OutReason)
{
	TSharedPtr<FShuttlegentChatEntry>* Found = Conversation.Entries.FindByPredicate([EntryId](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry->Id == EntryId; });
	if (Found == nullptr || Conversation.ChoiceEntryId != EntryId)
	{
		OutReason = LOCTEXT("ChoiceClosed", "这张选择卡已不在等待作答。");
		return false;
	}

	FShuttlegentChoiceAnswer Answer;
	if (!FShuttlegentChoiceJson::ParseAnswer(AnswerJson, (*Found)->Choice, Answer) || (Answer.Selected.IsEmpty() && Answer.ItemNotes.IsEmpty() && Answer.OverallNote.IsEmpty()))
	{
		OutReason = LOCTEXT("ChoiceEmpty", "先选择一项或写下意见再提交。");
		return false;
	}

	// 与 SubmitDraft 的拒绝条件一致；先查，免得卡片已标为提交而消息没有发出
	if (bRuntimeLost || StartIssue != EShuttlegentStartIssue::None || Conversation.bNeedsVerify)
	{
		OutReason = LOCTEXT("ChoiceBlocked", "现在不能发送消息：先处理上方的提示。");
		return false;
	}

	const FString Message = ShuttlegentChatModelPrivate::ComposeChoiceMessage((*Found)->Choice, Answer);
	SettleChoice(Conversation, ShuttlegentEditorWebField::ChoiceAnswered, Answer);

	TArray<FString> InvalidPaths;
	SubmitDraft(Conversation, Message, TArray<FShuttlegentReference>(), false, TArray<FShuttlegentPromptImage>(), InvalidPaths);
	return true;
}

void FShuttlegentChatModel::SettleChoice(FShuttlegentConversation& Conversation, const TCHAR* Status, const FShuttlegentChoiceAnswer& Answer)
{
	if (Conversation.ChoiceEntryId == 0) return;

	const int32 EntryId = Conversation.ChoiceEntryId;
	Conversation.ChoiceEntryId = 0;
	TSharedPtr<FShuttlegentChatEntry>* Found = Conversation.Entries.FindByPredicate([EntryId](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry->Id == EntryId; });
	if (Found == nullptr) return;

	(*Found)->Status = Status;
	(*Found)->ChoiceAnswer = Answer;
	RecordChoice(Conversation, **Found);

	AddRunLog(Conversation, FText::Format(LOCTEXT("LogChoiceSettled", "选择卡：{0}"), FText::FromString(Status)), false);
	NotifyStructure(Conversation);
	NotifyStatus(Conversation);
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::RecordChoice(const FShuttlegentConversation& Conversation, const FShuttlegentChatEntry& Entry) const
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || Conversation.SessionId.IsEmpty()) return;

	Pinned->GetSessionStore().AppendTranscript(Conversation.SessionId, ShuttlegentChatModelPrivate::TranscriptChoice, FShuttlegentChoiceJson::ToJson(Entry.Choice, Entry.ChoiceAnswer, Entry.Status));
}

bool FShuttlegentChatModel::IsPlanTurnRunning() const
{
	return Conversations.ContainsByPredicate([this](const TSharedPtr<FShuttlegentConversation>& Conversation) { return Conversation->bPlanMode && IsBusy(*Conversation); });
}

EShuttlegentWritePolicy FShuttlegentChatModel::GetWritePolicy(const FShuttlegentWriteRequest& Request) const
{
	// 带对话标识的请求按发起它的对话判断；未带时按全部正在运行的对话判断
	const TSharedPtr<FShuttlegentConversation> Owner = FindConversationByKey(Request.ConversationKey);
	const bool bPlanTurn = Owner.IsValid() ? Owner->bPlanMode && IsBusy(*Owner) : IsPlanTurnRunning();
	if (bPlanTurn) return EShuttlegentWritePolicy::Block;
	if (Request.bDestructive) return EShuttlegentWritePolicy::Ask;

	TOptional<EShuttlegentWriteMode> Mode;
	if (Owner.IsValid())
	{
		Mode = Owner->WriteMode;
	}
	else
	{
		// 取正在运行的对话中最严格的模式（枚举按从严到宽排列）
		for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
		{
			if (!IsBusy(*Conversation)) continue;

			if (!Mode.IsSet() || static_cast<uint8>(Conversation->WriteMode) < static_cast<uint8>(Mode.GetValue())) Mode = Conversation->WriteMode;
		}
	}
	if (!Mode.IsSet()) return EShuttlegentWritePolicy::Ask;

	if (Mode.GetValue() == EShuttlegentWriteMode::Auto) return EShuttlegentWritePolicy::Approve;
	if (Mode.GetValue() == EShuttlegentWriteMode::AcceptEdits && Request.bUndoable) return EShuttlegentWritePolicy::Approve;
	return EShuttlegentWritePolicy::Ask;
}

void FShuttlegentChatModel::SetWriteMode(FShuttlegentConversation& Conversation, EShuttlegentWriteMode Mode)
{
	FText Reason;
	if (Conversation.bPlanMode && CanSetPlanMode(Conversation, Reason)) ApplyPlanMode(Conversation.AsShared(), false, nullptr);

	if (Conversation.WriteMode == Mode) return;

	Conversation.WriteMode = Mode;
	RecordWriteMode(Conversation);
	NotifyStatus(Conversation);
}

void FShuttlegentChatModel::RequestTitle(const FShuttlegentConversation& Conversation)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || Conversation.SessionId.IsEmpty()) return;

	// 只在第一轮结束后取一次；用户改过名或已生成过的不再覆盖
	const FShuttlegentSessionRecord* Record = Pinned->GetSessionStore().FindRecord(Conversation.SessionId);
	const int32 UserMessages = Conversation.Entries.FilterByPredicate([](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry->Kind == EShuttlegentChatEntryKind::User; }).Num();
	if (Record == nullptr || Record->bTitleFinal || UserMessages != 1) return;

	const TSharedPtr<FShuttlegentChatEntry>* User = Conversation.Entries.FindByPredicate([](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry->Kind == EShuttlegentChatEntryKind::User; });
	const TSharedPtr<FShuttlegentChatEntry>* Answer = Conversation.Entries.FindByPredicate([](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry->Kind == EShuttlegentChatEntryKind::Assistant && !Entry->Text.TrimStartAndEnd().IsEmpty(); });
	if (User == nullptr || Answer == nullptr) return;

	// 给模型的是用户写的原文，不含面板附带的引用块
	FString UserText;
	TArray<FString> ReferenceLines;
	FShuttlegentReferencePrompt::Split((*User)->Text, UserText, ReferenceLines);
	TArray<FShuttlegentConfigChoice> Choices;
	FString ModelValue;
	GetConfigChoices(Conversation, ShuttlegentEditorConfigId::Model, Choices, ModelValue);

	const TWeakPtr<FShuttlegentChatModel> WeakThis = AsShared();
	const FString SessionId = Conversation.SessionId;
	FShuttlegentSessionTitler::Generate(ModelValue, UserText, (*Answer)->Text, FShuttlegentSessionTitler::FOnTitled::CreateLambda([WeakThis, SessionId](const FString& Title)
	{
		const TSharedPtr<FShuttlegentChatModel> This = WeakThis.Pin();
		const TSharedPtr<FShuttlegentAgentService> AgentService = This.IsValid() ? This->Service.Pin() : nullptr;
		if (!AgentService.IsValid() || Title.IsEmpty()) return;

		// 等待期间用户可能已改名
		const FShuttlegentSessionRecord* Latest = AgentService->GetSessionStore().FindRecord(SessionId);
		if (Latest == nullptr || Latest->bTitleFinal) return;

		FShuttlegentSessionRecord Updated = *Latest;
		Updated.Title = Title;
		Updated.bTitleFinal = true;
		AgentService->GetSessionStore().UpsertRecord(Updated);
		This->NotifyAllStatus();
		This->OnSessionsChanged.Broadcast();
	}));
}

void FShuttlegentChatModel::RecordWriteMode(const FShuttlegentConversation& Conversation)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Pinned.IsValid() || Conversation.SessionId.IsEmpty()) return;

	// 会话尚未建立时由会话就绪时补记
	FShuttlegentSessionStore& Store = Pinned->GetSessionStore();
	const FShuttlegentSessionRecord* Record = Store.FindRecord(Conversation.SessionId);
	if (Record == nullptr || Record->WriteMode == Conversation.WriteMode) return;

	FShuttlegentSessionRecord Updated = *Record;
	Updated.WriteMode = Conversation.WriteMode;
	Store.UpsertRecord(Updated);
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

bool FShuttlegentChatModel::CanCompact(const FShuttlegentConversation& Conversation, FText& OutReason) const
{
	if (Conversation.bCompacting)
	{
		OutReason = LOCTEXT("CompactRunning", "正在压缩。");
		return false;
	}
	if (!CanEditHistory(Conversation, OutReason)) return false;

	OutReason = FText::GetEmpty();
	return true;
}

void FShuttlegentChatModel::CompactContext(FShuttlegentConversation& Target)
{
	FText Reason;
	if (!CanCompact(Target, Reason)) return;

	const TSharedPtr<FShuttlegentConversation> Conversation = Target.AsShared();
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
	if (HasPendingWrite(Conversation)) return EShuttlegentChatStatus::AwaitingWriteConfirmation;
	if (IsWritingFor(Conversation)) return EShuttlegentChatStatus::Writing;
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

TSharedPtr<FShuttlegentConversation> FShuttlegentChatModel::FindConversationByKey(const FString& ConversationKey) const
{
	if (ConversationKey.IsEmpty()) return nullptr;

	const TSharedPtr<FShuttlegentConversation>* Found = Conversations.FindByPredicate([&ConversationKey](const TSharedPtr<FShuttlegentConversation>& Conversation) { return Conversation->ConversationKey == ConversationKey; });
	return Found != nullptr ? *Found : nullptr;
}

bool FShuttlegentChatModel::HasPendingWrite(const FShuttlegentConversation& Conversation) const
{
	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (!Gate.IsValid()) return false;

	return Gate->GetPending().ContainsByPredicate([this, &Conversation](const FShuttlegentWriteRequest& Request)
	{
		const TWeakPtr<FShuttlegentConversation>* Owner = WriteOwners.Find(Request.RequestId);
		return Owner != nullptr && Owner->Pin().Get() == &Conversation;
	});
}

bool FShuttlegentChatModel::IsWritingFor(const FShuttlegentConversation& Conversation) const
{
	if (!bWriting) return false;

	const TSharedPtr<FShuttlegentConversation> Owner = ApprovedWriteOwner.Pin();
	return Owner.IsValid() ? Owner.Get() == &Conversation : GetFocused().Get() == &Conversation;
}

TSharedPtr<FShuttlegentConversation> FShuttlegentChatModel::FindQuestionOwner(const FShuttlegentQuestion& Question) const
{
	const TSharedPtr<FShuttlegentConversation> Owner = FindConversationByKey(Question.ConversationKey);
	return Owner.IsValid() ? Owner : GetFocused();
}

bool FShuttlegentChatModel::CanDrop(const FShuttlegentConversation& Conversation) const
{
	const bool bWritingHere = bWriting && ApprovedWriteOwner.Pin().Get() == &Conversation;
	return Conversation.DisplayCount == 0 && !IsBusy(Conversation) && Conversation.Queue.Items.IsEmpty() && Conversation.PendingPermissions.IsEmpty() && !HasPendingWrite(Conversation) && !bWritingHere;
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

void FShuttlegentChatModel::NotifyAllStatus()
{
	for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
	{
		OnStatusChanged.Broadcast(*Conversation);
	}
}

void FShuttlegentChatModel::NotifyStructure(const FShuttlegentConversation& Conversation)
{
	OnStructureChanged.Broadcast(Conversation);
}

void FShuttlegentChatModel::NotifyContent(const FShuttlegentConversation& Conversation)
{
	OnContentChanged.Broadcast(Conversation);
}

void FShuttlegentChatModel::NotifyStatus(const FShuttlegentConversation& Conversation)
{
	OnStatusChanged.Broadcast(Conversation);

	// 会话列表显示每个已持有对话的状态
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::NotifyQueue(const FShuttlegentConversation& Conversation)
{
	// 队列每次变化即写盘，重启后放回；尚无会话 id 的新对话等有了 id 后的下一次变化再写
	if (!Conversation.SessionId.IsEmpty()) Drafts.SetQueue(Conversation.SessionId, Conversation.Queue.Items);

	OnQueueChanged.Broadcast(Conversation);

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
	OnRunLogChanged.Broadcast(Conversation);
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

	// 任何途径请求会话（首次显示、发送、重试）都结束暂缓
	Conversation.bResumeDeferred = false;
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
	if (!bResume)
	{
		StartNewSession(Conversation);
		return;
	}

	FString Error;
	if (Pinned->ResumeSession(Conversation.SessionId, Conversation.ConversationKey, Error)) return;

	ParkPendingPrompt(Conversation);
	Conversation.bResumeFailed = true;
	AddNotice(Conversation, FText::Format(LOCTEXT("ResumeFailed", "恢复会话失败：{0}"), FText::FromString(Error)), true);
}

void FShuttlegentChatModel::StartNewSession(FShuttlegentConversation& Conversation)
{
	// 指定 Agent 与 session/new 之间不能夹进另一个新会话，否则两个会话可能互换 Agent
	const TSharedPtr<FShuttlegentConversation> Handoff = AgentHandoff.Pin();
	if (Handoff.IsValid() && Handoff.Get() != &Conversation)
	{
		WaitingForHandoff.AddUnique(Conversation.AsShared());
		return;
	}

	const FString AgentId = Conversation.AgentId.IsEmpty() ? FString(ShuttlegentAgentConstant::DefaultAgentId) : Conversation.AgentId;
	FShuttlegentMcpServerModule& Mcp = FShuttlegentMcpServerModule::Get();
	if (!Mcp.CanForkSessions())
	{
		// 控制口尚未报来：分发插件没收到指定时用默认 Agent，其他 Agent 不能静默改用它
		if (AgentId == ShuttlegentAgentConstant::DefaultAgentId)
		{
			SendNewSession(Conversation);
			return;
		}
		ParkPendingPrompt(Conversation);
		AddNotice(Conversation, FText::Format(LOCTEXT("AgentControlMissing", "运行时控制口未就绪，无法为新会话指定 Agent“{0}”；稍后重新发送"), FShuttlegentSettingsChoices::GetAgentLabel(AgentId)), true);
		return;
	}

	AgentHandoff = Conversation.AsShared();
	const TWeakPtr<FShuttlegentChatModel> WeakThis = AsShared();
	const TWeakPtr<FShuttlegentConversation> WeakConversation = Conversation.AsShared();
	Mcp.SetNextAgent(AgentId, [WeakThis, WeakConversation, AgentId](const FString& Error)
	{
		const TSharedPtr<FShuttlegentChatModel> This = WeakThis.Pin();
		const TSharedPtr<FShuttlegentConversation> Held = WeakConversation.Pin();
		if (!This.IsValid()) return;
		if (!Held.IsValid())
		{
			This->ReleaseAgentHandoff();
			return;
		}

		if (!Error.IsEmpty())
		{
			This->ParkPendingPrompt(*Held);
			This->AddNotice(*Held, FText::Format(LOCTEXT("AgentHandoffFailed", "无法为新会话指定 Agent“{0}”：{1}"), FShuttlegentSettingsChoices::GetAgentLabel(AgentId), FText::FromString(Error)), true);
			This->NotifyStatus(*Held);
			This->ReleaseAgentHandoff();
			return;
		}

		// 交接在 session/new 答复（会话就绪或出错）时结束
		This->SendNewSession(*Held);
	});
}

void FShuttlegentChatModel::SendNewSession(FShuttlegentConversation& Conversation)
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	FString Error;
	if (Pinned.IsValid() && Pinned->NewSession(Conversation.ConversationKey, Error))
	{
		AwaitingNewSession.Add(Conversation.AsShared());
		return;
	}

	ParkPendingPrompt(Conversation);
	AddNotice(Conversation, FText::Format(LOCTEXT("NewSessionFailed", "新建会话失败：{0}"), FText::FromString(Error)), true);
	if (AgentHandoff.Pin().Get() == &Conversation) ReleaseAgentHandoff();
}

void FShuttlegentChatModel::ReleaseAgentHandoff()
{
	AgentHandoff.Reset();
	while (!WaitingForHandoff.IsEmpty())
	{
		const TSharedPtr<FShuttlegentConversation> Next = WaitingForHandoff[0].Pin();
		WaitingForHandoff.RemoveAt(0);
		if (!Next.IsValid()) continue;

		RequestSession(*Next);
		return;
	}
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
		OnStatusChanged.Broadcast(Conversation);
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

	// 记下提问而没有记下回合结束的一轮没有正常结束：编辑器在回合中关闭，或运行时在回合中退出
	TSharedPtr<FShuttlegentChatEntry> OpenPrompt;
	for (const TSharedPtr<FJsonObject>& Item : Pinned->GetSessionStore().LoadTranscript(Conversation.SessionId))
	{
		FString Kind;
		const TSharedPtr<FJsonObject>* Data = nullptr;
		if (!Item->TryGetStringField(ShuttlegentChatModelPrivate::FieldKind, Kind) || !Item->TryGetObjectField(ShuttlegentChatModelPrivate::FieldData, Data)) continue;

		if (Kind == ShuttlegentChatModelPrivate::TranscriptPrompt)
		{
			if (OpenPrompt.IsValid()) AddInterruptedNotice(Conversation);

			FString Text;
			(*Data)->TryGetStringField(ShuttlegentChatModelPrivate::FieldText, Text);
			const TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(Conversation, EShuttlegentChatEntryKind::User, Text);
			(*Data)->TryGetStringArrayField(ShuttlegentChatModelPrivate::FieldImages, Entry->Images);

			FString Time;
			FDateTime At;
			if (Item->TryGetStringField(ShuttlegentChatModelPrivate::FieldTime, Time) && FDateTime::ParseIso8601(*Time, At)) Entry->TimeUtc = At;
			OpenPrompt = Entry;
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
			OpenPrompt.Reset();
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
		else if (Kind == ShuttlegentChatModelPrivate::TranscriptChoice)
		{
			FShuttlegentChoiceRequest Request;
			FShuttlegentChoiceAnswer Answer;
			FString Status;
			if (!FShuttlegentChoiceJson::FromJson(**Data, Request, Answer, Status)) continue;

			// 同一张卡片先记出现、后记结束，后一条更新同一条目
			TSharedPtr<FShuttlegentChatEntry>* Found = Conversation.Entries.FindByPredicate([&Request](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry->Kind == EShuttlegentChatEntryKind::Choice && Entry->Choice.ChoiceKey == Request.ChoiceKey; });
			const TSharedPtr<FShuttlegentChatEntry> Entry = Found != nullptr ? *Found : AddEntry(Conversation, EShuttlegentChatEntryKind::Choice, Request.Question);
			Entry->Choice = MoveTemp(Request);
			Entry->ChoiceAnswer = MoveTemp(Answer);
			Entry->Status = Status;
			Conversation.ChoiceEntryId = Status == ShuttlegentEditorWebField::ChoicePending ? Entry->Id : 0;
		}
	}

	// 刚打开的对话没有人在等作答：记录里仍在等待的选择卡已经作废
	if (Conversation.ChoiceEntryId != 0)
	{
		for (const TSharedPtr<FShuttlegentChatEntry>& Entry : Conversation.Entries)
		{
			if (Entry->Id == Conversation.ChoiceEntryId) Entry->Status = ShuttlegentEditorWebField::ChoiceUnanswered;
		}
		Conversation.ChoiceEntryId = 0;
	}

	// 最后一轮被中断：可以“重试”，重新发送那条提问
	if (OpenPrompt.IsValid())
	{
		AddInterruptedNotice(Conversation);
		bool bAllImages = true;
		Conversation.LastSentPrompt = OpenPrompt->Text;
		Conversation.LastSentImages = ShuttlegentChatModelPrivate::LoadPromptImages(OpenPrompt->Images, bAllImages);
		Conversation.LastOutcome = EShuttlegentTurnOutcome::Failed;
		if (!bAllImages) AddNotice(Conversation, LOCTEXT("InterruptedImagesMissing", "这条消息所附的图片有的已找不到，重试时只发送找得到的；需要的话请重新添加。"), false);
		UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent session %s: the last turn did not end (editor closed or runtime lost during the turn); retry is offered."), *Conversation.SessionId);
	}
	NotifyStructure(Conversation);
}

void FShuttlegentChatModel::AddInterruptedNotice(FShuttlegentConversation& Conversation)
{
	AddNotice(Conversation, LOCTEXT("TurnInterrupted", "这一轮没有正常结束（编辑器在回合中关闭，或运行时中途退出）；当时未确认的写入、权限请求与提问均已作废。"), true);
}

void FShuttlegentChatModel::HandleStateChanged(EShuttlegentAgentState NewState)
{
	// 启动时的配置文件刚按当前设置写好：记下运行时读到的模型连接
	if (NewState == EShuttlegentAgentState::Starting) LaunchedModelSignature = ShuttlegentChatModelPrivate::MakeModelSignature();

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

		// 启动期间连接测完或被改动时，运行时读到的已过时
		ApplyModelSettings();
	}

	NotifyAllStatus();
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

	if (AgentHandoff.Pin() == Conversation) ReleaseAgentHandoff();
	const bool bResumed = Conversation->bResuming;
	if (bResumed) AddRunLog(*Conversation, LOCTEXT("LogResumed", "会话已恢复"), false);
	Conversation->SessionId = SessionId;
	Conversation->bResuming = false;
	Conversation->bResumeFailed = false;
	bRuntimeLost = false;

	// 新会话记下建立它的 Agent，恢复与分支都以此为准
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	const FShuttlegentSessionRecord* Record = Pinned.IsValid() ? Pinned->GetSessionStore().FindRecord(SessionId) : nullptr;
	if (Record != nullptr && Record->AgentId.IsEmpty() && !Conversation->AgentId.IsEmpty())
	{
		FShuttlegentSessionRecord Updated = *Record;
		Updated.AgentId = Conversation->AgentId;
		Pinned->GetSessionStore().UpsertRecord(Updated);
	}

	// 新会话记下发出第一条消息前选好的写入方式；恢复的会话取自记录，不变
	RecordWriteMode(*Conversation);

	// 等待会话 id 期间排进队列的提示此时才有地方存
	if (!Conversation->Queue.Items.IsEmpty()) Drafts.SetQueue(SessionId, Conversation->Queue.Items);

	const TMap<FString, FString> PendingConfig = MoveTemp(Conversation->PendingConfig);
	Conversation->PendingConfig.Reset();
	for (const TPair<FString, FString>& Pair : PendingConfig)
	{
		// 会话打开前的模型按个人设置列出；换成运行时选项中路由与模型 ID 相同的那一项，运行时没有它时不切换
		FString Value = Pair.Value;
		FString Route;
		FString ModelId;
		if (Pair.Key == ShuttlegentEditorConfigId::Model && ShuttlegentChatModelPrivate::ParseModelValue(Pair.Value, Route, ModelId))
		{
			TArray<FShuttlegentConfigChoice> Choices;
			FString Current;
			GetConfigChoices(*Conversation, ShuttlegentEditorConfigId::Model, Choices, Current);
			Value = ShuttlegentChatModelPrivate::FindModelChoice(Choices, Route, ModelId);
			if (Value.IsEmpty() || Value == Current) continue;
		}
		SetConfigValue(*Conversation, Pair.Key, Value);
	}

	// 恢复的会话沿用记录中的模型；那个模型的连接已不可用（没有密钥、未连通或已删除）时换成默认模型
	if (bResumed && !PendingConfig.Contains(ShuttlegentEditorConfigId::Model)) ReplaceUnusableModel(*Conversation);

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

	// 等会话建立期间改动的模型连接此时生效（随即要发送的提示会让它等到回合结束）
	ApplyModelSettings();
	NotifyAllStatus();
	OnSessionsChanged.Broadcast();
}

void FShuttlegentChatModel::ReplaceUnusableModel(FShuttlegentConversation& Conversation)
{
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	TArray<FShuttlegentConfigChoice> Choices;
	FString Current;
	FString Route;
	FString ModelId;
	if (!IsValid(Settings) || !GetConfigChoices(Conversation, ShuttlegentEditorConfigId::Model, Choices, Current)) return;
	if (!ShuttlegentChatModelPrivate::ParseModelValue(Current, Route, ModelId) || FShuttlegentModelRoutes::IsUsable(*Settings, Route, ModelId)) return;

	FShuttlegentModelRoute DefaultRoute;
	FString DefaultModel;
	if (!FShuttlegentModelRoutes::FindDefaultModel(*Settings, DefaultRoute, DefaultModel)) return;

	const FString Value = ShuttlegentChatModelPrivate::FindModelChoice(Choices, DefaultRoute.Route, DefaultModel);
	if (Value.IsEmpty()) return;

	SetConfigValue(Conversation, ShuttlegentEditorConfigId::Model, Value);
	AddRunLog(Conversation, FText::Format(LOCTEXT("LogModelReplaced", "原模型 {0} 的连接已不可用，已换成 {1}"), FText::FromString(ModelId), FText::FromString(DefaultModel)), true);
	UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent session %s: model %s (%s) is no longer usable; switched to %s (%s)."), *Conversation.SessionId, *ModelId, *Route, *DefaultModel, *DefaultRoute.Route);
}

void FShuttlegentChatModel::HandleSettingChanged(UObject* Settings, FPropertyChangedEvent& Event)
{
	ApplyModelSettings();
	NotifyAllStatus();
}

void FShuttlegentChatModel::HandleConnectionTested(int32 ConnectionIndex)
{
	ApplyModelSettings();
	NotifyAllStatus();
}

void FShuttlegentChatModel::ApplyModelSettings()
{
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	if (!IsValid(Settings) || !Pinned.IsValid() || bRestartPending) return;

	// 地址或密钥刚改、正在或即将重新检测的连接，等检测完（OnTested）再定，免得按旧结论重启一次
	const bool bTesting = Settings->Connections.ContainsByPredicate([](const FShuttlegentConnection& Connection)
	{
		return !Connection.ApiKey.TrimStartAndEnd().IsEmpty() && (Connection.Status == EShuttlegentConnectionStatus::Testing || Connection.TestedFingerprint != FShuttlegentModelRoutes::MakeFingerprint(Connection));
	});
	if (bTesting) return;

	// 因没有可用连接或密钥被拒而停下的运行时：有连通的连接后清掉启动引导卡片，再次启动并恢复获得焦点的对话的会话
	const bool bKeyIssue = StartIssue == EShuttlegentStartIssue::MissingKey || StartIssue == EShuttlegentStartIssue::AuthFailed;
	if (Pinned->GetState() == EShuttlegentAgentState::Stopped && bKeyIssue)
	{
		FShuttlegentModelRoute DefaultRoute;
		FString DefaultModel;
		if (!FShuttlegentModelRoutes::FindDefaultModel(*Settings, DefaultRoute, DefaultModel)) return;

		UE_LOG(LogShuttlegent, Log, TEXT("A model connection is now connected; starting the runtime again."));
		StartIssue = EShuttlegentStartIssue::None;
		StartIssueDetail.Reset();
		bRuntimeLost = false;
		const TSharedPtr<FShuttlegentConversation> Focus = GetFocused();
		if (Focus.IsValid() && !Focus->SessionId.IsEmpty()) RequestSession(*Focus);
		else EnsureRuntime();
		return;
	}

	if (Pinned->GetState() != EShuttlegentAgentState::Ready || ShuttlegentChatModelPrivate::MakeModelSignature() == LaunchedModelSignature) return;

	// 重启会中断进行中的回合、正在建立的会话与正在执行的写入，等它们结束（回合结束、会话就绪时再来）
	const bool bBusy = bWriting || AgentHandoff.IsValid() || !AwaitingNewSession.IsEmpty() || !WaitingForHandoff.IsEmpty()
		|| Conversations.ContainsByPredicate([this](const TSharedPtr<FShuttlegentConversation>& Held) { return IsPrompting(*Held) || Held->bResuming || !Held->PendingPrompt.IsEmpty(); });
	if (bBusy) return;

	UE_LOG(LogShuttlegent, Log, TEXT("Model connections changed since the runtime started; restarting the runtime."));
	const TSharedPtr<FShuttlegentConversation> Focus = GetFocused();
	if (Focus.IsValid()) AddRunLog(*Focus, LOCTEXT("LogModelsChanged", "模型连接已变化，重启运行时"), false);

	// 旧进程退出后由 HandleRuntimeExited 再次启动，并恢复获得焦点的对话的会话
	bRestartPending = true;
	Pinned->Stop();
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

	// 用户停止的回合里交出的选择卡随之取消
	if (Conversation.LastOutcome == EShuttlegentTurnOutcome::Stopped) SettleChoice(Conversation, ShuttlegentEditorWebField::ChoiceCancelled, FShuttlegentChoiceAnswer());

	if (Conversation.LastOutcome == EShuttlegentTurnOutcome::Completed) AddRunLog(Conversation, LOCTEXT("LogTurnCompleted", "本轮结束"), false);
	else if (Conversation.LastOutcome == EShuttlegentTurnOutcome::Stopped) AddRunLog(Conversation, LOCTEXT("LogTurnStopped", "本轮已停止"), true);
	else AddRunLog(Conversation, LOCTEXT("LogTurnFailed", "本轮查询失败"), true);

	// 回合结束时仍处于打开状态的请求已无法再处理：带对话标识的请求随其对话的回合过期；
	// 未带标识的请求不知属于谁，只有在不再有任何会话运行回合时才会过期。
	RejectPendingPermissions(Conversation);
	const bool bAnyPrompting = Conversations.ContainsByPredicate([this](const TSharedPtr<FShuttlegentConversation>& Held) { return IsPrompting(*Held); });
	const TSharedPtr<FShuttlegentWriteGate> Gate = WriteGate.Pin();
	if (Gate.IsValid())
	{
		Gate->ExpireConversation(Conversation.ConversationKey);
		if (!bAnyPrompting) Gate->ExpireAll();
	}
	const TSharedPtr<FShuttlegentQuestionGate> Questions = QuestionGate.Pin();
	if (Questions.IsValid())
	{
		Questions->ExpireConversation(Conversation.ConversationKey);
		if (!bAnyPrompting) Questions->ExpireAll();
	}

	ApplyTurnEnd(Conversation, StopReason, ErrorMessage);

	// 用户没在看这个对话时留下“完成未看”；用户自己停止的回合不提醒
	const bool bSeen = Conversation.VisibleCount > 0;
	if (Conversation.LastOutcome != EShuttlegentTurnOutcome::Stopped && !bSeen) Conversation.bUnseenFinish = true;

	OnTurnFinished.Broadcast(Conversation, bCompleted);
	NotifyStructure(Conversation);
	NotifyStatus(Conversation);

	if (bCompleted) RequestTitle(Conversation);
	if (bCompleted) SendNextQueued(Conversation);
	else PauseQueue(Conversation);

	// 回合进行中改动的模型连接等所有回合结束后生效；队列接着发出的提示会让它继续等
	ApplyModelSettings();
}

void FShuttlegentChatModel::HandleRuntimeExited(int32 ExitCode, bool bExpected)
{
	AwaitingNewSession.Reset();
	AgentHandoff.Reset();
	WaitingForHandoff.Reset();
	ModelImageInput.Reset();
	if (!bExpected) bRuntimeLost = true;
	ParkPendingPrompts();

	// 已确认但尚未收到结果的写入可能已执行也可能未执行；由用户核对其对象
	if (bWriting)
	{
		bWriting = false;
		const TSharedPtr<FShuttlegentConversation> ApprovedOwner = ApprovedWriteOwner.Pin();
		const TSharedPtr<FShuttlegentConversation> Owner = ApprovedOwner.IsValid() ? ApprovedOwner : GetFocused();
		ApprovedWriteOwner.Reset();
		if (Owner.IsValid())
		{
			TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(*Owner, EShuttlegentChatEntryKind::WriteUnknown, ApprovedWrite.Action);
			Entry->WriteResult.RequestId = ApprovedWrite.RequestId;
			Entry->WriteResult.Action = ApprovedWrite.Action;
			Entry->WriteResult.Objects = ApprovedWrite.Objects;
			Owner->bNeedsVerify = true;
			AddRunLog(*Owner, LOCTEXT("LogWriteUnknown", "编辑器写入结果未知"), true);
			NotifyStructure(*Owner);
		}
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

	NotifyAllStatus();
	for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
	{
		OnQueueChanged.Broadcast(*Conversation);
	}
	OnSessionsChanged.Broadcast();

	// 重试已停止运行时，以便其读取修正后的密钥或可执行文件；现在再次启动，并恢复获得焦点的对话的会话
	if (bRestartPending)
	{
		bRestartPending = false;
		const TSharedPtr<FShuttlegentConversation> Focus = GetFocused();
		if (Focus.IsValid() && !Focus->SessionId.IsEmpty()) RequestSession(*Focus);
		else EnsureRuntime();
	}
}

void FShuttlegentChatModel::HandleError(const FString& SessionId, const FString& Message)
{
	// 不带会话的错误是失败的 session/new（属于最早一个等待的对话）或失败的启动（属于获得焦点的对话）
	TSharedPtr<FShuttlegentConversation> Conversation = FindConversation(SessionId);
	while (!Conversation.IsValid() && SessionId.IsEmpty() && !AwaitingNewSession.IsEmpty())
	{
		Conversation = AwaitingNewSession[0].Pin();
		AwaitingNewSession.RemoveAt(0);
	}
	if (Conversation.IsValid() && AgentHandoff.Pin() == Conversation) ReleaseAgentHandoff();

	// 失败的 initialize 不属于任何对话；由启动引导卡片显示
	const TSharedPtr<FShuttlegentAgentService> Pinned = Service.Pin();
	if (!Conversation.IsValid() && SessionId.IsEmpty() && Pinned.IsValid() && Pinned->GetStartIssue() != EShuttlegentStartIssue::None)
	{
		RecordStartIssue(Pinned->GetStartIssue(), Message);
		NotifyAllStatus();
		return;
	}
	if (!Conversation.IsValid()) Conversation = GetFocused();
	if (!Conversation.IsValid()) return;

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

		// 新对话建立时还没有模型选项，Agent 的默认模型此时才能对上
		for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
		{
			if (!ShuttlegentChatModelPrivate::IsAgentLocked(*Conversation) && !Conversation->PendingConfig.Contains(ShuttlegentEditorConfigId::Model)) SetAgent(*Conversation, Conversation->AgentId);
		}

		TArray<FShuttlegentConfigChoice> Models;
		FString ModelValue;
		ShuttlegentChatModelPrivate::ParseConfigChoices(Options, ShuttlegentEditorConfigId::Model, Models, ModelValue);
		RequestImageInput(ModelValue);
	}

	NotifyAllStatus();
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
		This->NotifyAllStatus();
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

	// 关闭权限提升时，请求立即得到答复并留下可读记录
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
	// 带对话标识的请求归属发起它的对话；未带时（运行时没有转发请求头）退回到获得焦点的对话
	TSharedPtr<FShuttlegentConversation> Owner = FindConversationByKey(Request.ConversationKey);
	if (!Owner.IsValid())
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Write request %d (%s) names no held conversation ('%s'); it goes to the focused conversation."), Request.RequestId, *Request.ToolName, *Request.ConversationKey);
		Owner = GetFocused();
	}
	OpenWriteRequests.Add(Request.RequestId, Request);
	if (!Owner.IsValid()) return;

	WriteOwners.Add(Request.RequestId, Owner);

	AddRunLog(*Owner, FText::Format(LOCTEXT("LogWriteRequested", "编辑器写入请求：{0}"), FText::FromString(Request.Action)), true);
	NotifyStatus(*Owner);
}

void FShuttlegentChatModel::HandleWriteClosed(int32 RequestId, EShuttlegentWriteDecision Decision)
{
	TWeakPtr<FShuttlegentConversation> RequestOwner;
	WriteOwners.RemoveAndCopyValue(RequestId, RequestOwner);
	const TSharedPtr<FShuttlegentConversation> PinnedOwner = RequestOwner.Pin();
	const TSharedPtr<FShuttlegentConversation> Owner = PinnedOwner.IsValid() ? PinnedOwner : GetFocused();

	FShuttlegentWriteRequest Closed;
	OpenWriteRequests.RemoveAndCopyValue(RequestId, Closed);
	if (Decision == EShuttlegentWriteDecision::Approved)
	{
		ApprovedWrite = MoveTemp(Closed);
		ApprovedWriteOwner = Owner;
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

	const TSharedPtr<FShuttlegentConversation> ApprovedOwner = ApprovedWriteOwner.Pin();
	const TSharedPtr<FShuttlegentConversation> Owner = ApprovedOwner.IsValid() ? ApprovedOwner : GetFocused();
	ApprovedWriteOwner.Reset();
	bWriting = false;
	EnsureUndoListener();
	if (!Owner.IsValid()) return;

	TSharedPtr<FShuttlegentChatEntry> Entry = AddEntry(*Owner, EShuttlegentChatEntryKind::WriteResult, Result.Action);
	Entry->WriteResult = Result;
	Entry->bAutoApproved = ApprovedWrite.RequestId == Result.RequestId && ApprovedWrite.bAutoApproved;

	// 未完成全部对象的批次会使队列停止
	const bool bAllCompleted = !Result.Objects.ContainsByPredicate([](const FShuttlegentWriteObject& Object) { return Object.Status != EShuttlegentWriteObjectStatus::Completed; });
	AddRunLog(*Owner, bAllCompleted ? LOCTEXT("LogWriteCompleted", "编辑器写入完成") : LOCTEXT("LogWritePartial", "编辑器写入未全部完成"), !bAllCompleted);
	if (!bAllCompleted) PauseQueue(*Owner);

	NotifyStructure(*Owner);
	NotifyStatus(*Owner);
}

void FShuttlegentChatModel::HandleUndoBufferChanged()
{
	// 任何新的事务或撤销都可能改变某个结果是否仍位于顶部
	for (const TSharedPtr<FShuttlegentConversation>& Conversation : Conversations)
	{
		const bool bHasWriteResults = Conversation->Entries.ContainsByPredicate([](const TSharedPtr<FShuttlegentChatEntry>& Entry) { return Entry.IsValid() && Entry->Kind == EShuttlegentChatEntryKind::WriteResult; });
		if (bHasWriteResults) OnContentChanged.Broadcast(*Conversation);
	}
}

void FShuttlegentChatModel::HandleQuestionsChanged()
{
	// 各视图在输入区的位置显示属于自己对话的第一个问题；会话列表随之刷新各对话的提醒
	NotifyAllStatus();
	OnSessionsChanged.Broadcast();
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
	const TSharedPtr<FShuttlegentConversation> Owner = FindQuestionOwner(Question);
	AddNotice(*Owner, Notice, Outcome != EShuttlegentQuestionOutcome::Answered);
	NotifyStructure(*Owner);
}

void FShuttlegentChatModel::EnsureUndoListener()
{
	UTransBuffer* TransBuffer = GEditor != nullptr ? Cast<UTransBuffer>(GEditor->Trans) : nullptr;
	if (IsValid(TransBuffer) && !UndoBufferHandle.IsValid()) UndoBufferHandle = TransBuffer->OnUndoBufferChanged().AddSP(this, &FShuttlegentChatModel::HandleUndoBufferChanged);
	if (!PostUndoRedoHandle.IsValid()) PostUndoRedoHandle = FEditorDelegates::PostUndoRedo.AddSP(this, &FShuttlegentChatModel::HandleUndoBufferChanged);
}

#undef LOCTEXT_NAMESPACE
