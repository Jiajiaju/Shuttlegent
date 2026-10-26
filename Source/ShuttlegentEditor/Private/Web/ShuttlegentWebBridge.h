#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "ShuttlegentWebBridge.generated.h"

/**
 * Commands the panel page sends to the editor. The page calls window.ue.bridge.<function> (names are lower-cased by the binding).
 * Glue only: SShuttlegentWebPanel keeps it alive with a strong pointer, binds the callbacks and clears them before releasing it.
 */
UCLASS()
class UShuttlegentWebBridge : public UObject
{
	GENERATED_BODY()

public:
	TFunction<void()> OnReady;

	TFunction<void(const FString&, const FString&)> OnSubmit;

	TFunction<void()> OnCancel;

	TFunction<void()> OnNewConversation;

	TFunction<void()> OnReconnect;

	TFunction<void(const FString&, const FString&)> OnSetConfig;

	TFunction<void(const FString&)> OnCopyText;

	TFunction<void()> OnToggleContentBrowser;

	TFunction<void(const FString&)> OnOpenSession;

	TFunction<void(const FString&, const FString&)> OnRenameSession;

	TFunction<void(const FString&)> OnArchiveSession;

	TFunction<void(const FString&)> OnRestoreSession;

	TFunction<void(const FString&)> OnDeleteSession;

	TFunction<void(int32, int32)> OnSetPaneWidths;

	TFunction<void(int32, bool)> OnAnswerPermission;

	TFunction<void()> OnOpenSettings;

	TFunction<void(const FString&)> OnQueryReferences;

	TFunction<void(const FString&)> OnWatchReferences;

	TFunction<void(int32)> OnRemoveQueued;

	TFunction<void(int32, const FString&, const FString&)> OnTakeQueued;

	TFunction<void()> OnResumeQueue;

	TFunction<void()> OnRetry;

	TFunction<void(int32, bool)> OnAnswerWrite;

	TFunction<void(int32)> OnUndoWrite;

	TFunction<void()> OnRetryStart;

	TFunction<void()> OnRetryResume;

	TFunction<void(int32)> OnAcknowledgeWrite;

	TFunction<void()> OnOpenPluginFolder;

public:
	/** The page finished initializing and wants the full state; called once per document load. */
	UFUNCTION()
	void Ready();

	/** Sends the draft as a prompt with its reference chips (a JSON array); the answer comes back through onSubmitResult. */
	UFUNCTION()
	void Submit(const FString& Text, const FString& ReferencesJson);

	/** Stops the running turn. */
	UFUNCTION()
	void Cancel();

	UFUNCTION()
	void NewConversation();

	/** Restarts a lost runtime and resumes the last session. */
	UFUNCTION()
	void Reconnect();

	/** Changes a session config option ("model", "reasoning_effort"). */
	UFUNCTION()
	void SetConfig(const FString& ConfigId, const FString& Value);

	/** Puts text on the system clipboard (the page's clipboard API is not available for file:// pages). */
	UFUNCTION()
	void CopyText(const FString& Text);

	/** The page's add-reference button: opens or closes the Content Browser drawer. */
	UFUNCTION()
	void ToggleContentBrowser();

	/** Shows a stored session's transcript and resumes it in the runtime. */
	UFUNCTION()
	void OpenSession(const FString& SessionId);

	/** Renames a stored session; the editor trims the name and rejects an empty or too long one. */
	UFUNCTION()
	void RenameSession(const FString& SessionId, const FString& Title);

	/** Hides a session from the list; a running or permission-waiting session is refused. */
	UFUNCTION()
	void ArchiveSession(const FString& SessionId);

	UFUNCTION()
	void RestoreSession(const FString& SessionId);

	/** Deletes a session's record and transcript for good; a running or permission-waiting session is refused. */
	UFUNCTION()
	void DeleteSession(const FString& SessionId);

	/** A docked pane was resized: remembers the sidebar and inspector widths in CSS pixels for the next page load. */
	UFUNCTION()
	void SetPaneWidths(int32 SidebarWidth, int32 InspectorWidth);

	/** Answers the runtime permission request shown by the decision card. */
	UFUNCTION()
	void AnswerPermission(int32 EntryId, bool bAllow);

	/** Opens the Shuttlegent page of Editor Preferences. */
	UFUNCTION()
	void OpenSettings();

	/** The @ menu's query changed; candidates come back through onReferenceResults. */
	UFUNCTION()
	void QueryReferences(const FString& Query);

	/** The draft's reference chips (a JSON array) changed; their existence comes back through onReferenceValidity, now and whenever it changes. */
	UFUNCTION()
	void WatchReferences(const FString& ReferencesJson);

	/** Drops a queued prompt; a running turn is not affected. */
	UFUNCTION()
	void RemoveQueued(int32 ItemId);

	/** Takes a queued prompt back into the draft; the current draft (text and a JSON array of references) takes its place. The item comes back through onQueueItemTaken. */
	UFUNCTION()
	void TakeQueued(int32 ItemId, const FString& DraftText, const FString& DraftReferencesJson);

	/** Continues a paused queue. */
	UFUNCTION()
	void ResumeQueue();

	/** Sends the failed turn's prompt again. */
	UFUNCTION()
	void Retry();

	/** Confirms or refuses the editor write shown by the confirmation card. */
	UFUNCTION()
	void AnswerWrite(int32 RequestId, bool bApprove);

	/** "Undo this change" on a write result. */
	UFUNCTION()
	void UndoWrite(int32 EntryId);

	/** "Retry" on the start guide card: starts the runtime again (restarting a running one). */
	UFUNCTION()
	void RetryStart();

	/** "Resume again" on the failed-resume banner. */
	UFUNCTION()
	void RetryResume();

	/** "Verified" on a write whose result is unknown. */
	UFUNCTION()
	void AcknowledgeWrite(int32 EntryId);

	/** "Open plugin folder" on the start guide card, where the .env file and Runtime/ live. */
	UFUNCTION()
	void OpenPluginFolder();
};
