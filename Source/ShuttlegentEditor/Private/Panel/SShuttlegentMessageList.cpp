#include "Panel/SShuttlegentMessageList.h"

#include "HAL/PlatformApplicationMisc.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "Chat/FShuttlegentToolPresentation.h"
#include "Chat/ShuttlegentChatModel.h"
#include "Mark/EShuttlegentMarkMotion.h"
#include "Mark/EShuttlegentMarkTier.h"
#include "Mark/SShuttlegentMark.h"
#include "Markdown/SShuttlegentMarkdownView.h"
#include "Panel/FShuttlegentPanelWidgets.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentStyle.h"

#define LOCTEXT_NAMESPACE "SShuttlegentMessageList"

namespace SShuttlegentMessageListPrivate
{
	// Tool call statuses reported by the runtime
	inline const TCHAR* StatusCompleted = TEXT("completed");
	inline const TCHAR* StatusFailed = TEXT("failed");

	// Icons
	inline const TCHAR* IconCheck = TEXT("check");
	inline const TCHAR* IconAlert = TEXT("alert");
	inline const TCHAR* IconTools = TEXT("tools");

	// Distance from the bottom, in Slate units, within which new content keeps the view pinned to the end
	inline constexpr float FollowThreshold = 24.0f;

	bool IsReadOnlyToolCall(const TSharedPtr<FShuttlegentChatEntry>& Entry)
	{
		return Entry.IsValid() && Entry->Kind == EShuttlegentChatEntryKind::ToolCall && FShuttlegentToolPresentation::IsReadOnly(Entry->Title);
	}

	bool IsToolFinished(const FString& Status)
	{
		return Status == StatusCompleted || Status == StatusFailed;
	}

	const FSlateBrush* GetStatusIcon(const FString& Status)
	{
		if (Status == StatusCompleted) return FShuttlegentStyle::Icon(IconCheck);
		if (Status == StatusFailed) return FShuttlegentStyle::Icon(IconAlert);
		return FShuttlegentStyle::Icon(IconTools);
	}

	FSlateColor GetStatusTint(const FString& Status)
	{
		if (Status == StatusCompleted) return FShuttlegentStyle::Tint(ShuttlegentEditorColor::Success);
		if (Status == StatusFailed) return FShuttlegentStyle::Tint(ShuttlegentEditorColor::Error);
		return FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted);
	}

	/** Right-hand summary of a tool row: what it touched, or how it stands. */
	FText GetToolTrailer(const FString& Status, bool bReadOnly, int32 Objects, int32 Failed)
	{
		if (!IsToolFinished(Status)) return LOCTEXT("TrailerRunning", "正在执行 ›");
		if (Failed > 0) return FText::Format(LOCTEXT("TrailerFailed", "{0} 项失败 ›"), FText::AsNumber(Failed));
		if (bReadOnly) return FText::Format(LOCTEXT("TrailerReadOnly", "只读 · {0} 个对象 ›"), FText::AsNumber(Objects));
		return LOCTEXT("TrailerDone", "已完成 ›");
	}
}

void SShuttlegentMessageList::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());
	OnToolSelected = InArgs._OnToolSelected;

	EmptyState = BuildEmptyState();

	ChildSlot
	[
		SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SAssignNew(ScrollBox, SScrollBox)
			.OnUserScrolled(this, &SShuttlegentMessageList::HandleUserScrolled)
			+ SScrollBox::Slot()
			.HAlign(HAlign_Center)
			.Padding(FMargin(ShuttlegentEditorMetric::ConversationPaddingSide, ShuttlegentEditorMetric::ConversationPaddingTop, ShuttlegentEditorMetric::ConversationPaddingSide, ShuttlegentEditorMetric::ConversationPaddingBottom))
			[
				SNew(SBox)
				.WidthOverride(this, &SShuttlegentMessageList::GetColumnWidth)
				[
					SAssignNew(MessageList, SVerticalBox)
				]
			]
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Bottom)
		.Padding(ShuttlegentEditorMetric::Space3)
		[
			BuildScrollToEndButton()
		]
	];

	Model->OnStructureChanged.AddSP(this, &SShuttlegentMessageList::HandleStructureChanged);
	Model->OnContentChanged.AddSP(this, &SShuttlegentMessageList::HandleContentChanged);

	RebuildMessages();
	ScrollBox->ScrollToEnd();
}

