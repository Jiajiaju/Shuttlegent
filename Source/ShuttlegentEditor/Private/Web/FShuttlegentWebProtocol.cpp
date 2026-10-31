#include "Web/FShuttlegentWebProtocol.h"

#include "Dom/JsonObject.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Chat/FShuttlegentToolPresentation.h"
#include "Chat/ShuttlegentChatModel.h"
#include "Reference/FShuttlegentReferencePrompt.h"
#include "Web/FShuttlegentWebJson.h"
#include "WriteGate/FShuttlegentWriteRequest.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentEditorConstants.h"
#include "FShuttlegentProjectConfig.h"
#include "Template/FShuttlegentTemplateLibrary.h"
#include "ShuttlegentSettings.h"
#include "Endpoint/FShuttlegentContextBreakdown.h"
#include "Endpoint/FShuttlegentExternalMcpState.h"
#include "FShuttlegentToolsets.h"
#include "ShuttlegentMcpServerModule.h"
#include "ShuttlegentProjectSettings.h"
#include "Tool/IShuttlegentTool.h"

namespace FShuttlegentWebProtocolPrivate
{
	const TCHAR* KindName(EShuttlegentChatEntryKind Kind)
	{
		switch (Kind)
		{
		case EShuttlegentChatEntryKind::User:
			{
				return ShuttlegentEditorWebField::KindUser;
			}
		case EShuttlegentChatEntryKind::Assistant:
			{
				return ShuttlegentEditorWebField::KindAssistant;
			}
		case EShuttlegentChatEntryKind::ToolCall:
			{
				return ShuttlegentEditorWebField::KindToolCall;
			}
		case EShuttlegentChatEntryKind::Permission:
			{
				return ShuttlegentEditorWebField::KindPermission;
			}
		case EShuttlegentChatEntryKind::TurnEnd:
			{
				return ShuttlegentEditorWebField::KindTurnEnd;
			}
		case EShuttlegentChatEntryKind::WriteResult:
			{
				return ShuttlegentEditorWebField::KindWriteResult;
			}
		case EShuttlegentChatEntryKind::WriteUnknown:
			{
				return ShuttlegentEditorWebField::KindWriteUnknown;
			}
		case EShuttlegentChatEntryKind::Notice:
		default:
			{
				return ShuttlegentEditorWebField::KindNotice;
			}
		}
	}

	const TCHAR* StatusName(EShuttlegentChatStatus Status)
	{
		switch (Status)
		{
		case EShuttlegentChatStatus::Connecting:
			{
				return ShuttlegentEditorWebField::StatusConnecting;
			}
		case EShuttlegentChatStatus::Connected:
			{
				return ShuttlegentEditorWebField::StatusConnected;
			}
		case EShuttlegentChatStatus::Querying:
			{
				return ShuttlegentEditorWebField::StatusQuerying;
			}
		case EShuttlegentChatStatus::AwaitingPermission:
			{
				return ShuttlegentEditorWebField::StatusAwaitingPermission;
			}
		case EShuttlegentChatStatus::AwaitingWriteConfirmation:
			{
				return ShuttlegentEditorWebField::StatusAwaitingWrite;
			}
		case EShuttlegentChatStatus::Writing:
			{
				return ShuttlegentEditorWebField::StatusWriting;
			}
		case EShuttlegentChatStatus::Stopping:
			{
				return ShuttlegentEditorWebField::StatusStopping;
			}
		case EShuttlegentChatStatus::Stopped:
			{
				return ShuttlegentEditorWebField::StatusStopped;
			}
		case EShuttlegentChatStatus::Failed:
			{
				return ShuttlegentEditorWebField::StatusFailed;
			}
		case EShuttlegentChatStatus::Lost:
			{
				return ShuttlegentEditorWebField::StatusLost;
			}
		case EShuttlegentChatStatus::Resuming:
			{
				return ShuttlegentEditorWebField::StatusResuming;
			}
		case EShuttlegentChatStatus::StartBlocked:
			{
				return ShuttlegentEditorWebField::StatusStartBlocked;
			}
		case EShuttlegentChatStatus::ResumeFailed:
			{
				return ShuttlegentEditorWebField::StatusResumeFailed;
			}
		case EShuttlegentChatStatus::NotConnected:
		default:
			{
				return ShuttlegentEditorWebField::StatusNotConnected;
			}
		}
	}

	const TCHAR* IssueName(EShuttlegentStartIssue Issue)
	{
		switch (Issue)
		{
		case EShuttlegentStartIssue::MissingRuntime:
			{
				return ShuttlegentEditorWebField::IssueMissingRuntime;
			}
		case EShuttlegentStartIssue::MissingKey:
			{
				return ShuttlegentEditorWebField::IssueMissingKey;
			}
		case EShuttlegentStartIssue::AuthFailed:
			{
				return ShuttlegentEditorWebField::IssueAuthFailed;
			}
		case EShuttlegentStartIssue::Other:
		case EShuttlegentStartIssue::None:
		default:
			{
				return ShuttlegentEditorWebField::IssueOther;
			}
		}
	}

