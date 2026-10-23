#include "Panel/SShuttlegentInspector.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Chat/FShuttlegentToolPresentation.h"
#include "Chat/ShuttlegentChatModel.h"
#include "Panel/FShuttlegentPanelWidgets.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentStyle.h"

#define LOCTEXT_NAMESPACE "SShuttlegentInspector"

namespace SShuttlegentInspectorPrivate
{
	// Tool call statuses reported by the runtime
	inline const TCHAR* StatusCompleted = TEXT("completed");
	inline const TCHAR* StatusFailed = TEXT("failed");

	inline const TCHAR* IconClose = TEXT("close");

	// Number of tabs; EShuttlegentInspectorTab values run from 0 to TabCount - 1
	inline constexpr int32 TabCount = static_cast<int32>(EShuttlegentInspectorTab::Log) + 1;

	FText GetStatusLabel(const FString& Status)
	{
		if (Status == StatusCompleted) return LOCTEXT("ToolCompleted", "已完成");
		if (Status == StatusFailed) return LOCTEXT("ToolFailed", "执行失败");
		return LOCTEXT("ToolRunning", "正在执行");
	}

	TSharedRef<SWidget> MakeCaption(const FText& Text)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(FShuttlegentStyle::CaptionFont())
			.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
			.AutoWrapText(true);
	}
}

void SShuttlegentInspector::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());
	OnClose = InArgs._OnClose;

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Pane))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(ShuttlegentEditorMetric::HeaderHeight)
				.Padding(FMargin(ShuttlegentEditorMetric::Space4, 0.0f, ShuttlegentEditorMetric::Space2, 0.0f))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Title", "检视"))
						.Font(FShuttlegentStyle::EmphasisFont())
						.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						FShuttlegentPanelWidgets::MakeIconButton(SShuttlegentInspectorPrivate::IconClose, LOCTEXT("Close", "关闭检视窗格 (Ctrl+I)"), FOnClicked::CreateLambda([this]()
						{
							OnClose.ExecuteIfBound();
							return FReply::Handled();
						}))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				FShuttlegentPanelWidgets::MakeHorizontalDivider()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(ShuttlegentEditorMetric::Space3, 0.0f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					BuildTab(EShuttlegentInspectorTab::Changes, LOCTEXT("TabChanges", "变更预览"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					BuildTab(EShuttlegentInspectorTab::Tools, LOCTEXT("TabTools", "工具详情"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					BuildTab(EShuttlegentInspectorTab::Log, LOCTEXT("TabLog", "运行日志"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				FShuttlegentPanelWidgets::MakeHorizontalDivider()
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SWidgetSwitcher)
				.WidgetIndex_Lambda([this]() { return static_cast<int32>(ActiveTab); })

				+ SWidgetSwitcher::Slot()
				[
					BuildChangesPage()
				]

				+ SWidgetSwitcher::Slot()
				[
					BuildToolsPage()
				]

				+ SWidgetSwitcher::Slot()
				[
					BuildLogPage()
				]
			]
		]
	];

	Model->OnStructureChanged.AddSP(this, &SShuttlegentInspector::HandleStructureChanged);
	Model->OnRunLogChanged.AddSP(this, &SShuttlegentInspector::HandleRunLogChanged);
	RebuildTools();
	RebuildLog();
}

SShuttlegentInspector::~SShuttlegentInspector()
{
	if (!Model.IsValid()) return;

	Model->OnStructureChanged.RemoveAll(this);
	Model->OnRunLogChanged.RemoveAll(this);
}

void SShuttlegentInspector::SetActiveTab(EShuttlegentInspectorTab Tab)
{
	ActiveTab = Tab;
}

void SShuttlegentInspector::SelectToolCall(int32 EntryId)
{
	SelectedToolId = EntryId;
	ActiveTab = EShuttlegentInspectorTab::Tools;
	RebuildTools();

	const TWeakPtr<SWidget>* Row = ToolRows.Find(EntryId);
	const TSharedPtr<SWidget> Pinned = Row != nullptr ? Row->Pin() : nullptr;
	if (Pinned.IsValid() && ToolScroll.IsValid()) ToolScroll->ScrollDescendantIntoView(Pinned, true, EDescendantScrollDestination::TopOrLeft);
}

void SShuttlegentInspector::FocusActiveTab()
{
	const int32 Index = static_cast<int32>(ActiveTab);
	if (TabButtons.IsValidIndex(Index) && TabButtons[Index].IsValid()) FSlateApplication::Get().SetKeyboardFocus(TabButtons[Index], EFocusCause::Navigation);
}

FReply SShuttlegentInspector::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (!IsTabFocused()) return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);

	const FKey Key = InKeyEvent.GetKey();
	int32 Index = static_cast<int32>(ActiveTab);
	if (Key == EKeys::Left) Index = (Index + SShuttlegentInspectorPrivate::TabCount - 1) % SShuttlegentInspectorPrivate::TabCount;
	else if (Key == EKeys::Right) Index = (Index + 1) % SShuttlegentInspectorPrivate::TabCount;
	else if (Key == EKeys::Home) Index = 0;
	else if (Key == EKeys::End) Index = SShuttlegentInspectorPrivate::TabCount - 1;
	else return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);

	ActiveTab = static_cast<EShuttlegentInspectorTab>(Index);
	FocusActiveTab();
	return FReply::Handled();
}