SShuttlegentMessageList::~SShuttlegentMessageList()
{
	if (!Model.IsValid()) return;

	Model->OnStructureChanged.RemoveAll(this);
	Model->OnContentChanged.RemoveAll(this);
}

void SShuttlegentMessageList::ScrollToEnd()
{
	if (ScrollBox.IsValid()) ScrollBox->ScrollToEnd();
	bHasUnseenContent = false;
}

void SShuttlegentMessageList::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	ViewWidth = AllottedGeometry.GetLocalSize().X;
}

TSharedRef<SWidget> SShuttlegentMessageList::BuildEmptyState()
{
	return SNew(SBox)
		.Padding(FMargin(0.0f, ShuttlegentEditorMetric::EmptyStateTopPadding, 0.0f, ShuttlegentEditorMetric::Space4))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SAssignNew(EmptyStateMark, SShuttlegentMark)
				.Tier(EShuttlegentMarkTier::Large)
				.ColorAndOpacity_Lambda([this]()
				{
					const EShuttlegentAgentState State = Model->GetState();
					const bool bConnected = !Model->IsRuntimeLost() && (State == EShuttlegentAgentState::Ready || State == EShuttlegentAgentState::Prompting);
					return FSlateColor(bConnected ? FLinearColor::White : FLinearColor::White.CopyWithNewOpacity(ShuttlegentEditorMarkAnim::DisconnectedOpacity));
				})
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, ShuttlegentEditorMetric::Space4, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EmptyStateTitle", "今天要在编辑器里做什么？"))
				.Font(FShuttlegentStyle::DisplayFont())
				.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, ShuttlegentEditorMetric::Space2, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EmptyStateSubtitle", "从当前上下文开始，先看清楚，再执行。"))
				.Font(FShuttlegentStyle::BodyFont())
				.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
			]
		];
}

TSharedRef<SWidget> SShuttlegentMessageList::BuildScrollToEndButton()
{
	return SNew(SButton)
		.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::SecondaryButton))
		.Visibility_Lambda([this]()
		{
			return bHasUnseenContent && !IsScrolledToEnd() ? EVisibility::Visible : EVisibility::Collapsed;
		})
		.OnClicked_Lambda([this]()
		{
			ScrollToEnd();
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(LOCTEXT("ScrollToEnd", "新内容 · 回到底部 ↓"))
			.Font(FShuttlegentStyle::CaptionFont())
			.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
		];
}

TSharedRef<SWidget> SShuttlegentMessageList::BuildRow(const TSharedPtr<FShuttlegentChatEntry>& Entry)
{
	switch (Entry->Kind)
	{
	case EShuttlegentChatEntryKind::User:
		{
			return BuildUserRow(Entry);
		}
	case EShuttlegentChatEntryKind::Assistant:
		{
			return BuildAssistantRow(Entry);
		}
	case EShuttlegentChatEntryKind::ToolCall:
		{
			return BuildToolCallRow(Entry);
		}
	case EShuttlegentChatEntryKind::Permission:
		{
			return BuildPermissionRow(Entry);
		}
	case EShuttlegentChatEntryKind::TurnEnd:
	case EShuttlegentChatEntryKind::Notice:
	default:
		{
			return BuildNoticeRow(Entry);
		}
	}
}

TSharedRef<SWidget> SShuttlegentMessageList::BuildUserRow(const TSharedPtr<FShuttlegentChatEntry>& Entry)
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SSpacer)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.MaxDesiredWidth(this, &SShuttlegentMessageList::GetBubbleMaxWidth)
			[
				SNew(SBorder)
				.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::UserBubble))
				.Padding(FMargin(ShuttlegentEditorMetric::Space4, ShuttlegentEditorMetric::Space3))
				[
					SNew(STextBlock)
					.Text(FText::FromString(Entry->Text))
					.Font(FShuttlegentStyle::BodyFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
					.AutoWrapText(true)
				]
			]
		];
}