	/** 启动引导卡片：阻碍运行时启动的原因及修复位置。只给出变量名与路径，绝不包含密钥值。 */
	TSharedPtr<FJsonValue> MakeStartIssue(EShuttlegentStartIssue Issue, const FString& Detail)
	{
		if (Issue == EShuttlegentStartIssue::None) return MakeShared<FJsonValueNull>();

		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ShuttlegentPath::PluginName);
		const FString PluginDir = Plugin.IsValid() ? FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()) : FString();
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		const bool bOverride = IsValid(Settings) && !Settings->RuntimeExecutableOverride.FilePath.IsEmpty();
		const FString RuntimePath = bOverride ? FPaths::ConvertRelativePathToFull(Settings->RuntimeExecutableOverride.FilePath) : FPaths::Combine(PluginDir, ShuttlegentPath::RuntimeExecutable);

		// 默认路由：缺少或被拒绝的密钥所对应的路由
		FString KeyVariable;
		FString ProviderName;
		FShuttlegentProjectConfig::GetDefaultProviderKey(KeyVariable, ProviderName);

		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(ShuttlegentEditorWebField::IssueKind, IssueName(Issue));
		Item->SetStringField(ShuttlegentEditorWebField::Detail, Detail);
		Item->SetStringField(ShuttlegentEditorWebField::IssueProvider, ProviderName);
		Item->SetStringField(ShuttlegentEditorWebField::IssueRuntimePath, RuntimePath);
		Item->SetStringField(ShuttlegentEditorWebField::IssueBuildScript, FPaths::Combine(PluginDir, ShuttlegentEditorRuntime::BuildScript));
		return MakeShared<FJsonValueObject>(Item);
	}

	/** 页面所显示的该条目任何内容发生变化时即随之变化。流式文本只会增长，因此以长度代替文本本身。 */
	uint32 Fingerprint(const FShuttlegentChatEntry& Entry)
	{
		uint32 Hash = GetTypeHash(static_cast<uint8>(Entry.Kind));
		Hash = HashCombine(Hash, GetTypeHash(Entry.Text.Len()));
		Hash = HashCombine(Hash, GetTypeHash(Entry.Thought.Len()));
		Hash = HashCombine(Hash, GetTypeHash(Entry.Input.Len()));
		Hash = HashCombine(Hash, GetTypeHash(Entry.Result.Len()));
		Hash = HashCombine(Hash, GetTypeHash(Entry.Title));
		Hash = HashCombine(Hash, GetTypeHash(Entry.Status));
		Hash = HashCombine(Hash, GetTypeHash(Entry.ThoughtEndUtc.GetTicks()));
		Hash = HashCombine(Hash, GetTypeHash(Entry.bAnswered));
		Hash = HashCombine(Hash, GetTypeHash(Entry.bAllowed));
		Hash = HashCombine(Hash, GetTypeHash(Entry.bError));
		return Hash;
	}

	/** 第一个与最新一个推理分块之间的秒数，至少为 1；没有计时推理时为 NoThoughtSeconds。 */
	int32 ThoughtSeconds(const FShuttlegentChatEntry& Entry)
	{
		if (Entry.ThoughtStartUtc.GetTicks() <= 0) return ShuttlegentEditorWebField::NoThoughtSeconds;

		return FMath::Max(1, FMath::CeilToInt((Entry.ThoughtEndUtc - Entry.ThoughtStartUtc).GetTotalSeconds()));
	}

	const TCHAR* ObjectStatusName(EShuttlegentWriteObjectStatus Status)
	{
		switch (Status)
		{
		case EShuttlegentWriteObjectStatus::Completed:
			{
				return ShuttlegentEditorWebField::ObjectCompleted;
			}
		case EShuttlegentWriteObjectStatus::Failed:
			{
				return ShuttlegentEditorWebField::ObjectFailed;
			}
		case EShuttlegentWriteObjectStatus::RolledBack:
			{
				return ShuttlegentEditorWebField::ObjectRolledBack;
			}
		case EShuttlegentWriteObjectStatus::NotRun:
		default:
			{
				return ShuttlegentEditorWebField::ObjectNotRun;
			}
		}
	}

	TArray<TSharedPtr<FJsonValue>> MakeWriteObjects(const TArray<FShuttlegentWriteObject>& Objects)
	{
		TArray<TSharedPtr<FJsonValue>> Values;
		for (const FShuttlegentWriteObject& Object : Objects)
		{
			const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(ShuttlegentEditorWebField::Name, Object.Name);
			Item->SetStringField(ShuttlegentEditorWebField::Path, Object.Path);
			Item->SetStringField(ShuttlegentEditorWebField::Field, Object.Field);
			Item->SetStringField(ShuttlegentEditorWebField::CurrentValue, Object.CurrentValue);
			Item->SetStringField(ShuttlegentEditorWebField::PlannedValue, Object.PlannedValue);
			Item->SetStringField(ShuttlegentEditorWebField::ObjectStatus, ObjectStatusName(Object.Status));
			Item->SetStringField(ShuttlegentEditorWebField::Message, Object.Message);
			Values.Add(MakeShared<FJsonValueObject>(Item));
		}
		return Values;
	}

	const TCHAR* ExternalIssueName(EShuttlegentExternalMcpIssue Issue)
	{
		switch (Issue)
		{
		case EShuttlegentExternalMcpIssue::NameInvalid:
			{
				return ShuttlegentEditorWebField::IssueNameInvalid;
			}
		case EShuttlegentExternalMcpIssue::NameTaken:
			{
				return ShuttlegentEditorWebField::IssueNameTaken;
			}
		case EShuttlegentExternalMcpIssue::CommandNotAbsolute:
			{
				return ShuttlegentEditorWebField::IssueCommandNotAbsolute;
			}
		case EShuttlegentExternalMcpIssue::UrlMissing:
			{
				return ShuttlegentEditorWebField::IssueUrlMissing;
			}
		case EShuttlegentExternalMcpIssue::HeaderValueMissing:
			{
				return ShuttlegentEditorWebField::IssueHeaderValueMissing;
			}
		case EShuttlegentExternalMcpIssue::None:
		default:
			{
				return ShuttlegentEditorWebField::IssueNone;
			}
		}
	}

	/** UTC 时间对应的 Unix 毫秒时间；页面按本地时间格式化。 */
	double UnixMilliseconds(const FDateTime& UtcTime)
	{
		return static_cast<double>(UtcTime.ToUnixTimestamp() * ShuttlegentEditorWebField::MillisecondsPerSecond + UtcTime.GetMillisecond());
	}

	/** 会话列表项；面板所持有的会话还带有其状态与排队数量。 */
	TSharedPtr<FJsonValue> MakeSession(const FShuttlegentSessionRecord& Record, const FShuttlegentChatModel& Model)
	{
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(ShuttlegentEditorWebField::Id, Record.SessionId);
		Item->SetStringField(ShuttlegentEditorWebField::Title, Record.Title);
		Item->SetStringField(ShuttlegentEditorWebField::ModelName, Record.ModelName);
		Item->SetNumberField(ShuttlegentEditorWebField::UpdatedAt, UnixMilliseconds(Record.UpdatedAt));
		Item->SetBoolField(ShuttlegentEditorWebField::CanArchive, Model.CanArchive(Record.SessionId));

		EShuttlegentChatStatus Status;
		int32 Queued = 0;
		if (Model.GetSessionActivity(Record.SessionId, Status, Queued))
		{
			Item->SetStringField(ShuttlegentEditorWebField::Status, StatusName(Status));
			Item->SetNumberField(ShuttlegentEditorWebField::QueuedCount, Queued);
		}
		return MakeShared<FJsonValueObject>(Item);
	}
}

