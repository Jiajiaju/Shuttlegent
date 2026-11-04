#include "Chat/FShuttlegentChatView.h"

#include "Chat/ShuttlegentChatModel.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentLog.h"

void FShuttlegentChatView::Initialize(const TSharedRef<FShuttlegentChatModel>& InModel, EShuttlegentViewMode InMode)
{
	Model = InModel;
	Mode = InMode;
	if (DraftViewKey.IsEmpty()) DraftViewKey = ShuttlegentEditorDraft::HubKey;
	Current = Model->CreateConversation();
	NewDraftConversation = Current;
	Model->SetDisplayed(*Current, true, bVisible);
	Model->SetFocused(*Current);

	Model->OnStructureChanged.AddSP(this, &FShuttlegentChatView::HandleStructureChanged);
	Model->OnContentChanged.AddSP(this, &FShuttlegentChatView::HandleContentChanged);
	Model->OnStatusChanged.AddSP(this, &FShuttlegentChatView::HandleStatusChanged);
	Model->OnTurnFinished.AddSP(this, &FShuttlegentChatView::HandleTurnFinished);
	Model->OnSessionsChanged.AddSP(this, &FShuttlegentChatView::HandleSessionsChanged);
	Model->OnRunLogChanged.AddSP(this, &FShuttlegentChatView::HandleRunLogChanged);
	Model->OnQueueChanged.AddSP(this, &FShuttlegentChatView::HandleQueueChanged);
	Model->OnDraftRestored.AddSP(this, &FShuttlegentChatView::HandleDraftRestored);
	Model->OnSessionClosed.AddSP(this, &FShuttlegentChatView::HandleSessionClosed);
	Model->OnForkAdopted.AddSP(this, &FShuttlegentChatView::HandleForkAdopted);
}

void FShuttlegentChatView::Shutdown()
{
	if (!Model.IsValid()) return;

	Model->OnStructureChanged.RemoveAll(this);
	Model->OnContentChanged.RemoveAll(this);
	Model->OnStatusChanged.RemoveAll(this);
	Model->OnTurnFinished.RemoveAll(this);
	Model->OnSessionsChanged.RemoveAll(this);
	Model->OnRunLogChanged.RemoveAll(this);
	Model->OnQueueChanged.RemoveAll(this);
	Model->OnDraftRestored.RemoveAll(this);
	Model->OnSessionClosed.RemoveAll(this);
	Model->OnForkAdopted.RemoveAll(this);

	if (Current.IsValid())
	{
		Model->SetDisplayed(*Current, false, bVisible);
		Model->ReleaseConversation(*Current);
	}
	Current.Reset();
	Model.Reset();
}

void FShuttlegentChatView::SetVisible(bool bInVisible)
{
	if (bVisible == bInVisible) return;

	// 显示计数不变，只把这一份显示从“在后台”换成“在前台”或反过来
	Model->SetDisplayed(*Current, false, bVisible);
	bVisible = bInVisible;
	Model->SetDisplayed(*Current, true, bVisible);
	if (!bVisible) return;

	Model->SetFocused(*Current);
	// 重启后恢复的页签第一次到前台时才恢复会话
	Model->ResumeDeferred(*Current);
}

void FShuttlegentChatView::NewConversation()
{
	Show(Model->CreateConversation());
}

void FShuttlegentChatView::OpenSession(const FString& SessionId, bool bDeferResume)
{
	if (SessionId.IsEmpty() || SessionId == Current->SessionId) return;

	const TSharedPtr<FShuttlegentConversation> Held = Model->HoldConversation(SessionId, bDeferResume);
	if (!Held.IsValid()) return;

	Show(Held.ToSharedRef());
	// 用户主动打开的会话立即恢复，即使它此前由恢复的页签暂缓、所在页签又不在前台
	if (!bDeferResume) Model->ResumeDeferred(*Held);
}

