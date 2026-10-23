#pragma once

#include "CoreMinimal.h"
#include "Types/SlateStructs.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FShuttlegentChatModel;
class SScrollBox;
class SShuttlegentMark;
class SVerticalBox;
struct FShuttlegentChatEntry;

DECLARE_DELEGATE_OneParam(FOnShuttlegentToolSelected, int32 /*EntryId*/);

/**
 * Conversation column: messages centered at most ConversationMaxWidth wide, the empty state,
 * and the back-to-bottom button. Tool rows open their details in the inspector.
 */
class SShuttlegentMessageList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShuttlegentMessageList) {}
		SLATE_ARGUMENT(TSharedPtr<FShuttlegentChatModel>, Model)
		SLATE_EVENT(FOnShuttlegentToolSelected, OnToolSelected)
	SLATE_END_ARGS()

private:
	TSharedPtr<FShuttlegentChatModel> Model;
	FOnShuttlegentToolSelected OnToolSelected;
	TSharedPtr<SScrollBox> ScrollBox;
	TSharedPtr<SVerticalBox> MessageList;
	TSharedPtr<SShuttlegentMark> EmptyStateMark;
	TSharedPtr<SWidget> EmptyState;

	/** Width of the scroll area at the last tick; sizes the column. */
	float ViewWidth = 0.0f;

	bool bShowingEmptyState = false;

	/** Content arrived while the view was scrolled away from the end; shows the back-to-bottom button. */
	bool bHasUnseenContent = false;

public:
	void Construct(const FArguments& InArgs);
	virtual ~SShuttlegentMessageList() override;

	void ScrollToEnd();

	//~ Begin SWidget Interface.
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget Interface.

private:
	TSharedRef<SWidget> BuildEmptyState();
	TSharedRef<SWidget> BuildScrollToEndButton();

	TSharedRef<SWidget> BuildRow(const TSharedPtr<FShuttlegentChatEntry>& Entry);
	TSharedRef<SWidget> BuildUserRow(const TSharedPtr<FShuttlegentChatEntry>& Entry);
	TSharedRef<SWidget> BuildAssistantRow(const TSharedPtr<FShuttlegentChatEntry>& Entry);
	TSharedRef<SWidget> BuildToolCallRow(const TSharedPtr<FShuttlegentChatEntry>& Entry);
	TSharedRef<SWidget> BuildToolGroupRow(const TArray<TSharedPtr<FShuttlegentChatEntry>>& Group);
	TSharedRef<SWidget> BuildPermissionRow(const TSharedPtr<FShuttlegentChatEntry>& Entry);
	TSharedRef<SWidget> BuildNoticeRow(const TSharedPtr<FShuttlegentChatEntry>& Entry);

	/** Hint-style row: 2 px rule on the left, 12 px secondary text (prototype .message.note). */
	TSharedRef<SWidget> MakeNote(const TSharedRef<SWidget>& Content) const;

	void RebuildMessages();
	bool IsScrolledToEnd() const;
	FOptionalSize GetColumnWidth() const;
	FOptionalSize GetBubbleMaxWidth() const;

	void HandleStructureChanged();
	void HandleContentChanged();
	void HandleUserScrolled(float ScrollOffset);
};