void FShuttlegentWebProtocol::Initialize(const TSharedRef<FShuttlegentChatModel>& InModel, TFunction<void(const FString&)> InPush)
{
	Model = InModel;
	Push = MoveTemp(InPush);

	InModel->OnStatusChanged.AddSP(this, &FShuttlegentWebProtocol::HandleStatusChanged);
	InModel->OnStructureChanged.AddSP(this, &FShuttlegentWebProtocol::HandleEntriesChanged);
	InModel->OnContentChanged.AddSP(this, &FShuttlegentWebProtocol::HandleEntriesChanged);
	InModel->OnTurnFinished.AddSP(this, &FShuttlegentWebProtocol::HandleTurnFinished);
	InModel->OnSessionsChanged.AddSP(this, &FShuttlegentWebProtocol::HandleSessionsChanged);
	InModel->OnRunLogChanged.AddSP(this, &FShuttlegentWebProtocol::HandleRunLogChanged);
	InModel->OnQueueChanged.AddSP(this, &FShuttlegentWebProtocol::HandleQueueChanged);
	InModel->OnDraftRestored.AddSP(this, &FShuttlegentWebProtocol::HandleDraftRestored);

	UShuttlegentSettings* Settings = GetMutableDefault<UShuttlegentSettings>();
	if (IsValid(Settings)) SettingsChangedHandle = Settings->OnSettingChanged().AddSP(this, &FShuttlegentWebProtocol::HandleSettingChanged);

	// 外部 MCP 服务器配置在项目设置中
	UShuttlegentProjectSettings* ProjectSettings = GetMutableDefault<UShuttlegentProjectSettings>();
	if (IsValid(ProjectSettings)) ProjectSettingsChangedHandle = ProjectSettings->OnSettingChanged().AddSP(this, &FShuttlegentWebProtocol::HandleProjectSettingChanged);

	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateSP(this, &FShuttlegentWebProtocol::HandleTick));
}

void FShuttlegentWebProtocol::Shutdown()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();

	UShuttlegentSettings* Settings = GetMutableDefault<UShuttlegentSettings>();
	if (IsValid(Settings) && SettingsChangedHandle.IsValid()) Settings->OnSettingChanged().Remove(SettingsChangedHandle);
	SettingsChangedHandle.Reset();

	UShuttlegentProjectSettings* ProjectSettings = GetMutableDefault<UShuttlegentProjectSettings>();
	if (IsValid(ProjectSettings) && ProjectSettingsChangedHandle.IsValid()) ProjectSettings->OnSettingChanged().Remove(ProjectSettingsChangedHandle);
	ProjectSettingsChangedHandle.Reset();

	if (Model.IsValid())
	{
		Model->OnStatusChanged.RemoveAll(this);
		Model->OnStructureChanged.RemoveAll(this);
		Model->OnContentChanged.RemoveAll(this);
		Model->OnTurnFinished.RemoveAll(this);
		Model->OnSessionsChanged.RemoveAll(this);
		Model->OnRunLogChanged.RemoveAll(this);
		Model->OnQueueChanged.RemoveAll(this);
		Model->OnDraftRestored.RemoveAll(this);
	}
	Model.Reset();
	Push = nullptr;
}

void FShuttlegentWebProtocol::HandlePageReady()
{
	bPageReady = true;
	bResetPending = true;

	// 重新加载之前的事件已体现在快照中
	PendingEvents.Reset();
	Flush();
}

bool FShuttlegentWebProtocol::HandleTick(float DeltaTime)
{
	if (bPageReady && (bResetPending || bStatusDirty || bEntriesDirty || bSessionsDirty || bRunLogDirty || bQueueDirty || bMcpDirty || bDraftPending || PendingEvents.Num() > 0)) Flush();
	return true;
}