void FShuttlegentChatView::Show(const TSharedRef<FShuttlegentConversation>& Next)
{
	if (Current == Next) return;

	const TSharedPtr<FShuttlegentConversation> Previous = Current;
	Current = Next;
	NewDraftConversation = Next->SessionId.IsEmpty() ? TWeakPtr<FShuttlegentConversation>(Next) : nullptr;
	Model->SetDisplayed(*Next, true, bVisible);
	Model->SetFocused(*Next);
	if (bVisible) Model->ResumeDeferred(*Next);

	if (Previous.IsValid())
	{
		Model->SetDisplayed(*Previous, false, bVisible);
		Model->ReleaseConversation(*Previous);
	}
	BroadcastAll();
}

void FShuttlegentChatView::BroadcastAll()
{
	OnStructureChanged.Broadcast();
	OnStatusChanged.Broadcast();
	OnSessionsChanged.Broadcast();
	OnRunLogChanged.Broadcast();
	OnQueueChanged.Broadcast();
}

EShuttlegentAgentState FShuttlegentChatView::GetState() const
{
	return Model->GetState();
}

bool FShuttlegentChatView::IsRuntimeLost() const
{
	return Model->IsRuntimeLost();
}

EShuttlegentStartIssue FShuttlegentChatView::GetStartIssue() const
{
	return Model->GetStartIssue();
}

const FString& FShuttlegentChatView::GetStartIssueDetail() const
{
	return Model->GetStartIssueDetail();
}

bool FShuttlegentChatView::IsPrompting() const
{
	return Model->IsPrompting(*Current);
}

bool FShuttlegentChatView::IsBusy() const
{
	return Model->IsBusy(*Current);
}

EShuttlegentChatStatus FShuttlegentChatView::GetStatus() const
{
	return Model->GetStatus(*Current);
}

TSharedPtr<FShuttlegentChatEntry> FShuttlegentChatView::GetPendingPermission() const
{
	return Model->GetPendingPermission(*Current);
}

TSharedPtr<FShuttlegentChatEntry> FShuttlegentChatView::FindToolCallOf(const TSharedPtr<FShuttlegentChatEntry>& Permission) const
{
	return Model->FindToolCallOf(*Current, Permission);
}

bool FShuttlegentChatView::CanCompact(FText& OutReason) const
{
	return Model->CanCompact(*Current, OutReason);
}

bool FShuttlegentChatView::CanSetPlanMode(FText& OutReason) const
{
	return Model->CanSetPlanMode(*Current, OutReason);
}

bool FShuttlegentChatView::CanAttachImages(FText& OutReason) const
{
	return Model->CanAttachImages(*Current, OutReason);
}

bool FShuttlegentChatView::CanRetry() const
{
	return Model->CanRetry(*Current);
}

bool FShuttlegentChatView::CanEditHistory(FText& OutReason) const
{
	return Model->CanEditHistory(*Current, OutReason);
}

bool FShuttlegentChatView::CanUndoWrite(const FShuttlegentChatEntry& Entry, FText& OutReason) const
{
	return Model->CanUndoWrite(Entry, OutReason);
}

const FShuttlegentWriteRequest* FShuttlegentChatView::GetPendingWrite() const
{
	return Model->GetPendingWrite(*Current);
}

const FShuttlegentQuestion* FShuttlegentChatView::GetPendingQuestion() const
{
	return Model->GetPendingQuestion(*Current);
}

bool FShuttlegentChatView::GetConfigChoices(const FString& ConfigId, TArray<FShuttlegentConfigChoice>& OutChoices, FString& OutCurrentValue) const
{
	return Model->GetConfigChoices(*Current, ConfigId, OutChoices, OutCurrentValue);
}

bool FShuttlegentChatView::IsAgentLocked() const
{
	return Model->IsAgentLocked(*Current);
}

FString FShuttlegentChatView::GetCurrentTitle() const
{
	return Model->GetTitle(*Current);
}

TArray<FShuttlegentSessionRecord> FShuttlegentChatView::GetSessions(bool bArchived) const
{
	return Model->GetSessions(bArchived);
}

bool FShuttlegentChatView::CanArchive(const FString& SessionId) const
{
	return Model->CanArchive(SessionId);
}

bool FShuttlegentChatView::GetSessionActivity(const FString& SessionId, EShuttlegentChatStatus& OutStatus, int32& OutQueued) const
{
	return Model->GetSessionActivity(SessionId, OutStatus, OutQueued);
}