TSharedRef<SWidget> SShuttlegentMessageList::BuildAssistantRow(const TSharedPtr<FShuttlegentChatEntry>& Entry)
{
	TWeakPtr<FShuttlegentChatEntry> WeakEntry = Entry;
	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;

	const TSharedRef<SWidget> ThoughtToggle = SNew(SButton)
		.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
		.ContentPadding(0.0f)
		.ToolTipText(LOCTEXT("ThoughtToggle", "展开或收起思考过程"))
		.OnClicked_Lambda([WeakEntry]()
		{
			const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
			if (Pinned.IsValid()) Pinned->bThoughtExpanded = !Pinned->bThoughtExpanded;
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Font(FShuttlegentStyle::CaptionFont())
			.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
			.Text_Lambda([WeakEntry, WeakModel]()
			{
				const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
				const TSharedPtr<FShuttlegentChatModel> PinnedModel = WeakModel.Pin();
				if (!Pinned.IsValid() || !PinnedModel.IsValid()) return FText::GetEmpty();

				// Still thinking: the latest entry of a running turn with no reply text yet
				const TArray<TSharedPtr<FShuttlegentChatEntry>>& All = PinnedModel->GetEntries();
				const bool bThinking = Pinned->Text.IsEmpty() && PinnedModel->GetState() == EShuttlegentAgentState::Prompting && All.Num() > 0 && All.Last() == Pinned;
				if (bThinking) return LOCTEXT("Thinking", "思考中…");

				const FText Label = Pinned->ThoughtStartUtc.GetTicks() > 0
					? FText::Format(LOCTEXT("ThoughtSeconds", "已思考 {0} 秒"), FText::AsNumber(FMath::Max(1, FMath::CeilToInt((Pinned->ThoughtEndUtc - Pinned->ThoughtStartUtc).GetTotalSeconds()))))
					: LOCTEXT("ThoughtUntimed", "思考过程");
				return FText::Format(Pinned->bThoughtExpanded ? LOCTEXT("ThoughtExpanded", "{0}（收起）") : LOCTEXT("ThoughtCollapsed", "{0}（展开）"), Label);
			})
		];

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(ShuttlegentEditorMetric::MarkSizeAssistant)
				.HeightOverride(ShuttlegentEditorMetric::MarkSizeAssistant)
				[
					SNew(SImage)
					.Image(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Mark))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AssistantName", "Shuttlegent"))
				.Font(FShuttlegentStyle::CaptionFont())
				.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				FShuttlegentPanelWidgets::MakeIconButton(ShuttlegentEditorResource::IconCopy, LOCTEXT("Copy", "复制回答"), FOnClicked::CreateLambda([WeakEntry]()
				{
					const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
					if (Pinned.IsValid()) FPlatformApplicationMisc::ClipboardCopy(*Pinned->Text);
					return FReply::Handled();
				}))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, ShuttlegentEditorMetric::Space3, 0.0f, 0.0f)
		[
			SNew(SBox)
			.Visibility_Lambda([WeakEntry]()
			{
				const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
				return Pinned.IsValid() && !Pinned->Thought.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
			})
			[
				MakeNote(SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Left)
					[
						ThoughtToggle
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, ShuttlegentEditorMetric::Space1, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Font(FShuttlegentStyle::CaptionFont())
						.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
						.AutoWrapText(true)
						.Visibility_Lambda([WeakEntry]()
						{
							const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
							return Pinned.IsValid() && Pinned->bThoughtExpanded ? EVisibility::Visible : EVisibility::Collapsed;
						})
						.Text_Lambda([WeakEntry]()
						{
							const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
							return Pinned.IsValid() ? FText::FromString(Pinned->Thought) : FText::GetEmpty();
						})
					])
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, ShuttlegentEditorMetric::Space2, 0.0f, 0.0f)
		[
			SNew(SShuttlegentMarkdownView)
			.Entry(Entry)
		];
}