void FShuttlegentWebProtocol::Flush()
{
	if (!Model.IsValid() || !Push) return;

	const bool bReset = bResetPending;
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	if (bReset) Root->SetBoolField(ShuttlegentEditorWebField::Reset, true);

	if (bReset || bStatusDirty)
	{
		WriteStatus(Root);
		WriteConfig(Root);
	}
	if (bReset)
	{
		WriteTemplates(Root);
		WritePaneWidths(Root);
	}
	if (bReset || bEntriesDirty) WriteEntries(Root, bReset);
	if (bReset || bStatusDirty || bEntriesDirty) WritePendingPermission(Root);
	if (bReset || bStatusDirty) WritePendingWrite(Root);
	if (bReset || bStatusDirty) WritePendingQuestion(Root);

	// 当前会话能否归档既取决于会话列表，也取决于回合状态
	if (bReset || bSessionsDirty || bStatusDirty) WriteSessions(Root);
	if (bReset || bRunLogDirty) WriteRunLog(Root);
	if (bReset || bMcpDirty) WriteMcp(Root);

	// 队列能否继续、失败的回合能否重试同样取决于回合状态
	if (bReset || bQueueDirty || bStatusDirty) WriteQueue(Root);

	if (PendingEvents.Num() > 0)
	{
		TArray<TSharedPtr<FJsonValue>> Events;
		for (const FString& Event : PendingEvents)
		{
			Events.Add(MakeShared<FJsonValueString>(Event));
		}
		Root->SetArrayField(ShuttlegentEditorWebField::Events, Events);
	}

	// 放在会话切换的同一条消息里，页面先切换会话再填入原文
	if (bDraftPending) Root->SetStringField(ShuttlegentEditorWebField::RestoreDraft, PendingDraft);

	bResetPending = false;
	bStatusDirty = false;
	bEntriesDirty = false;
	bSessionsDirty = false;
	bRunLogDirty = false;
	bQueueDirty = false;
	bMcpDirty = false;
	bDraftPending = false;
	PendingDraft.Reset();
	PendingEvents.Reset();
	if (Root->Values.IsEmpty()) return;

	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	FJsonSerializer::Serialize(Root, Writer);
	Push(Json);
}

void FShuttlegentWebProtocol::WriteStatus(const TSharedRef<FJsonObject>& Root) const
{
	const EShuttlegentAgentState State = Model->GetState();
	const bool bConnected = !Model->IsRuntimeLost() && State == EShuttlegentAgentState::Ready;
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();

	Root->SetStringField(ShuttlegentEditorWebField::Status, FShuttlegentWebProtocolPrivate::StatusName(Model->GetStatus()));
	Root->SetBoolField(ShuttlegentEditorWebField::Connected, bConnected);
	Root->SetBoolField(ShuttlegentEditorWebField::Prompting, Model->IsPrompting());
	Root->SetBoolField(ShuttlegentEditorWebField::Stopping, Model->IsStopping());
	Root->SetBoolField(ShuttlegentEditorWebField::Busy, Model->IsBusy());
	Root->SetBoolField(ShuttlegentEditorWebField::RuntimeLost, Model->IsRuntimeLost());
	Root->SetBoolField(ShuttlegentEditorWebField::AnimateMark, IsValid(Settings) && Settings->bAnimateMark);
	Root->SetNumberField(ShuttlegentEditorWebField::FontSize, IsValid(Settings) ? Settings->FontSize : ShuttlegentAppearanceConstant::DefaultFontSize);
	Root->SetBoolField(ShuttlegentEditorWebField::WideLayout, IsValid(Settings) && Settings->bWideLayout);
	Root->SetNumberField(ShuttlegentEditorWebField::UsedTokens, static_cast<double>(Model->GetUsedTokens()));
	Root->SetNumberField(ShuttlegentEditorWebField::ContextSize, static_cast<double>(Model->GetContextSize()));

	const FShuttlegentContextBreakdown& Breakdown = Model->GetContextBreakdown();
	if (Breakdown.IsEmpty())
	{
		Root->SetField(ShuttlegentEditorWebField::ContextBreakdown, MakeShared<FJsonValueNull>());
	}
	else
	{
		const TSharedRef<FJsonObject> BreakdownObject = MakeShared<FJsonObject>();
		BreakdownObject->SetNumberField(ShuttlegentEditorWebField::SystemTokens, static_cast<double>(Breakdown.SystemTokens));
		BreakdownObject->SetNumberField(ShuttlegentEditorWebField::ToolsTokens, static_cast<double>(Breakdown.ToolsTokens));
		BreakdownObject->SetNumberField(ShuttlegentEditorWebField::MessageTokens, static_cast<double>(Breakdown.MessageTokens));
		Root->SetObjectField(ShuttlegentEditorWebField::ContextBreakdown, BreakdownObject);
	}
	Root->SetField(ShuttlegentEditorWebField::StartIssue, FShuttlegentWebProtocolPrivate::MakeStartIssue(Model->GetStartIssue(), Model->GetStartIssueDetail()));
	Root->SetBoolField(ShuttlegentEditorWebField::ResumeFailed, Model->IsResumeFailed());

	FText HistoryReason;
	Root->SetBoolField(ShuttlegentEditorWebField::CanEditHistory, Model->CanEditHistory(HistoryReason));
	Root->SetStringField(ShuttlegentEditorWebField::EditHistoryReason, HistoryReason.ToString());
	Root->SetBoolField(ShuttlegentEditorWebField::NeedsVerify, Model->NeedsVerify());
}

