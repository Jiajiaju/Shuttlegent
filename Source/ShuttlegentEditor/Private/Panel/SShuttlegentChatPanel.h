#pragma once

#include "CoreMinimal.h"
#include "Types/SlateStructs.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

#include "Panel/EShuttlegentPanelLayout.h"
#include "Panel/EShuttlegentPanelOverlay.h"

class FShuttlegentChatModel;
class FUICommandList;
class SBox;
class SShuttlegentChatHeader;
class SShuttlegentComposer;
class SShuttlegentInspector;
class SShuttlegentMessageList;
class SShuttlegentSessionSidebar;

/**
 * Shuttlegent chat panel (Window > Shuttlegent): session sidebar, conversation column and
 * inspector, laid out by the panel width (Shuttlegent UI interaction spec section 2).
 *
 * The sidebar and the inspector are single widgets moved between their docked slot and their
 * overlay slot, so a layout change keeps search text, selection, the draft and a pending decision.
 */
class SShuttlegentChatPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShuttlegentChatPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FShuttlegentChatModel>, Model)
	SLATE_END_ARGS()

private:
	TSharedPtr<FShuttlegentChatModel> Model;
	TSharedPtr<FUICommandList> CommandList;

	TSharedPtr<SShuttlegentSessionSidebar> Sidebar;
	TSharedPtr<SShuttlegentInspector> Inspector;
	TSharedPtr<SShuttlegentChatHeader> Header;
	TSharedPtr<SShuttlegentMessageList> MessageList;
	TSharedPtr<SShuttlegentComposer> Composer;

	/** Slots the sidebar and the inspector move between. */
	TSharedPtr<SBox> DockedSidebarSlot;
	TSharedPtr<SBox> OverlaySidebarSlot;
	TSharedPtr<SBox> DockedInspectorSlot;
	TSharedPtr<SBox> OverlayInspectorSlot;

	/** Widget that opened the current overlay; focus returns to it when the overlay closes. */
	TWeakPtr<SWidget> OverlayOpener;

	/** Panel width at the last tick. */
	float PanelWidth = 0.0f;

	EShuttlegentPanelLayout Layout = EShuttlegentPanelLayout::Narrow;
	EShuttlegentPanelOverlay Overlay = EShuttlegentPanelOverlay::None;

	/** Docked panes the user has not closed; used where the layout docks them. */
	bool bSidebarDocked = true;
	bool bInspectorDocked = true;

	/** Layout has been applied once; the first tick always places the panes. */
	bool bLayoutApplied = false;

public:
	void Construct(const FArguments& InArgs);
	virtual ~SShuttlegentChatPanel() override;

	//~ Begin SWidget Interface.
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget Interface.

private:
	TSharedRef<SWidget> BuildBanner();
	void BindCommands();

	/** Puts the sidebar and the inspector into their docked or overlay slot for the current layout and overlay. */
	void PlacePanes();

	bool IsSidebarDockable() const;
	bool IsInspectorDockable() const;
	bool IsSidebarVisible() const;
	bool IsInspectorVisible() const;

	void ToggleSidebar();
	void ToggleInspector();
	void OpenOverlay(EShuttlegentPanelOverlay InOverlay, const TSharedPtr<SWidget>& Opener);
	void CloseOverlay();

	/** Makes the sidebar visible (docked or as an overlay). */
	void ShowSidebar();

	/** Makes the inspector visible (docked or as an overlay). */
	void ShowInspector();

	FOptionalSize GetOverlayInspectorWidth() const;

	void HandleToolSelected(int32 EntryId);
};