TSharedRef<SWidget> SShuttlegentMessageList::BuildToolCallRow(const TSharedPtr<FShuttlegentChatEntry>& Entry)
{
	TWeakPtr<FShuttlegentChatEntry> WeakEntry = Entry;
	const int32 EntryId = Entry->Id;

	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SButton)
			.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
			.ContentPadding(0.0f)
			.ToolTipText(LOCTEXT("ToolOpen", "在检视窗格查看参数与结果"))
			.OnClicked_Lambda([this, EntryId]()
			{
				OnToolSelected.ExecuteIfBound(EntryId);
				return FReply::Handled();
			})
			[
				SNew(SBorder)
				.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::ToolRow))
				.Padding(FMargin(ShuttlegentEditorMetric::Space3, ShuttlegentEditorMetric::Space2))
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SBox)
						.WidthOverride(ShuttlegentEditorMetric::IconSizeSmall)
						.HeightOverride(ShuttlegentEditorMetric::IconSizeSmall)
						[
							SNew(SImage)
							.Image_Lambda([WeakEntry]()
							{
								const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
								return SShuttlegentMessageListPrivate::GetStatusIcon(Pinned.IsValid() ? Pinned->Status : FString());
							})
							.ColorAndOpacity_Lambda([WeakEntry]()
							{
								const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
								return SShuttlegentMessageListPrivate::GetStatusTint(Pinned.IsValid() ? Pinned->Status : FString());
							})
						]
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Font(FShuttlegentStyle::CaptionFont())
						.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
						.Text_Lambda([WeakEntry]()
						{
							const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
							return Pinned.IsValid() ? FShuttlegentToolPresentation::GetDisplayName(Pinned->Title) : FText::GetEmpty();
						})
					]

					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					.Padding(ShuttlegentEditorMetric::Space2, 0.0f)
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

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Font(FShuttlegentStyle::CaptionFont())
						.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
						.Text_Lambda([WeakEntry]()
						{
							const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
							if (!Pinned.IsValid()) return FText::GetEmpty();

							const bool bFailed = Pinned->Status == SShuttlegentMessageListPrivate::StatusFailed;
							return SShuttlegentMessageListPrivate::GetToolTrailer(Pinned->Status, FShuttlegentToolPresentation::IsReadOnly(Pinned->Title), 1, bFailed ? 1 : 0);
						})
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(ShuttlegentEditorMetric::Space3, ShuttlegentEditorMetric::Space1, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Font(FShuttlegentStyle::CaptionFont())
			.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Error))
			.AutoWrapText(true)
			.Visibility_Lambda([WeakEntry]()
			{
				const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
				return Pinned.IsValid() && Pinned->Status == SShuttlegentMessageListPrivate::StatusFailed && !Pinned->Result.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
			})
			.Text_Lambda([WeakEntry]()
			{
				const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
				return Pinned.IsValid() ? FText::FromString(FShuttlegentToolPresentation::GetFailureReason(Pinned->Result)) : FText::GetEmpty();
			})
		];
}

