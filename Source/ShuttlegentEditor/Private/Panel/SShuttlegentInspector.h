#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "Panel/EShuttlegentInspectorTab.h"

class FShuttlegentChatModel;
class SButton;
class SScrollBox;
class SVerticalBox;

/**
 * Inspector pane (360 wide when docked): change preview, tool details of the current session,
 * and the run log. Tabs switch with Left / Right / Home / End while a tab has focus.
 */
class SShuttlegentInspector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShuttlegentInspector) {}
		SLATE_ARGUMENT(TSharedPtr<FShuttlegentChatModel>, Model)
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

private:
	TSharedPtr<FShuttlegentChatModel> Model;
	FSimpleDelegate OnClose;
	TArray<TSharedPtr<SButton>> TabButtons;
	TSharedPtr<SScrollBox> ToolScroll;
	TSharedPtr<SVerticalBox> ToolList;
	TSharedPtr<SVerticalBox> LogList;

	/** Tool call row widgets by entry id, to scroll the selected one into view. */
	TMap<int32, TWeakPtr<SWidget>> ToolRows;

	EShuttlegentInspectorTab ActiveTab = EShuttlegentInspectorTab::Changes;

	/** Tool call whose parameters and result are expanded; 0 for none. */
	int32 SelectedToolId = 0;

public:
	void Construct(const FArguments& InArgs);
	virtual ~SShuttlegentInspector() override;

	void SetActiveTab(EShuttlegentInspectorTab Tab);

	/** Opens "tool details" with one call expanded and scrolled into view. */
	void SelectToolCall(int32 EntryId);

	/** Moves keyboard focus to the active tab. */
	void FocusActiveTab();

	//~ Begin SWidget Interface.
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget Interface.

private:
	TSharedRef<SWidget> BuildTab(EShuttlegentInspectorTab Tab, const FText& Label);
	TSharedRef<SWidget> BuildChangesPage();
	TSharedRef<SWidget> BuildToolsPage();
	TSharedRef<SWidget> BuildLogPage();
	TSharedRef<SWidget> BuildToolDetail(int32 EntryId);

	void RebuildTools();
	void RebuildLog();
	bool IsTabFocused() const;

	void HandleStructureChanged();
	void HandleRunLogChanged();
};
