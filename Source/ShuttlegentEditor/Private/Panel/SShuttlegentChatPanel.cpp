#include "Panel/SShuttlegentChatPanel.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "Chat/ShuttlegentChatModel.h"
#include "Panel/FShuttlegentPanelCommands.h"
#include "Panel/FShuttlegentPanelWidgets.h"
#include "Panel/SShuttlegentChatHeader.h"
#include "Panel/SShuttlegentComposer.h"
#include "Panel/SShuttlegentInspector.h"
#include "Panel/SShuttlegentMessageList.h"
#include "Panel/SShuttlegentSessionSidebar.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentStyle.h"

#define LOCTEXT_NAMESPACE "SShuttlegentChatPanel"

namespace SShuttlegentChatPanelPrivate
{
	EShuttlegentPanelLayout GetLayout(float Width)
	{
		if (Width >= ShuttlegentEditorMetric::BreakpointFull) return EShuttlegentPanelLayout::Full;
		if (Width >= ShuttlegentEditorMetric::BreakpointWide) return EShuttlegentPanelLayout::Wide;
		if (Width >= ShuttlegentEditorMetric::BreakpointMedium) return EShuttlegentPanelLayout::Medium;
		return EShuttlegentPanelLayout::Narrow;
	}
}

void SShuttlegentChatPanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());

	SAssignNew(Sidebar, SShuttlegentSessionSidebar)
		.Model(Model)
		.OnCollapse(this, &SShuttlegentChatPanel::ToggleSidebar)
		.OnSessionChosen_Lambda([this]()
		{
			if (Overlay == EShuttlegentPanelOverlay::Sidebar) CloseOverlay();
		});

	SAssignNew(Inspector, SShuttlegentInspector)
		.Model(Model)
		.OnClose(this, &SShuttlegentChatPanel::ToggleInspector);

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Background))
		.Padding(0.0f)
		[
			SNew(SOverlay)

			+ SOverlay::Slot()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SHorizontalBox)
					.Visibility_Lambda([this]() { return IsSidebarDockable() && IsSidebarVisible() ? EVisibility::Visible : EVisibility::Collapsed; })

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(DockedSidebarSlot, SBox)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						FShuttlegentPanelWidgets::MakeVerticalDivider()
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(Header, SShuttlegentChatHeader)
						.Model(Model)
						.OnToggleSidebar(this, &SShuttlegentChatPanel::ToggleSidebar)
						.OnToggleInspector(this, &SShuttlegentChatPanel::ToggleInspector)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						FShuttlegentPanelWidgets::MakeHorizontalDivider()
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						BuildBanner()
					]

					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(MessageList, SShuttlegentMessageList)
						.Model(Model)
						.OnToolSelected(this, &SShuttlegentChatPanel::HandleToolSelected)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SAssignNew(Composer, SShuttlegentComposer)
						.Model(Model)
						.OnSubmitted_Lambda([this]()
						{
							if (MessageList.IsValid()) MessageList->ScrollToEnd();
						})
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SHorizontalBox)
					.Visibility_Lambda([this]() { return IsInspectorDockable() && IsInspectorVisible() ? EVisibility::Visible : EVisibility::Collapsed; })

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						FShuttlegentPanelWidgets::MakeVerticalDivider()
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SAssignNew(DockedInspectorSlot, SBox)
						.WidthOverride(ShuttlegentEditorMetric::InspectorWidth)
					]
				]
			]

			+ SOverlay::Slot()
			[
				SNew(SBorder)
				.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Scrim))
				.Visibility_Lambda([this]() { return Overlay != EShuttlegentPanelOverlay::None ? EVisibility::Visible : EVisibility::Collapsed; })
				.OnMouseButtonDown_Lambda([this](const FGeometry&, const FPointerEvent&)
				{
					CloseOverlay();
					return FReply::Handled();
				})
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Left)
			[
				SAssignNew(OverlaySidebarSlot, SBox)
				.Visibility_Lambda([this]() { return Overlay == EShuttlegentPanelOverlay::Sidebar ? EVisibility::Visible : EVisibility::Collapsed; })
			]

			+ SOverlay::Slot()
			.HAlign(HAlign_Right)
			[
				SAssignNew(OverlayInspectorSlot, SBox)
				.WidthOverride(this, &SShuttlegentChatPanel::GetOverlayInspectorWidth)
				.Visibility_Lambda([this]() { return Overlay == EShuttlegentPanelOverlay::Inspector ? EVisibility::Visible : EVisibility::Collapsed; })
			]
		]
	];

	BindCommands();
	PlacePanes();

	Model->SetPermissionHandlerAttached(true);
	Model->EnsureRuntime();
}