TSharedRef<SWidget> SShuttlegentMessageList::BuildToolGroupRow(const TArray<TSharedPtr<FShuttlegentChatEntry>>& Group)
{
	const int32 FirstId = Group[0]->Id;
	TArray<TWeakPtr<FShuttlegentChatEntry>> WeakGroup;
	for (const TSharedPtr<FShuttlegentChatEntry>& Item : Group)
	{
		WeakGroup.Add(Item);
	}

	return SNew(SButton)
		.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
		.ContentPadding(0.0f)
		.ToolTipText(LOCTEXT("GroupOpen", "在检视窗格查看这组查询"))
		.OnClicked_Lambda([this, FirstId]()
		{
			OnToolSelected.ExecuteIfBound(FirstId);
			return FReply::Handled();
		})
		[
			SNew(SBorder)
			.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::ToolRow))
			.Padding(FMargin(ShuttlegentEditorMetric::Space3, ShuttlegentEditorMetric::Space2))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(ShuttlegentEditorMetric::IconSizeSmall)
					.HeightOverride(ShuttlegentEditorMetric::IconSizeSmall)
					[
						SNew(SImage)
						.Image_Lambda([WeakGroup]()
						{
							FString Status = SShuttlegentMessageListPrivate::StatusCompleted;
							for (const TWeakPtr<FShuttlegentChatEntry>& Weak : WeakGroup)
							{
								const TSharedPtr<FShuttlegentChatEntry> Pinned = Weak.Pin();
								if (Pinned.IsValid() && Pinned->Status != SShuttlegentMessageListPrivate::StatusCompleted) Status = Pinned->Status;
							}
							return SShuttlegentMessageListPrivate::GetStatusIcon(Status);
						})
						.ColorAndOpacity_Lambda([WeakGroup]()
						{
							FString Status = SShuttlegentMessageListPrivate::StatusCompleted;
							for (const TWeakPtr<FShuttlegentChatEntry>& Weak : WeakGroup)
							{
								const TSharedPtr<FShuttlegentChatEntry> Pinned = Weak.Pin();
								if (Pinned.IsValid() && Pinned->Status != SShuttlegentMessageListPrivate::StatusCompleted) Status = Pinned->Status;
							}
							return SShuttlegentMessageListPrivate::GetStatusTint(Status);
						})
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(ShuttlegentEditorMetric::Space2, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("GroupTitle", "已查询 {0} 项"), FText::AsNumber(Group.Num())))
					.Font(FShuttlegentStyle::CaptionFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Font(FShuttlegentStyle::CaptionFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
					.Text_Lambda([WeakGroup]()
					{
						int32 Failed = 0;
						for (const TWeakPtr<FShuttlegentChatEntry>& Weak : WeakGroup)
						{
							const TSharedPtr<FShuttlegentChatEntry> Pinned = Weak.Pin();
							if (!Pinned.IsValid()) continue;
							if (!SShuttlegentMessageListPrivate::IsToolFinished(Pinned->Status)) return SShuttlegentMessageListPrivate::GetToolTrailer(Pinned->Status, true, WeakGroup.Num(), 0);
							if (Pinned->Status == SShuttlegentMessageListPrivate::StatusFailed) ++Failed;
						}
						return SShuttlegentMessageListPrivate::GetToolTrailer(SShuttlegentMessageListPrivate::StatusCompleted, true, WeakGroup.Num(), Failed);
					})
				]
			]
		];
}

TSharedRef<SWidget> SShuttlegentMessageList::BuildPermissionRow(const TSharedPtr<FShuttlegentChatEntry>& Entry)
{
	TWeakPtr<FShuttlegentChatEntry> WeakEntry = Entry;
	const FText Tool = FShuttlegentToolPresentation::GetDisplayName(Entry->Title);

	return MakeNote(SNew(STextBlock)
		.Font(FShuttlegentStyle::CaptionFont())
		.AutoWrapText(true)
		.ColorAndOpacity_Lambda([WeakEntry]()
		{
			const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
			const bool bPending = Pinned.IsValid() && !Pinned->bAnswered;
			return FShuttlegentStyle::Tint(bPending ? ShuttlegentEditorColor::Warning : ShuttlegentEditorColor::Muted);
		})
		.Text_Lambda([WeakEntry, Tool]()
		{
			const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
			if (!Pinned.IsValid()) return FText::GetEmpty();

			if (!Pinned->bAnswered) return FText::Format(LOCTEXT("PermissionPending", "运行时请求额外权限：{0} · 等待本次授权"), Tool);
			return FText::Format(Pinned->bAllowed ? LOCTEXT("PermissionAllowed", "运行时请求额外权限：{0} · 已允许本次") : LOCTEXT("PermissionRejected", "运行时请求额外权限：{0} · 已拒绝"), Tool);
		}));
}

TSharedRef<SWidget> SShuttlegentMessageList::BuildNoticeRow(const TSharedPtr<FShuttlegentChatEntry>& Entry)
{
	return MakeNote(SNew(STextBlock)
		.Text(FText::FromString(Entry->Text))
		.Font(FShuttlegentStyle::CaptionFont())
		.ColorAndOpacity(FShuttlegentStyle::Tint(Entry->bError ? ShuttlegentEditorColor::Error : ShuttlegentEditorColor::Muted))
		.AutoWrapText(true));
}