void FShuttlegentWebProtocol::WritePaneWidths(const TSharedRef<FJsonObject>& Root) const
{
	int32 SidebarWidth = 0;
	int32 InspectorWidth = 0;
	GConfig->GetInt(ShuttlegentEditorWeb::ConfigSection, ShuttlegentEditorWeb::ConfigKeySidebarWidth, SidebarWidth, GEditorPerProjectIni);
	GConfig->GetInt(ShuttlegentEditorWeb::ConfigSection, ShuttlegentEditorWeb::ConfigKeyInspectorWidth, InspectorWidth, GEditorPerProjectIni);

	Root->SetNumberField(ShuttlegentEditorWebField::SidebarWidth, SidebarWidth);
	Root->SetNumberField(ShuttlegentEditorWebField::InspectorWidth, InspectorWidth);
}

void FShuttlegentWebProtocol::WriteConfig(const TSharedRef<FJsonObject>& Root) const
{
	const TSharedRef<FJsonObject> Config = MakeShared<FJsonObject>();
	for (const TCHAR* ConfigId : { ShuttlegentEditorConfigId::Model, ShuttlegentEditorConfigId::Reasoning })
	{
		TArray<FShuttlegentConfigChoice> Choices;
		FString CurrentValue;
		Model->GetConfigChoices(ConfigId, Choices, CurrentValue);

		TArray<TSharedPtr<FJsonValue>> ChoiceValues;
		for (const FShuttlegentConfigChoice& Choice : Choices)
		{
			const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(ShuttlegentEditorWebField::Value, Choice.Value);
			Item->SetStringField(ShuttlegentEditorWebField::Name, Choice.Name);
			ChoiceValues.Add(MakeShared<FJsonValueObject>(Item));
		}

		const TSharedRef<FJsonObject> Option = MakeShared<FJsonObject>();
		Option->SetStringField(ShuttlegentEditorWebField::Current, CurrentValue);
		Option->SetArrayField(ShuttlegentEditorWebField::Choices, ChoiceValues);
		Config->SetObjectField(ConfigId, Option);
	}
	Root->SetObjectField(ShuttlegentEditorWebField::Config, Config);
}

void FShuttlegentWebProtocol::WriteTemplates(const TSharedRef<FJsonObject>& Root) const
{
	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FShuttlegentTemplate& Template : FShuttlegentTemplateLibrary::Load())
	{
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(ShuttlegentEditorWebField::Name, Template.Name);
		Item->SetStringField(ShuttlegentEditorWebField::TemplateDescription, Template.Description);
		Item->SetStringField(ShuttlegentEditorWebField::TemplateBody, Template.Body);
		Items.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(ShuttlegentEditorWebField::Templates, Items);
}

void FShuttlegentWebProtocol::WriteSessions(const TSharedRef<FJsonObject>& Root) const
{
	TArray<TSharedPtr<FJsonValue>> Active;
	for (const FShuttlegentSessionRecord& Record : Model->GetSessions(false))
	{
		Active.Add(FShuttlegentWebProtocolPrivate::MakeSession(Record, *Model));
	}

	TArray<TSharedPtr<FJsonValue>> Archived;
	for (const FShuttlegentSessionRecord& Record : Model->GetSessions(true))
	{
		Archived.Add(FShuttlegentWebProtocolPrivate::MakeSession(Record, *Model));
	}

	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	const bool bWorkspaceWrite = IsValid(Settings) && Settings->bAllowWorkspaceWrite;

	Root->SetArrayField(ShuttlegentEditorWebField::Sessions, Active);
	Root->SetArrayField(ShuttlegentEditorWebField::Archived, Archived);
	Root->SetStringField(ShuttlegentEditorWebField::CurrentSessionId, Model->GetCurrentSessionId());
	Root->SetStringField(ShuttlegentEditorWebField::CurrentTitle, Model->GetCurrentTitle());
	Root->SetBoolField(ShuttlegentEditorWebField::CanArchiveCurrent, Model->CanArchive(Model->GetCurrentSessionId()));
	Root->SetNumberField(ShuttlegentEditorWebField::RenameMaxChars, ShuttlegentSessionConstant::RenameMaxChars);
	Root->SetStringField(ShuttlegentEditorWebField::PermissionMode, bWorkspaceWrite ? ShuttlegentEditorWebField::PermissionWorkspaceWrite : ShuttlegentEditorWebField::PermissionReadOnly);
	Root->SetBoolField(ShuttlegentEditorWebField::ShellEnabled, IsValid(Settings) && Settings->bAllowShell);
}

void FShuttlegentWebProtocol::WriteRunLog(const TSharedRef<FJsonObject>& Root) const
{
	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FShuttlegentRunLogEntry& Entry : Model->GetRunLog())
	{
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetNumberField(ShuttlegentEditorWebField::Time, FShuttlegentWebProtocolPrivate::UnixMilliseconds(Entry.Time));
		Item->SetStringField(ShuttlegentEditorWebField::Text, Entry.Text.ToString());
		Item->SetBoolField(ShuttlegentEditorWebField::Warning, Entry.bWarning);
		Items.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(ShuttlegentEditorWebField::RunLog, Items);
}