SShuttlegentChatPanel::~SShuttlegentChatPanel()
{
	if (Model.IsValid()) Model->SetPermissionHandlerAttached(false);
}

void SShuttlegentChatPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	PanelWidth = AllottedGeometry.GetLocalSize().X;

	const EShuttlegentPanelLayout NewLayout = SShuttlegentChatPanelPrivate::GetLayout(PanelWidth);
	if (bLayoutApplied && NewLayout == Layout) return;

	Layout = NewLayout;
	bLayoutApplied = true;

	// An overlay whose pane now docks becomes the docked pane; only the layout changes, nothing is cleared
	if (Overlay == EShuttlegentPanelOverlay::Sidebar && IsSidebarDockable())
	{
		Overlay = EShuttlegentPanelOverlay::None;
		bSidebarDocked = true;
	}
	if (Overlay == EShuttlegentPanelOverlay::Inspector && IsInspectorDockable())
	{
		Overlay = EShuttlegentPanelOverlay::None;
		bInspectorDocked = true;
	}
	PlacePanes();
}

FReply SShuttlegentChatPanel::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// Esc closes the topmost overlay first and never reaches the task underneath
	if (InKeyEvent.GetKey() == EKeys::Escape && Overlay != EShuttlegentPanelOverlay::None)
	{
		CloseOverlay();
		return FReply::Handled();
	}

	if (CommandList.IsValid() && CommandList->ProcessCommandBindings(InKeyEvent)) return FReply::Handled();

	return SCompoundWidget::OnPreviewKeyDown(MyGeometry, InKeyEvent);
}

TSharedRef<SWidget> SShuttlegentChatPanel::BuildBanner()
{
	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;

	return SNew(SVerticalBox)
		.Visibility_Lambda([WeakModel]()
		{
			const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
			const EShuttlegentChatStatus Status = Pinned.IsValid() ? Pinned->GetStatus() : EShuttlegentChatStatus::NotConnected;
			return Status == EShuttlegentChatStatus::Lost || Status == EShuttlegentChatStatus::Failed ? EVisibility::Visible : EVisibility::Collapsed;
		})

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Banner))
			.Padding(FMargin(ShuttlegentEditorMetric::Space4, ShuttlegentEditorMetric::Space2))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FShuttlegentStyle::CaptionFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Warning))
					.AutoWrapText(true)
					.Text_Lambda([WeakModel]()
					{
						const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
						const bool bLost = Pinned.IsValid() && Pinned->GetStatus() == EShuttlegentChatStatus::Lost;
						return bLost ? LOCTEXT("LostBanner", "连接中断，草稿与队列已保留。") : LOCTEXT("FailedBanner", "查询失败，尚未执行修改。草稿和队列已保留。");
					})
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
				[
					SNew(SBox)
					.Visibility_Lambda([WeakModel]()
					{
						const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
						return Pinned.IsValid() && Pinned->GetStatus() == EShuttlegentChatStatus::Lost ? EVisibility::Visible : EVisibility::Collapsed;
					})
					[
						FShuttlegentPanelWidgets::MakeTextButton(LOCTEXT("Reconnect", "重新连接"), false, FOnClicked::CreateLambda([WeakModel]()
						{
							const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
							if (Pinned.IsValid()) Pinned->Reconnect();
							return FReply::Handled();
						}))
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(ShuttlegentEditorMetric::BorderWidth)
			[
				SNew(SImage)
				.Image(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::BannerLine))
			]
		];
}

void SShuttlegentChatPanel::BindCommands()
{
	const FShuttlegentPanelCommands& Commands = FShuttlegentPanelCommands::Get();
	CommandList = MakeShared<FUICommandList>();

	CommandList->MapAction(Commands.ToggleSidebar, FExecuteAction::CreateSP(this, &SShuttlegentChatPanel::ToggleSidebar));
	CommandList->MapAction(Commands.ToggleInspector, FExecuteAction::CreateSP(this, &SShuttlegentChatPanel::ToggleInspector));
	CommandList->MapAction(Commands.NewSession, FExecuteAction::CreateLambda([this]()
	{
		Model->NewConversation();
		if (Composer.IsValid()) Composer->FocusInput();
	}));
	CommandList->MapAction(Commands.SearchSessions, FExecuteAction::CreateLambda([this]()
	{
		ShowSidebar();
		if (Sidebar.IsValid()) Sidebar->FocusSearch();
	}));
	CommandList->MapAction(Commands.FocusInput, FExecuteAction::CreateLambda([this]()
	{
		if (Composer.IsValid()) Composer->FocusInput();
	}));
}