EShuttlegentAttention FShuttlegentChatView::GetSessionAttention(const FString& SessionId) const
{
	return Model->GetSessionAttention(SessionId);
}

bool FShuttlegentChatView::IsShownElsewhere(const FString& SessionId) const
{
	const int32 ShownHere = SessionId == Current->SessionId ? 1 : 0;
	return Model->GetDisplayCount(SessionId) > ShownHere;
}

EShuttlegentAttention FShuttlegentChatView::GetTabAttention() const
{
	return Mode == EShuttlegentViewMode::Hub ? Model->GetAttentionOutside(*Current) : Model->GetAttention(*Current);
}

void FShuttlegentChatView::CompactContext()
{
	Model->CompactContext(*Current);
}

void FShuttlegentChatView::SetPlanMode(bool bActive)
{
	Model->SetPlanMode(*Current, bActive);
}

void FShuttlegentChatView::AnswerPlanReview(bool bApprove, const FString& Feedback, bool bAcceptEdits)
{
	Model->AnswerPlanReview(*Current, bApprove, Feedback, bAcceptEdits);
}

void FShuttlegentChatView::SetWriteMode(EShuttlegentWriteMode WriteMode)
{
	Model->SetWriteMode(*Current, WriteMode);
}

void FShuttlegentChatView::EnsureRuntime()
{
	Model->EnsureRuntime();
}

void FShuttlegentChatView::RetryStart()
{
	Model->RetryStart(*Current);
}

void FShuttlegentChatView::RetryResume()
{
	Model->RetryResume(*Current);
}

void FShuttlegentChatView::AcknowledgeUnknownWrite(int32 EntryId)
{
	Model->AcknowledgeUnknownWrite(*Current, EntryId);
}

EShuttlegentSubmitResult FShuttlegentChatView::SubmitDraft(const FString& Text, const TArray<FShuttlegentReference>& References, bool bEnterPlanMode, const TArray<FShuttlegentPromptImage>& Images, TArray<FString>& OutInvalidPaths)
{
	Model->SetFocused(*Current);
	return Model->SubmitDraft(*Current, Text, References, bEnterPlanMode, Images, OutInvalidPaths);
}

void FShuttlegentChatView::RemoveQueued(int32 ItemId)
{
	Model->RemoveQueued(*Current, ItemId);
}

bool FShuttlegentChatView::TakeQueued(int32 ItemId, const FString& DraftText, const TArray<FShuttlegentReference>& DraftReferences, FShuttlegentQueuedPrompt& OutItem)
{
	return Model->TakeQueued(*Current, ItemId, DraftText, DraftReferences, OutItem);
}

void FShuttlegentChatView::ResumeQueue()
{
	Model->ResumeQueue(*Current);
}

void FShuttlegentChatView::Retry()
{
	Model->Retry(*Current);
}

void FShuttlegentChatView::AnswerWrite(int32 RequestId, bool bApprove)
{
	Model->AnswerWrite(RequestId, bApprove);
}

void FShuttlegentChatView::UndoWrite(int32 EntryId)
{
	Model->UndoWrite(*Current, EntryId);
}

void FShuttlegentChatView::UndoMemory(int32 EntryId)
{
	Model->UndoMemory(*Current, EntryId);
}

void FShuttlegentChatView::BranchFrom(int32 EntryId)
{
	Model->BranchFrom(*Current, EntryId);
}

void FShuttlegentChatView::RewindTo(int32 EntryId)
{
	Model->RewindTo(*Current, EntryId);
}

void FShuttlegentChatView::AnswerQuestion(int32 QuestionId, const FString& Answer, bool bSkip)
{
	Model->AnswerQuestion(QuestionId, Answer, bSkip);
}

bool FShuttlegentChatView::SubmitChoice(int32 EntryId, const FString& AnswerJson, FText& OutReason)
{
	return Model->SubmitChoice(*Current, EntryId, AnswerJson, OutReason);
}

void FShuttlegentChatView::Cancel()
{
	Model->Cancel(*Current);
}

void FShuttlegentChatView::Reconnect()
{
	Model->Reconnect(*Current);
}

void FShuttlegentChatView::SetPermissionHandlerAttached(bool bAttached)
{
	Model->SetPermissionHandlerAttached(bAttached);
}