void FShuttlegentWebProtocol::WriteMcp(const TSharedRef<FJsonObject>& Root) const
{
	const FShuttlegentMcpServerModule& McpServer = FShuttlegentMcpServerModule::Get();

	TArray<TSharedRef<IShuttlegentTool>> Tools = McpServer.GetRegisteredTools();
	Tools.Sort([](const TSharedRef<IShuttlegentTool>& Left, const TSharedRef<IShuttlegentTool>& Right) { return Left->GetDefinition().Name < Right->GetDefinition().Name; });

	TArray<TSharedPtr<FJsonValue>> ToolValues;
	for (const TSharedRef<IShuttlegentTool>& Tool : Tools)
	{
		const FShuttlegentToolDefinition& Definition = Tool->GetDefinition();

		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(ShuttlegentEditorWebField::Name, Definition.Name);
		Item->SetBoolField(ShuttlegentEditorWebField::ReadOnly, Definition.bReadOnly);
		Item->SetBoolField(ShuttlegentEditorWebField::Enabled, FShuttlegentProjectConfig::IsToolEnabled(Definition.Name));
		ToolValues.Add(MakeShared<FJsonValueObject>(Item));
	}

	const TSharedRef<FJsonObject> Endpoint = MakeShared<FJsonObject>();
	Endpoint->SetStringField(ShuttlegentEditorWebField::Name, McpServer.GetEndpointName());
	Endpoint->SetBoolField(ShuttlegentEditorWebField::Listening, McpServer.IsEndpointListening());
	Endpoint->SetNumberField(ShuttlegentEditorWebField::Port, McpServer.GetEndpointPort());
	Endpoint->SetArrayField(ShuttlegentEditorWebField::Tools, ToolValues);

	// 引擎 Toolset 通过端点的 Toolset 工具提供给模型；注册表未就绪时为空
	TArray<TSharedPtr<FJsonValue>> ToolsetValues;
	for (const FShuttlegentToolsetInfo& Toolset : FShuttlegentToolsets::GetToolsets())
	{
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(ShuttlegentEditorWebField::Name, Toolset.Name);
		Item->SetNumberField(ShuttlegentEditorWebField::ToolCount, Toolset.ToolCount);
		Item->SetBoolField(ShuttlegentEditorWebField::Blocked, Toolset.bBlockedByProject);
		Item->SetBoolField(ShuttlegentEditorWebField::Enabled, !Toolset.bDisabledByUser);
		ToolsetValues.Add(MakeShared<FJsonValueObject>(Item));
	}

	TArray<TSharedPtr<FJsonValue>> ServerValues;
	for (const FShuttlegentExternalMcpState& Server : McpServer.GetExternalServers())
	{
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(ShuttlegentEditorWebField::Name, Server.Name);
		Item->SetBoolField(ShuttlegentEditorWebField::Stdio, Server.bStdio);
		Item->SetStringField(ShuttlegentEditorWebField::Issue, FShuttlegentWebProtocolPrivate::ExternalIssueName(Server.Issue));
		Item->SetStringField(ShuttlegentEditorWebField::Detail, Server.Detail);
		ServerValues.Add(MakeShared<FJsonValueObject>(Item));
	}

	const TSharedRef<FJsonObject> Mcp = MakeShared<FJsonObject>();
	Mcp->SetObjectField(ShuttlegentEditorWebField::Endpoint, Endpoint);
	Mcp->SetArrayField(ShuttlegentEditorWebField::Toolsets, ToolsetValues);
	Mcp->SetArrayField(ShuttlegentEditorWebField::Servers, ServerValues);
	Root->SetObjectField(ShuttlegentEditorWebField::Mcp, Mcp);
}

void FShuttlegentWebProtocol::WriteQueue(const TSharedRef<FJsonObject>& Root) const
{
	const FShuttlegentPromptQueue Queue = Model->GetCurrentQueue();

	TArray<TSharedPtr<FJsonValue>> Items;
	for (const FShuttlegentQueuedPrompt& Item : Queue.Items)
	{
		const TSharedRef<FJsonObject> Value = MakeShared<FJsonObject>();
		Value->SetNumberField(ShuttlegentEditorWebField::Id, Item.Id);
		Value->SetStringField(ShuttlegentEditorWebField::Text, Item.Text);
		Value->SetArrayField(ShuttlegentEditorWebField::References, FShuttlegentWebJson::MakeReferences(Item.References));
		Items.Add(MakeShared<FJsonValueObject>(Value));
	}

	const TSharedRef<FJsonObject> QueueObject = MakeShared<FJsonObject>();
	QueueObject->SetBoolField(ShuttlegentEditorWebField::Paused, Queue.bPaused);
	QueueObject->SetArrayField(ShuttlegentEditorWebField::Items, Items);
	Root->SetObjectField(ShuttlegentEditorWebField::Queue, QueueObject);
	Root->SetBoolField(ShuttlegentEditorWebField::CanRetry, Model->CanRetry());
}

void FShuttlegentWebProtocol::WritePendingWrite(const TSharedRef<FJsonObject>& Root) const
{
	const FShuttlegentWriteRequest* Request = Model->GetPendingWrite();
	if (Request == nullptr)
	{
		Root->SetField(ShuttlegentEditorWebField::PendingWrite, MakeShared<FJsonValueNull>());
		return;
	}

	const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
	Item->SetNumberField(ShuttlegentEditorWebField::Id, Request->RequestId);
	Item->SetStringField(ShuttlegentEditorWebField::Tool, Request->ToolName);
	Item->SetStringField(ShuttlegentEditorWebField::Action, Request->Action);
	Item->SetStringField(ShuttlegentEditorWebField::Level, Request->Level);
	Item->SetBoolField(ShuttlegentEditorWebField::Undoable, Request->bUndoable);
	Item->SetBoolField(ShuttlegentEditorWebField::Destructive, Request->bDestructive);
	Item->SetArrayField(ShuttlegentEditorWebField::Objects, FShuttlegentWebProtocolPrivate::MakeWriteObjects(Request->Objects));
	Item->SetStringField(ShuttlegentEditorWebField::Raw, Request->RawArguments);
	Item->SetNumberField(ShuttlegentEditorWebField::ExpiresAt, FShuttlegentWebProtocolPrivate::UnixMilliseconds(Request->ExpiresAtUtc));
	Root->SetObjectField(ShuttlegentEditorWebField::PendingWrite, Item);
}