void SShuttlegentChatPanel::PlacePanes()
{
	// A widget has one parent: empty every slot before placing the panes again
	DockedSidebarSlot->SetContent(SNullWidget::NullWidget);
	OverlaySidebarSlot->SetContent(SNullWidget::NullWidget);
	DockedInspectorSlot->SetContent(SNullWidget::NullWidget);
	OverlayInspectorSlot->SetContent(SNullWidget::NullWidget);

	if (Overlay == EShuttlegentPanelOverlay::Sidebar) OverlaySidebarSlot->SetContent(Sidebar.ToSharedRef());
	else if (IsSidebarDockable()) DockedSidebarSlot->SetContent(Sidebar.ToSharedRef());

	if (Overlay == EShuttlegentPanelOverlay::Inspector) OverlayInspectorSlot->SetContent(Inspector.ToSharedRef());
	else if (IsInspectorDockable()) DockedInspectorSlot->SetContent(Inspector.ToSharedRef());
}

bool SShuttlegentChatPanel::IsSidebarDockable() const
{
	return Layout == EShuttlegentPanelLayout::Full;
}

bool SShuttlegentChatPanel::IsInspectorDockable() const
{
	return Layout == EShuttlegentPanelLayout::Wide || Layout == EShuttlegentPanelLayout::Full;
}

bool SShuttlegentChatPanel::IsSidebarVisible() const
{
	return IsSidebarDockable() ? bSidebarDocked : Overlay == EShuttlegentPanelOverlay::Sidebar;
}

bool SShuttlegentChatPanel::IsInspectorVisible() const
{
	return IsInspectorDockable() ? bInspectorDocked : Overlay == EShuttlegentPanelOverlay::Inspector;
}

void SShuttlegentChatPanel::ToggleSidebar()
{
	if (IsSidebarDockable())
	{
		bSidebarDocked = !bSidebarDocked;
		return;
	}

	if (Overlay == EShuttlegentPanelOverlay::Sidebar)
	{
		CloseOverlay();
		return;
	}

	OpenOverlay(EShuttlegentPanelOverlay::Sidebar, Header.IsValid() ? Header->GetSidebarToggle() : nullptr);
}

void SShuttlegentChatPanel::ToggleInspector()
{
	if (IsInspectorDockable())
	{
		bInspectorDocked = !bInspectorDocked;
		return;
	}

	if (Overlay == EShuttlegentPanelOverlay::Inspector)
	{
		CloseOverlay();
		return;
	}

	OpenOverlay(EShuttlegentPanelOverlay::Inspector, Header.IsValid() ? Header->GetInspectorToggle() : nullptr);
}

void SShuttlegentChatPanel::OpenOverlay(EShuttlegentPanelOverlay InOverlay, const TSharedPtr<SWidget>& Opener)
{
	// Overlays exclude each other; opening one replaces the other but keeps the first opener
	if (Overlay == EShuttlegentPanelOverlay::None) OverlayOpener = Opener;
	Overlay = InOverlay;
	PlacePanes();
}

void SShuttlegentChatPanel::CloseOverlay()
{
	if (Overlay == EShuttlegentPanelOverlay::None) return;

	Overlay = EShuttlegentPanelOverlay::None;
	PlacePanes();

	const TSharedPtr<SWidget> Opener = OverlayOpener.Pin();
	OverlayOpener.Reset();
	if (Opener.IsValid()) FSlateApplication::Get().SetKeyboardFocus(Opener, EFocusCause::SetDirectly);
}

void SShuttlegentChatPanel::ShowSidebar()
{
	if (IsSidebarDockable())
	{
		bSidebarDocked = true;
		return;
	}

	if (Overlay != EShuttlegentPanelOverlay::Sidebar) OpenOverlay(EShuttlegentPanelOverlay::Sidebar, Header.IsValid() ? Header->GetSidebarToggle() : nullptr);
}

void SShuttlegentChatPanel::ShowInspector()
{
	if (IsInspectorDockable())
	{
		bInspectorDocked = true;
		return;
	}

	if (Overlay != EShuttlegentPanelOverlay::Inspector) OpenOverlay(EShuttlegentPanelOverlay::Inspector, Header.IsValid() ? Header->GetInspectorToggle() : nullptr);
}

FOptionalSize SShuttlegentChatPanel::GetOverlayInspectorWidth() const
{
	if (Layout == EShuttlegentPanelLayout::Narrow) return PanelWidth;

	return FMath::Min(PanelWidth, ShuttlegentEditorMetric::InspectorOverlayMaxWidth);
}

void SShuttlegentChatPanel::HandleToolSelected(int32 EntryId)
{
	ShowInspector();
	if (Inspector.IsValid()) Inspector->SelectToolCall(EntryId);
}

#undef LOCTEXT_NAMESPACE