TSharedRef<SWidget> SShuttlegentInspector::BuildTab(EShuttlegentInspectorTab Tab, const FText& Label)
{
	TSharedPtr<SButton> Button;

	const TSharedRef<SWidget> Widget = SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(Button, SButton)
			.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
			.ContentPadding(FMargin(ShuttlegentEditorMetric::Space2, ShuttlegentEditorMetric::Space3))
			.ToolTipText(Label)
			.OnClicked_Lambda([this, Tab]()
			{
				ActiveTab = Tab;
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FShuttlegentStyle::CaptionFont())
				.ColorAndOpacity_Lambda([this, Tab]() { return FShuttlegentStyle::Tint(ActiveTab == Tab ? ShuttlegentEditorColor::Text : ShuttlegentEditorColor::Muted); })
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBox)
			.HeightOverride(ShuttlegentEditorMetric::AccentLineWidth)
			.Visibility_Lambda([this, Tab]() { return ActiveTab == Tab ? EVisibility::HitTestInvisible : EVisibility::Hidden; })
			[
				SNew(SImage)
				.Image(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::AccentLine))
			]
		];

	TabButtons.Add(Button);
	return Widget;
}

TSharedRef<SWidget> SShuttlegentInspector::BuildChangesPage()
{
	return SNew(SBox)
		.Padding(ShuttlegentEditorMetric::Space4)
		[
			SShuttlegentInspectorPrivate::MakeCaption(LOCTEXT("NoChanges", "暂无待确认的编辑器变更"))
		];
}

TSharedRef<SWidget> SShuttlegentInspector::BuildToolsPage()
{
	return SAssignNew(ToolScroll, SScrollBox)
		+ SScrollBox::Slot()
		.Padding(ShuttlegentEditorMetric::Space4)
		[
			SAssignNew(ToolList, SVerticalBox)
		];
}

TSharedRef<SWidget> SShuttlegentInspector::BuildLogPage()
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		.Padding(ShuttlegentEditorMetric::Space4)
		[
			SAssignNew(LogList, SVerticalBox)
		];
}