void FShuttlegentChatView::AnswerPermission(int32 EntryId, bool bAllow)
{
	Model->AnswerPermission(EntryId, bAllow);
}

void FShuttlegentChatView::SetConfigValue(const FString& ConfigId, const FString& Value)
{
	Model->SetConfigValue(*Current, ConfigId, Value);
}

bool FShuttlegentChatView::RenameSession(const FString& SessionId, const FString& Title)
{
	return Model->RenameSession(SessionId, Title);
}

bool FShuttlegentChatView::ArchiveSession(const FString& SessionId)
{
	return Model->ArchiveSession(SessionId);
}

void FShuttlegentChatView::RestoreSession(const FString& SessionId)
{
	Model->RestoreSession(SessionId);
}

bool FShuttlegentChatView::DeleteSession(const FString& SessionId)
{
	return Model->DeleteSession(SessionId);
}

void FShuttlegentChatView::HandleStructureChanged(const FShuttlegentConversation& Conversation)
{
	if (IsCurrent(Conversation)) OnStructureChanged.Broadcast();
}

void FShuttlegentChatView::HandleContentChanged(const FShuttlegentConversation& Conversation)
{
	if (IsCurrent(Conversation)) OnContentChanged.Broadcast();
}

void FShuttlegentChatView::HandleStatusChanged(const FShuttlegentConversation& Conversation)
{
	if (!IsCurrent(Conversation)) return;

	AdoptNewDraft();
	OnStatusChanged.Broadcast();
}

void FShuttlegentChatView::HandleTurnFinished(const FShuttlegentConversation& Conversation, bool bCompleted)
{
	if (IsCurrent(Conversation)) OnTurnFinished.Broadcast(bCompleted);
}

void FShuttlegentChatView::HandleSessionsChanged()
{
	AdoptNewDraft();
	OnSessionsChanged.Broadcast();
}

TSharedPtr<FJsonObject> FShuttlegentChatView::GetDraft() const
{
	return Model->GetDrafts().GetDraft(MakeDraftKey(Current->SessionId));
}

void FShuttlegentChatView::SaveDraft(const FString& SessionId, const FString& Json)
{
	// 删除会话后页面离开它时还会交来一次草稿，不能让已删除会话的文件重新出现
	if (!SessionId.IsEmpty() && !Model->HasSessionRecord(SessionId)) return;

	if (!Model->GetDrafts().SetDraftJson(MakeDraftKey(SessionId), Json))
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Draft store: the page's draft for '%s' was refused (invalid key or content, %d characters)."), *SessionId, Json.Len());
	}
}

FString FShuttlegentChatView::MakeDraftKey(const FString& SessionId) const
{
	return SessionId.IsEmpty() ? FShuttlegentDraftStore::MakeViewKey(DraftViewKey) : SessionId;
}

void FShuttlegentChatView::AdoptNewDraft()
{
	const TSharedPtr<FShuttlegentConversation> Pending = NewDraftConversation.Pin();
	if (!Pending.IsValid() || Pending != Current || Current->SessionId.IsEmpty()) return;

	NewDraftConversation.Reset();
	Model->GetDrafts().MoveDraft(FShuttlegentDraftStore::MakeViewKey(DraftViewKey), Current->SessionId);
}

void FShuttlegentChatView::HandleRunLogChanged(const FShuttlegentConversation& Conversation)
{
	if (IsCurrent(Conversation)) OnRunLogChanged.Broadcast();
}

void FShuttlegentChatView::HandleQueueChanged(const FShuttlegentConversation& Conversation)
{
	if (IsCurrent(Conversation)) OnQueueChanged.Broadcast();
}

void FShuttlegentChatView::HandleDraftRestored(const FString& SessionId, const FString& Text)
{
	if (SessionId == Current->SessionId) OnDraftRestored.Broadcast(Text);
}

void FShuttlegentChatView::HandleSessionClosed(const FString& SessionId)
{
	if (SessionId == Current->SessionId) NewConversation();
}

void FShuttlegentChatView::HandleForkAdopted(const FString& SourceSessionId, const FString& NewSessionId)
{
	if (SourceSessionId == Current->SessionId) OpenSession(NewSessionId);
}
