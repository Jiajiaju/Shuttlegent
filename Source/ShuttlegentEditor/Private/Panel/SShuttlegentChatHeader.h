#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FShuttlegentChatModel;
class SButton;
class SComboButton;
class SEditableTextBox;
class SShuttlegentMark;

/**
 * Header of the conversation column (60 high): sidebar toggle, mark, session title with its
 * status line, rename, archive and inspector toggle. Plays the mark on turn transitions.
 */
class SShuttlegentChatHeader : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShuttlegentChatHeader) {}
		SLATE_ARGUMENT(TSharedPtr<FShuttlegentChatModel>, Model)
		SLATE_EVENT(FSimpleDelegate, OnToggleSidebar)
		SLATE_EVENT(FSimpleDelegate, OnToggleInspector)
	SLATE_END_ARGS()

private:
	TSharedPtr<FShuttlegentChatModel> Model;
	FSimpleDelegate OnToggleSidebar;
	FSimpleDelegate OnToggleInspector;
	TSharedPtr<SShuttlegentMark> Mark;
	TSharedPtr<SButton> SidebarToggle;
	TSharedPtr<SButton> InspectorToggle;
	TSharedPtr<SComboButton> RenameButton;
	TSharedPtr<SComboButton> ArchiveButton;
	TSharedPtr<SEditableTextBox> RenameBox;

	/** Rename rejected: the name was empty or too long. */
	bool bRenameRejected = false;

	/** Last observed state, to play the mark on transitions only. */
	bool bWasPrompting = false;

public:
	void Construct(const FArguments& InArgs);
	virtual ~SShuttlegentChatHeader() override;

	/** Toggles that open the overlays; focus returns here when an overlay closes. */
	TSharedPtr<SWidget> GetSidebarToggle() const;
	TSharedPtr<SWidget> GetInspectorToggle() const;

private:
	TSharedRef<SWidget> BuildRenameMenu();
	TSharedRef<SWidget> BuildArchiveMenu();
	void CommitRename();

	FText GetTitleText() const;
	FSlateColor GetMarkColor() const;
	bool IsPrompting() const;

	void HandleStatusChanged();
	void HandleTurnFinished(bool bCompleted);
};
