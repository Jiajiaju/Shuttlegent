#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"

class FJsonObject;
class FShuttlegentChatModel;
class UObject;
struct FPropertyChangedEvent;
struct FShuttlegentChatEntry;

/**
 * What the editor pushes to the panel page: a full snapshot when the page reports ready, then deltas.
 *
 * Listens to FShuttlegentChatModel and only marks what changed; a core ticker folds every change of one
 * frame into a single JSON message, so streamed chunks never cost one page call each. Entries are resent
 * only when their fingerprint changed since the last message. Game thread only.
 */
class FShuttlegentWebProtocol : public TSharedFromThis<FShuttlegentWebProtocol>
{
private:
	TSharedPtr<FShuttlegentChatModel> Model;

	/** Delivers one JSON message to the page. */
	TFunction<void(const FString&)> Push;

	FTSTicker::FDelegateHandle TickerHandle;
	FDelegateHandle SettingsChangedHandle;
	FDelegateHandle ProjectSettingsChangedHandle;

	/** Fingerprint of each entry as last sent, and the entry order as last sent. */
	TMap<int32, uint32> SentFingerprints;
	TArray<int32> SentOrder;

	/** One-shot events waiting for the next message. */
	TArray<FString> PendingEvents;

	bool bPageReady = false;
	bool bResetPending = false;
	bool bStatusDirty = false;
	bool bEntriesDirty = false;
	bool bSessionsDirty = false;
	bool bRunLogDirty = false;
	bool bQueueDirty = false;
	bool bMcpDirty = false;

public:
	void Initialize(const TSharedRef<FShuttlegentChatModel>& InModel, TFunction<void(const FString&)> InPush);
	void Shutdown();

	/** The page (re)loaded and asked for the full state; nothing is pushed before this. */
	void HandlePageReady();

private:
	bool HandleTick(float DeltaTime);
	void Flush();

	void WriteStatus(const TSharedRef<FJsonObject>& Root) const;
	void WriteConfig(const TSharedRef<FJsonObject>& Root) const;

	/** Slash templates, read from the template directories with every snapshot (so a page reload picks up edits). */
	void WriteTemplates(const TSharedRef<FJsonObject>& Root) const;

	/** Pane widths the user dragged, from the per-user config; sent with the snapshot only. */
	void WritePaneWidths(const TSharedRef<FJsonObject>& Root) const;

	/** Active and archived sessions, the current session and its title, the rename limit and the permission preset. */
	void WriteSessions(const TSharedRef<FJsonObject>& Root) const;
	void WriteRunLog(const TSharedRef<FJsonObject>& Root) const;

	/** The current session's queue and whether the failed turn can be retried. */
	void WriteQueue(const TSharedRef<FJsonObject>& Root) const;

	/** MCP card: the editor endpoint and its tools, engine toolsets, the project's external servers. */
	void WriteMcp(const TSharedRef<FJsonObject>& Root) const;

	/** Oldest unanswered runtime permission request with what it is about, or null. */
	void WritePendingPermission(const TSharedRef<FJsonObject>& Root) const;

	/** Oldest editor write waiting for confirmation, with its object table and raw arguments, or null. */
	void WritePendingWrite(const TSharedRef<FJsonObject>& Root) const;
	void WritePendingQuestion(const TSharedRef<FJsonObject>& Root) const;

	/** Adds the entries whose fingerprint changed (all of them on reset) and the order when it changed. */
	void WriteEntries(const TSharedRef<FJsonObject>& Root, bool bAll);

	TSharedRef<FJsonObject> MakeEntry(const FShuttlegentChatEntry& Entry) const;

	void HandleStatusChanged();
	void HandleEntriesChanged();
	void HandleSessionsChanged();
	void HandleRunLogChanged();
	void HandleQueueChanged();
	void HandleTurnFinished(bool bCompleted);
	void HandleSettingChanged(UObject* Settings, FPropertyChangedEvent& Event);
	void HandleProjectSettingChanged(UObject* Settings, FPropertyChangedEvent& Event);
};