TSharedRef<SWidget> SShuttlegentInspector::BuildToolDetail(int32 EntryId)
{
	TSharedPtr<FShuttlegentChatEntry> Entry;
	for (const TSharedPtr<FShuttlegentChatEntry>& Candidate : Model->GetEntries())
	{
		if (Candidate->Id == EntryId) Entry = Candidate;
	}
	TWeakPtr<FShuttlegentChatEntry> WeakEntry = Entry;

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, ShuttlegentEditorMetric::Space2, 0.0f, ShuttlegentEditorMetric::Space1)
		[
			SShuttlegentInspectorPrivate::MakeCaption(LOCTEXT("ToolInput", "参数"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			FShuttlegentPanelWidgets::MakeDetailText(TAttribute<FText>::CreateLambda([WeakEntry]()
			{
				const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
				return Pinned.IsValid() ? FText::FromString(Pinned->Input) : FText::GetEmpty();
			}))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, ShuttlegentEditorMetric::Space2, 0.0f, ShuttlegentEditorMetric::Space1)
		[
			SShuttlegentInspectorPrivate::MakeCaption(LOCTEXT("ToolResult", "结果"))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			FShuttlegentPanelWidgets::MakeDetailText(TAttribute<FText>::CreateLambda([WeakEntry]()
			{
				const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
				if (!Pinned.IsValid()) return FText::GetEmpty();

				return Pinned->Result.IsEmpty() ? LOCTEXT("ToolNoResult", "（尚无结果）") : FText::FromString(Pinned->Result);
			}))
		];
}

void SShuttlegentInspector::RebuildTools()
{
	if (!ToolList.IsValid()) return;

	ToolList->ClearChildren();
	ToolRows.Reset();

	for (const TSharedPtr<FShuttlegentChatEntry>& Entry : Model->GetEntries())
	{
		if (Entry->Kind != EShuttlegentChatEntryKind::ToolCall) continue;

		TWeakPtr<FShuttlegentChatEntry> WeakEntry = Entry;
		const int32 EntryId = Entry->Id;
		const bool bSelected = EntryId == SelectedToolId;

		const TSharedRef<SWidget> Row = SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
				.ContentPadding(FMargin(0.0f, ShuttlegentEditorMetric::Space1))
				.ToolTipText(LOCTEXT("ToolToggle", "展开或收起参数与结果"))
				.OnClicked_Lambda([this, EntryId]()
				{
					SelectedToolId = SelectedToolId == EntryId ? 0 : EntryId;
					RebuildTools();
					return FReply::Handled();
				})
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(STextBlock)
							.Font(bSelected ? FShuttlegentStyle::EmphasisFont() : FShuttlegentStyle::BodyFont())
							.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
							.Text_Lambda([WeakEntry]()
							{
								const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
								return Pinned.IsValid() ? FShuttlegentToolPresentation::GetDisplayName(Pinned->Title) : FText::GetEmpty();
							})
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Font(FShuttlegentStyle::CaptionFont())
							.Text_Lambda([WeakEntry]()
							{
								const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
								return SShuttlegentInspectorPrivate::GetStatusLabel(Pinned.IsValid() ? Pinned->Status : FString());
							})
							.ColorAndOpacity_Lambda([WeakEntry]()
							{
								const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
								const bool bFailed = Pinned.IsValid() && Pinned->Status == SShuttlegentInspectorPrivate::StatusFailed;
								return FShuttlegentStyle::Tint(bFailed ? ShuttlegentEditorColor::Error : ShuttlegentEditorColor::Muted);
							})
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Font(FShuttlegentStyle::CaptionFont())
						.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						.Text_Lambda([WeakEntry]()
						{
							const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
							return Pinned.IsValid() ? FText::FromString(FShuttlegentToolPresentation::GetSummary(Pinned->Input)) : FText::GetEmpty();
						})
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				bSelected ? BuildToolDetail(EntryId) : SNullWidget::NullWidget
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, ShuttlegentEditorMetric::Space2)
			[
				FShuttlegentPanelWidgets::MakeHorizontalDivider()
			];

		ToolRows.Add(EntryId, Row);
		ToolList->AddSlot()
			.AutoHeight()
			[
				Row
			];
	}

	if (ToolRows.Num() > 0) return;

	ToolList->AddSlot()
		.AutoHeight()
		[
			SShuttlegentInspectorPrivate::MakeCaption(LOCTEXT("NoTools", "本会话还没有工具调用。"))
		];
}

void SShuttlegentInspector::RebuildLog()
{
	if (!LogList.IsValid()) return;

	LogList->ClearChildren();

	const TArray<FShuttlegentRunLogEntry>& Log = Model->GetRunLog();
	if (Log.IsEmpty())
	{
		LogList->AddSlot()
			.AutoHeight()
			[
				SShuttlegentInspectorPrivate::MakeCaption(LOCTEXT("NoLog", "尚无调用记录。密钥、完整环境变量不进入日志。"))
			];
		return;
	}

	for (const FShuttlegentRunLogEntry& Entry : Log)
	{
		LogList->AddSlot()
			.AutoHeight()
			.Padding(0.0f, ShuttlegentEditorMetric::Space1)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FShuttlegentPanelWidgets::FormatLogTime(Entry.Time))
					.Font(FShuttlegentStyle::MonoCaptionFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Info))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(Entry.Text)
					.Font(FShuttlegentStyle::CaptionFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(Entry.bWarning ? ShuttlegentEditorColor::Warning : ShuttlegentEditorColor::Muted))
					.AutoWrapText(true)
				]
			];
	}
}

bool SShuttlegentInspector::IsTabFocused() const
{
	for (const TSharedPtr<SButton>& Button : TabButtons)
	{
		if (Button.IsValid() && Button->HasKeyboardFocus()) return true;
	}
	return false;
}

void SShuttlegentInspector::HandleStructureChanged()
{
	// Entries were reset (new or reopened conversation): drop a selection that no longer exists
	bool bSelectionExists = false;
	for (const TSharedPtr<FShuttlegentChatEntry>& Entry : Model->GetEntries())
	{
		if (Entry->Id == SelectedToolId) bSelectionExists = true;
	}
	if (!bSelectionExists) SelectedToolId = 0;

	RebuildTools();
}

void SShuttlegentInspector::HandleRunLogChanged()
{
	RebuildLog();
}

#undef LOCTEXT_NAMESPACE