TSharedRef<SWidget> SShuttlegentMessageList::MakeNote(const TSharedRef<SWidget>& Content) const
{
	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBox)
			.WidthOverride(ShuttlegentEditorMetric::AccentLineWidth)
			[
				SNew(SImage)
				.Image(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Divider))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(ShuttlegentEditorMetric::Space3, 0.0f, 0.0f, 0.0f)
		[
			Content
		];
}

void SShuttlegentMessageList::RebuildMessages()
{
	MessageList->ClearChildren();

	const bool bEmpty = Model->GetEntries().IsEmpty();
	if (bEmpty && EmptyState.IsValid())
	{
		MessageList->AddSlot()
			.AutoHeight()
			[
				EmptyState.ToSharedRef()
			];
	}
	if (bEmpty && !bShowingEmptyState && EmptyStateMark.IsValid()) EmptyStateMark->Play(EShuttlegentMarkMotion::Intro);
	bShowingEmptyState = bEmpty;

	const TArray<TSharedPtr<FShuttlegentChatEntry>>& Entries = Model->GetEntries();
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const TSharedPtr<FShuttlegentChatEntry>& Entry = Entries[Index];

		// Consecutive read-only tool calls fold into one group row
		TArray<TSharedPtr<FShuttlegentChatEntry>> Group;
		while (Index + Group.Num() < Entries.Num() && SShuttlegentMessageListPrivate::IsReadOnlyToolCall(Entries[Index + Group.Num()]))
		{
			Group.Add(Entries[Index + Group.Num()]);
		}
		if (Group.Num() >= ShuttlegentEditorTool::MinGroupSize)
		{
			MessageList->AddSlot()
				.AutoHeight()
				.Padding(0.0f, ShuttlegentEditorMetric::Space2, 0.0f, 0.0f)
				[
					BuildToolGroupRow(Group)
				];
			Index += Group.Num() - 1;
			continue;
		}

		const bool bSeparated = Entry->Kind == EShuttlegentChatEntryKind::User || Entry->Kind == EShuttlegentChatEntryKind::Assistant;
		MessageList->AddSlot()
			.AutoHeight()
			.Padding(0.0f, Index == 0 ? 0.0f : (bSeparated ? ShuttlegentEditorMetric::MessageSpacing : ShuttlegentEditorMetric::Space2), 0.0f, 0.0f)
			[
				BuildRow(Entry)
			];
	}
}

bool SShuttlegentMessageList::IsScrolledToEnd() const
{
	if (!ScrollBox.IsValid()) return true;

	return ScrollBox->GetScrollOffsetOfEnd() - ScrollBox->GetScrollOffset() <= SShuttlegentMessageListPrivate::FollowThreshold;
}

FOptionalSize SShuttlegentMessageList::GetColumnWidth() const
{
	const float Available = ViewWidth - 2.0f * ShuttlegentEditorMetric::ConversationPaddingSide;
	if (Available <= 0.0f) return FOptionalSize();

	return FMath::Min(Available, ShuttlegentEditorMetric::ConversationMaxWidth);
}

FOptionalSize SShuttlegentMessageList::GetBubbleMaxWidth() const
{
	const FOptionalSize Column = GetColumnWidth();
	if (!Column.IsSet()) return FOptionalSize();

	return Column.Get() * ShuttlegentEditorMetric::UserBubbleMaxShare;
}

void SShuttlegentMessageList::HandleStructureChanged()
{
	const bool bWasAtEnd = IsScrolledToEnd();
	RebuildMessages();
	if (bWasAtEnd && ScrollBox.IsValid()) ScrollBox->ScrollToEnd();
	if (!bWasAtEnd) bHasUnseenContent = true;
}

void SShuttlegentMessageList::HandleContentChanged()
{
	const bool bWasAtEnd = IsScrolledToEnd();
	if (bWasAtEnd && ScrollBox.IsValid()) ScrollBox->ScrollToEnd();
	if (!bWasAtEnd) bHasUnseenContent = true;
}

void SShuttlegentMessageList::HandleUserScrolled(float ScrollOffset)
{
	if (IsScrolledToEnd()) bHasUnseenContent = false;
}

#undef LOCTEXT_NAMESPACE