void FShuttlegentWebProtocol::WritePendingQuestion(const TSharedRef<FJsonObject>& Root) const
{
	const FShuttlegentQuestion* Question = Model->GetPendingQuestion();
	if (Question == nullptr)
	{
		Root->SetField(ShuttlegentEditorWebField::PendingQuestion, MakeShared<FJsonValueNull>());
		return;
	}

	TArray<TSharedPtr<FJsonValue>> Options;
	for (const FString& Option : Question->Options)
	{
		Options.Add(MakeShared<FJsonValueString>(Option));
	}

	const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
	Item->SetNumberField(ShuttlegentEditorWebField::Id, Question->QuestionId);
	Item->SetStringField(ShuttlegentEditorWebField::Question, Question->Question);
	Item->SetArrayField(ShuttlegentEditorWebField::Options, Options);
	Item->SetBoolField(ShuttlegentEditorWebField::AllowOther, Question->bAllowOther);
	Item->SetNumberField(ShuttlegentEditorWebField::ExpiresAt, FShuttlegentWebProtocolPrivate::UnixMilliseconds(Question->ExpiresAtUtc));
	Root->SetObjectField(ShuttlegentEditorWebField::PendingQuestion, Item);
}

void FShuttlegentWebProtocol::WritePendingPermission(const TSharedRef<FJsonObject>& Root) const
{
	const TSharedPtr<FShuttlegentChatEntry> Permission = Model->GetPendingPermission();
	if (!Permission.IsValid())
	{
		Root->SetField(ShuttlegentEditorWebField::PendingPermission, MakeShared<FJsonValueNull>());
		return;
	}

	// 目标与内容取自该请求所涉及的工具调用
	const TSharedPtr<FShuttlegentChatEntry> ToolCall = Model->FindToolCallOf(Permission);
	const FString Input = ToolCall.IsValid() ? ToolCall->Input : FString();

	const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
	Item->SetNumberField(ShuttlegentEditorWebField::Id, Permission->Id);
	Item->SetStringField(ShuttlegentEditorWebField::DisplayName, FShuttlegentToolPresentation::GetDisplayName(Permission->Title).ToString());
	Item->SetStringField(ShuttlegentEditorWebField::Target, FShuttlegentToolPresentation::GetSummary(Input));
	Item->SetStringField(ShuttlegentEditorWebField::Content, FShuttlegentToolPresentation::GetContentPreview(Input));
	Item->SetStringField(ShuttlegentEditorWebField::Detail, Permission->Detail);
	Root->SetObjectField(ShuttlegentEditorWebField::PendingPermission, Item);
}

void FShuttlegentWebProtocol::WriteEntries(const TSharedRef<FJsonObject>& Root, bool bAll)
{
	const TArray<TSharedPtr<FShuttlegentChatEntry>>& Entries = Model->GetEntries();

	TArray<int32> Order;
	TArray<TSharedPtr<FJsonValue>> Changed;
	TMap<int32, uint32> Fingerprints;
	for (const TSharedPtr<FShuttlegentChatEntry>& Entry : Entries)
	{
		if (!Entry.IsValid()) continue;

		uint32 Fingerprint = FShuttlegentWebProtocolPrivate::Fingerprint(*Entry);

		// 写入能否撤销随编辑器撤销缓冲区变化，而不随条目变化
		if (Entry->Kind == EShuttlegentChatEntryKind::WriteResult)
		{
			FText Reason;
			Fingerprint = HashCombine(Fingerprint, GetTypeHash(Model->CanUndoWrite(*Entry, Reason)));
			Fingerprint = HashCombine(Fingerprint, GetTypeHash(Entry->bUndone));
		}
		const uint32* Sent = SentFingerprints.Find(Entry->Id);
		if (bAll || Sent == nullptr || *Sent != Fingerprint) Changed.Add(MakeShared<FJsonValueObject>(MakeEntry(*Entry)));

		Fingerprints.Add(Entry->Id, Fingerprint);
		Order.Add(Entry->Id);
	}

	if (Changed.Num() > 0) Root->SetArrayField(ShuttlegentEditorWebField::Entries, Changed);
	if (bAll || Order != SentOrder)
	{
		TArray<TSharedPtr<FJsonValue>> OrderValues;
		for (const int32 Id : Order)
		{
			OrderValues.Add(MakeShared<FJsonValueNumber>(Id));
		}
		Root->SetArrayField(ShuttlegentEditorWebField::Order, OrderValues);
	}

	SentFingerprints = MoveTemp(Fingerprints);
	SentOrder = MoveTemp(Order);
}

