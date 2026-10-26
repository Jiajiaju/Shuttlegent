#include "Web/ShuttlegentWebBridge.h"

void UShuttlegentWebBridge::Ready()
{
	if (OnReady) OnReady();
}

void UShuttlegentWebBridge::Submit(const FString& Text, const FString& ReferencesJson)
{
	if (OnSubmit) OnSubmit(Text, ReferencesJson);
}

void UShuttlegentWebBridge::Cancel()
{
	if (OnCancel) OnCancel();
}

void UShuttlegentWebBridge::NewConversation()
{
	if (OnNewConversation) OnNewConversation();
}

void UShuttlegentWebBridge::Reconnect()
{
	if (OnReconnect) OnReconnect();
}

void UShuttlegentWebBridge::SetConfig(const FString& ConfigId, const FString& Value)
{
	if (OnSetConfig) OnSetConfig(ConfigId, Value);
}

void UShuttlegentWebBridge::CopyText(const FString& Text)
{
	if (OnCopyText) OnCopyText(Text);
}

void UShuttlegentWebBridge::ToggleContentBrowser()
{
	if (OnToggleContentBrowser) OnToggleContentBrowser();
}

void UShuttlegentWebBridge::OpenSession(const FString& SessionId)
{
	if (OnOpenSession) OnOpenSession(SessionId);
}

void UShuttlegentWebBridge::RenameSession(const FString& SessionId, const FString& Title)
{
	if (OnRenameSession) OnRenameSession(SessionId, Title);
}

void UShuttlegentWebBridge::ArchiveSession(const FString& SessionId)
{
	if (OnArchiveSession) OnArchiveSession(SessionId);
}

void UShuttlegentWebBridge::RestoreSession(const FString& SessionId)
{
	if (OnRestoreSession) OnRestoreSession(SessionId);
}

void UShuttlegentWebBridge::DeleteSession(const FString& SessionId)
{
	if (OnDeleteSession) OnDeleteSession(SessionId);
}

void UShuttlegentWebBridge::SetPaneWidths(int32 SidebarWidth, int32 InspectorWidth)
{
	if (OnSetPaneWidths) OnSetPaneWidths(SidebarWidth, InspectorWidth);
}

void UShuttlegentWebBridge::AnswerPermission(int32 EntryId, bool bAllow)
{
	if (OnAnswerPermission) OnAnswerPermission(EntryId, bAllow);
}

void UShuttlegentWebBridge::OpenSettings()
{
	if (OnOpenSettings) OnOpenSettings();
}

void UShuttlegentWebBridge::QueryReferences(const FString& Query)
{
	if (OnQueryReferences) OnQueryReferences(Query);
}

void UShuttlegentWebBridge::WatchReferences(const FString& ReferencesJson)
{
	if (OnWatchReferences) OnWatchReferences(ReferencesJson);
}

void UShuttlegentWebBridge::RemoveQueued(int32 ItemId)
{
	if (OnRemoveQueued) OnRemoveQueued(ItemId);
}

void UShuttlegentWebBridge::TakeQueued(int32 ItemId, const FString& DraftText, const FString& DraftReferencesJson)
{
	if (OnTakeQueued) OnTakeQueued(ItemId, DraftText, DraftReferencesJson);
}

void UShuttlegentWebBridge::ResumeQueue()
{
	if (OnResumeQueue) OnResumeQueue();
}

void UShuttlegentWebBridge::Retry()
{
	if (OnRetry) OnRetry();
}

void UShuttlegentWebBridge::AnswerWrite(int32 RequestId, bool bApprove)
{
	if (OnAnswerWrite) OnAnswerWrite(RequestId, bApprove);
}

void UShuttlegentWebBridge::UndoWrite(int32 EntryId)
{
	if (OnUndoWrite) OnUndoWrite(EntryId);
}

void UShuttlegentWebBridge::RetryStart()
{
	if (OnRetryStart) OnRetryStart();
}

void UShuttlegentWebBridge::RetryResume()
{
	if (OnRetryResume) OnRetryResume();
}

void UShuttlegentWebBridge::AcknowledgeWrite(int32 EntryId)
{
	if (OnAcknowledgeWrite) OnAcknowledgeWrite(EntryId);
}

void UShuttlegentWebBridge::OpenPluginFolder()
{
	if (OnOpenPluginFolder) OnOpenPluginFolder();
}
