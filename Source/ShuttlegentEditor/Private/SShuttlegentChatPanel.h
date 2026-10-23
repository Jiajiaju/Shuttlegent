#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

#include "Chat/FShuttlegentChatEntry.h"
#include "Chat/FShuttlegentConfigChoice.h"

class FShuttlegentChatModel;
class SEditableTextBox;
class SShuttlegentMark;
class SScrollBox;
class SVerticalBox;

/** Shuttlegent chat panel (Window > Shuttlegent), styled after VI 1.1's dark editor panel. */
class SShuttlegentChatPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShuttlegentChatPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FShuttlegentChatModel>, Model)
	SLATE_END_ARGS()

private:
	using FChoicePtr = TSharedPtr<FShuttlegentConfigChoice>;

	TSharedPtr<FShuttlegentChatModel> Model;
	TSharedPtr<SScrollBox> ScrollBox;
	TSharedPtr<SVerticalBox> MessageList;
	TSharedPtr<SEditableTextBox> InputBox;
	TSharedPtr<SShuttlegentMark> HeaderMark;
	TSharedPtr<SShuttlegentMark> EmptyStateMark;
	TSharedPtr<SWidget> EmptyState;
	TSharedPtr<SComboBox<FChoicePtr>> ModelCombo;
	TSharedPtr<SComboBox<FChoicePtr>> ReasoningCombo;
	TArray<FChoicePtr> ModelChoices;
	TArray<FChoicePtr> ReasoningChoices;
	FString CurrentModelValue;
	FString CurrentReasoningValue;

	/** Last observed states, to play the mark on transitions only. */
	bool bWasPrompting = false;
	bool bShowingEmptyState = false;

	/** Tool rows and read-only groups the user expanded, by entry id (a group by its first entry); survive rebuilds. */
	TSet<int32> ExpandedToolIds;
	TSet<int32> ExpandedGroupIds;

	/** Content arrived while the view was scrolled away from the end; shows the back-to-bottom button. */
	bool bHasUnseenContent = false;

public:
	void Construct(const FArguments& InArgs);
	virtual ~SShuttlegentChatPanel() override;

private:
	TSharedRef<SWidget> BuildHeader();
	TSharedRef<SWidget> BuildStatusRow();
	TSharedRef<SWidget> BuildBanner();
	TSharedRef<SWidget> BuildInput();
	TSharedRef<SWidget> BuildEmptyState();
	TSharedRef<SWidget> BuildConfigCombo(TSharedPtr<SComboBox<FChoicePtr>>& OutCombo, TArray<FChoicePtr>& Choices, const FString& ConfigId, FString* CurrentValue);
	TSharedRef<SWidget> BuildHistoryMenu();

	TSharedRef<SWidget> BuildRow(const TSharedPtr<FShuttlegentChatEntry>& Entry);
	TSharedRef<SWidget> BuildUserRow(const TSharedPtr<FShuttlegentChatEntry>& Entry);
	TSharedRef<SWidget> BuildAssistantRow(const TSharedPtr<FShuttlegentChatEntry>& Entry);
	TSharedRef<SWidget> BuildToolCallRow(const TSharedPtr<FShuttlegentChatEntry>& Entry);
	TSharedRef<SWidget> BuildToolGroupRow(const TArray<TSharedPtr<FShuttlegentChatEntry>>& Group);
	TSharedRef<SWidget> MakeDetailText(TAttribute<FText> Text) const;
	TSharedRef<SWidget> BuildScrollToEndButton();
	TSharedRef<SWidget> BuildPermissionRow(const TSharedPtr<FShuttlegentChatEntry>& Entry);
	TSharedRef<SWidget> BuildNoticeRow(const TSharedPtr<FShuttlegentChatEntry>& Entry);
	TSharedRef<SWidget> MakeIconButton(const TCHAR* IconName, const FText& ToolTip, FOnClicked OnClicked) const;

	void RebuildMessages();
	void RefreshConfigChoices();
	void ScrollToEndIfFollowing(bool bWasAtEnd);
	bool IsScrolledToEnd() const;

	void HandleStructureChanged();
	void HandleContentChanged();
	void HandleStatusChanged();
	void HandleTurnFinished(bool bCompleted);
	void HandleUserScrolled(float ScrollOffset);

	void SubmitInput();
	FReply OnSendClicked();
	FReply OnNewConversationClicked();
	FReply OnSettingsClicked();
	FReply OnReconnectClicked();
	void OnInputCommitted(const FText& Text, ETextCommit::Type CommitType);

	FText GetStatusText() const;
	FSlateColor GetStatusColor() const;
	FSlateColor GetMarkColor() const;
	FText GetUsageText() const;
	const FSlateBrush* GetSendIcon() const;
	bool IsPrompting() const;
};