TSharedRef<FJsonObject> FShuttlegentWebProtocol::MakeEntry(const FShuttlegentChatEntry& Entry) const
{
	const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
	Item->SetNumberField(ShuttlegentEditorWebField::Id, Entry.Id);
	Item->SetStringField(ShuttlegentEditorWebField::Kind, FShuttlegentWebProtocolPrivate::KindName(Entry.Kind));
	Item->SetBoolField(ShuttlegentEditorWebField::Error, Entry.bError);

	// 用户提示显示其文本，并在下方列出其携带的引用
	if (Entry.Kind == EShuttlegentChatEntryKind::User)
	{
		FString Text;
		TArray<FString> ReferenceLines;
		FShuttlegentReferencePrompt::Split(Entry.Text, Text, ReferenceLines);

		TArray<TSharedPtr<FJsonValue>> Lines;
		for (const FString& Line : ReferenceLines)
		{
			Lines.Add(MakeShared<FJsonValueString>(Line));
		}
		Item->SetStringField(ShuttlegentEditorWebField::Text, Text);
		Item->SetArrayField(ShuttlegentEditorWebField::References, Lines);
		if (Entry.TimeUtc.GetTicks() > 0) Item->SetNumberField(ShuttlegentEditorWebField::Time, FShuttlegentWebProtocolPrivate::UnixMilliseconds(Entry.TimeUtc));
	}
	else
	{
		Item->SetStringField(ShuttlegentEditorWebField::Text, Entry.Text);
	}

	if (Entry.Kind == EShuttlegentChatEntryKind::Assistant)
	{
		Item->SetStringField(ShuttlegentEditorWebField::Thought, Entry.Thought);
		Item->SetNumberField(ShuttlegentEditorWebField::ThoughtSeconds, FShuttlegentWebProtocolPrivate::ThoughtSeconds(Entry));
	}

	if (Entry.Kind == EShuttlegentChatEntryKind::ToolCall)
	{
		const bool bFailed = Entry.Status == ShuttlegentEditorTool::StatusFailed;
		Item->SetStringField(ShuttlegentEditorWebField::Tool, Entry.Title);
		Item->SetStringField(ShuttlegentEditorWebField::DisplayName, FShuttlegentToolPresentation::GetDisplayName(Entry.Title).ToString());
		Item->SetStringField(ShuttlegentEditorWebField::Summary, FShuttlegentToolPresentation::GetSummary(Entry.Input));
		Item->SetStringField(ShuttlegentEditorWebField::ToolStatus, Entry.Status);
		Item->SetBoolField(ShuttlegentEditorWebField::ReadOnly, FShuttlegentToolPresentation::IsReadOnly(Entry.Title));
		Item->SetStringField(ShuttlegentEditorWebField::FailureReason, bFailed ? FShuttlegentToolPresentation::GetFailureReason(Entry.Result) : FString());
		Item->SetStringField(ShuttlegentEditorWebField::Input, Entry.Input);
		Item->SetStringField(ShuttlegentEditorWebField::Result, Entry.Result);
	}

	if (Entry.Kind == EShuttlegentChatEntryKind::WriteResult)
	{
		FText Reason;
		const bool bCanUndo = Model->CanUndoWrite(Entry, Reason);
		Item->SetStringField(ShuttlegentEditorWebField::Action, Entry.WriteResult.Action);
		Item->SetBoolField(ShuttlegentEditorWebField::Undoable, Entry.WriteResult.bUndoable);
		Item->SetArrayField(ShuttlegentEditorWebField::Objects, FShuttlegentWebProtocolPrivate::MakeWriteObjects(Entry.WriteResult.Objects));
		Item->SetBoolField(ShuttlegentEditorWebField::CanUndo, bCanUndo);
		Item->SetStringField(ShuttlegentEditorWebField::UndoReason, Reason.ToString());
		Item->SetBoolField(ShuttlegentEditorWebField::Undone, Entry.bUndone);
	}

	// 由于实际执行情况未知，显示已确认的内容；用户核对对象后标记为已答复
	if (Entry.Kind == EShuttlegentChatEntryKind::WriteUnknown)
	{
		Item->SetStringField(ShuttlegentEditorWebField::Action, Entry.WriteResult.Action);
		Item->SetArrayField(ShuttlegentEditorWebField::Objects, FShuttlegentWebProtocolPrivate::MakeWriteObjects(Entry.WriteResult.Objects));
		Item->SetBoolField(ShuttlegentEditorWebField::Answered, Entry.bAnswered);
	}

	if (Entry.Kind == EShuttlegentChatEntryKind::Permission)
	{
		Item->SetStringField(ShuttlegentEditorWebField::DisplayName, FShuttlegentToolPresentation::GetDisplayName(Entry.Title).ToString());
		Item->SetBoolField(ShuttlegentEditorWebField::Answered, Entry.bAnswered);
		Item->SetBoolField(ShuttlegentEditorWebField::Allowed, Entry.bAllowed);
	}
	return Item;
}

void FShuttlegentWebProtocol::HandleStatusChanged()
{
	bStatusDirty = true;
}

void FShuttlegentWebProtocol::HandleEntriesChanged()
{
	bEntriesDirty = true;
}

void FShuttlegentWebProtocol::HandleSessionsChanged()
{
	bSessionsDirty = true;
}

void FShuttlegentWebProtocol::HandleRunLogChanged()
{
	bRunLogDirty = true;
}

void FShuttlegentWebProtocol::HandleQueueChanged()
{
	bQueueDirty = true;
}

void FShuttlegentWebProtocol::HandleTurnFinished(bool bCompleted)
{
	bStatusDirty = true;

	// 自上次查看以来，引擎 Toolset 可能已就绪
	bMcpDirty = true;
	PendingEvents.Add(bCompleted ? ShuttlegentEditorWebField::EventTurnCompleted : ShuttlegentEditorWebField::EventTurnStopped);
}

void FShuttlegentWebProtocol::HandleSettingChanged(UObject* Settings, FPropertyChangedEvent& Event)
{
	bStatusDirty = true;

	// 工具与 Toolset 开关属于个人设置
	bMcpDirty = true;
}

void FShuttlegentWebProtocol::HandleProjectSettingChanged(UObject* Settings, FPropertyChangedEvent& Event)
{
	bMcpDirty = true;
}

void FShuttlegentWebProtocol::HandleDraftRestored(const FString& Text)
{
	PendingDraft = Text;
	bDraftPending = true;
}
