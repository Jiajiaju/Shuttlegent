#include "SShuttlegentChatPanel.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Chat/ShuttlegentChatModel.h"
#include "Mark/EShuttlegentMarkMotion.h"
#include "Mark/EShuttlegentMarkTier.h"
#include "Mark/SShuttlegentMark.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentStyle.h"

#define LOCTEXT_NAMESPACE "SShuttlegentChatPanel"

namespace SShuttlegentChatPanelPrivate
{
	// Config option ids offered as pickers
	inline const TCHAR* ConfigIdModel = TEXT("model");
	inline const TCHAR* ConfigIdReasoning = TEXT("reasoning_effort");

	// Tool call statuses reported by the runtime
	inline const TCHAR* StatusCompleted = TEXT("completed");
	inline const TCHAR* StatusFailed = TEXT("failed");

	// Icons
	inline const TCHAR* IconPlus = TEXT("plus");
	inline const TCHAR* IconHistory = TEXT("history");
	inline const TCHAR* IconSettings = TEXT("settings");
	inline const TCHAR* IconSend = TEXT("send");
	inline const TCHAR* IconStop = TEXT("stop");
	inline const TCHAR* IconCopy = TEXT("copy");
	inline const TCHAR* IconCheck = TEXT("check");
	inline const TCHAR* IconAlert = TEXT("alert");
	inline const TCHAR* IconChevron = TEXT("chevron");
	inline const TCHAR* IconTools = TEXT("tools");

	// Distance from the bottom, in Slate units, within which new content keeps the view pinned to the end
	inline constexpr float FollowThreshold = 24.0f;

	// Token counts above this are shown in thousands
	inline constexpr int64 TokensPerThousand = 1000;

	FSlateColor Tint(const FColor& Token)
	{
		return FSlateColor(FShuttlegentStyle::Color(Token));
	}

	FText FormatTokens(int64 Tokens)
	{
		if (Tokens < TokensPerThousand) return FText::AsNumber(Tokens);

		FNumberFormattingOptions Options;
		Options.MaximumFractionalDigits = 1;
		return FText::Format(LOCTEXT("ThousandTokens", "{0}k"), FText::AsNumber(static_cast<double>(Tokens) / TokensPerThousand, &Options));
	}
}

void SShuttlegentChatPanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());

	EmptyState = BuildEmptyState();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Background))
		.Padding(0.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildHeader()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(ShuttlegentEditorMetric::BorderWidth)
				[
					SNew(SImage)
					.Image(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Divider))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildStatusRow()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				BuildBanner()
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(ShuttlegentEditorMetric::Space4, 0.0f)
			[
				SAssignNew(ScrollBox, SScrollBox)
				+ SScrollBox::Slot()
				[
					SAssignNew(MessageList, SVerticalBox)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(ShuttlegentEditorMetric::Space4)
			[
				BuildInput()
			]
		]
	];

	Model->OnStructureChanged.AddSP(this, &SShuttlegentChatPanel::HandleStructureChanged);
	Model->OnContentChanged.AddSP(this, &SShuttlegentChatPanel::HandleContentChanged);
	Model->OnStatusChanged.AddSP(this, &SShuttlegentChatPanel::HandleStatusChanged);
	Model->OnTurnFinished.AddSP(this, &SShuttlegentChatPanel::HandleTurnFinished);
	Model->SetPermissionHandlerAttached(true);

	RefreshConfigChoices();
	RebuildMessages();
	ScrollBox->ScrollToEnd();

	bWasPrompting = IsPrompting();
	if (bWasPrompting && HeaderMark.IsValid()) HeaderMark->Play(EShuttlegentMarkMotion::Working);

	Model->EnsureRuntime();
}

SShuttlegentChatPanel::~SShuttlegentChatPanel()
{
	if (!Model.IsValid()) return;

	Model->OnStructureChanged.RemoveAll(this);
	Model->OnContentChanged.RemoveAll(this);
	Model->OnStatusChanged.RemoveAll(this);
	Model->OnTurnFinished.RemoveAll(this);
	Model->SetPermissionHandlerAttached(false);
}

TSharedRef<SWidget> SShuttlegentChatPanel::BuildHeader()
{
	return SNew(SBox)
		.Padding(FMargin(ShuttlegentEditorMetric::Space4, ShuttlegentEditorMetric::Space3))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(HeaderMark, SShuttlegentMark)
				.Tier(EShuttlegentMarkTier::Small)
				.ColorAndOpacity(this, &SShuttlegentChatPanel::GetMarkColor)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Title", "Shuttlegent"))
				.Font(FShuttlegentStyle::SectionFont())
				.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Text))
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
				MakeIconButton(SShuttlegentChatPanelPrivate::IconPlus, LOCTEXT("NewConversation", "新建会话"), FOnClicked::CreateSP(this, &SShuttlegentChatPanel::OnNewConversationClicked))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorMetric::Space1, 0.0f)
			[
				SNew(SComboButton)
				.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::IconButton))
				.HasDownArrow(false)
				.ToolTipText(LOCTEXT("History", "历史会话"))
				.OnGetMenuContent(this, &SShuttlegentChatPanel::BuildHistoryMenu)
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FShuttlegentStyle::Icon(SShuttlegentChatPanelPrivate::IconHistory))
					.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Muted))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				MakeIconButton(SShuttlegentChatPanelPrivate::IconSettings, LOCTEXT("Settings", "设置"), FOnClicked::CreateSP(this, &SShuttlegentChatPanel::OnSettingsClicked))
			]
		];
}

TSharedRef<SWidget> SShuttlegentChatPanel::BuildStatusRow()
{
	return SNew(SBox)
		.Padding(FMargin(ShuttlegentEditorMetric::Space4, ShuttlegentEditorMetric::Space2))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				BuildConfigCombo(ModelCombo, ModelChoices, SShuttlegentChatPanelPrivate::ConfigIdModel, &CurrentModelValue)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
			[
				BuildConfigCombo(ReasoningCombo, ReasoningChoices, SShuttlegentChatPanelPrivate::ConfigIdReasoning, &CurrentReasoningValue)
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
				SNew(SImage)
				.Image(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::StatusDot))
				.ColorAndOpacity(this, &SShuttlegentChatPanel::GetStatusColor)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SShuttlegentChatPanel::GetStatusText)
				.Font(FShuttlegentStyle::CaptionFont())
				.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Muted))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorMetric::Space3, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SShuttlegentChatPanel::GetUsageText)
				.Font(FShuttlegentStyle::CaptionFont())
				.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Muted))
			]
		];
}

TSharedRef<SWidget> SShuttlegentChatPanel::BuildBanner()
{
	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;
	return SNew(SBox)
		.Padding(FMargin(ShuttlegentEditorMetric::Space4, 0.0f, ShuttlegentEditorMetric::Space4, ShuttlegentEditorMetric::Space2))
		.Visibility_Lambda([WeakModel]()
		{
			const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
			return Pinned.IsValid() && Pinned->IsRuntimeLost() ? EVisibility::Visible : EVisibility::Collapsed;
		})
		[
			SNew(SBorder)
			.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Raised))
			.Padding(ShuttlegentEditorMetric::Space3)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SImage)
					.Image(FShuttlegentStyle::Icon(SShuttlegentChatPanelPrivate::IconAlert))
					.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Error))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.Padding(ShuttlegentEditorMetric::Space2, 0.0f)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RuntimeLostBanner", "运行时已断开。"))
					.Font(FShuttlegentStyle::BodyFont())
					.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Text))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::SecondaryButton))
					.OnClicked(this, &SShuttlegentChatPanel::OnReconnectClicked)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Reconnect", "重新连接"))
						.Font(FShuttlegentStyle::BodyFont())
						.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Text))
					]
				]
			]
		];
}

TSharedRef<SWidget> SShuttlegentChatPanel::BuildInput()
{
	return SNew(SBorder)
		.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Input))
		.Padding(FMargin(ShuttlegentEditorMetric::Space3, ShuttlegentEditorMetric::Space2))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(InputBox, SEditableTextBox)
				.Style(&FShuttlegentStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(ShuttlegentEditorStyleKey::InputBox))
				.HintText(LOCTEXT("InputHint", "描述你想完成的操作…"))
				.ClearKeyboardFocusOnCommit(false)
				.OnTextCommitted(this, &SShuttlegentChatPanel::OnInputCommitted)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::PrimaryButton))
				.ContentPadding(ShuttlegentEditorMetric::Space1)
				.ToolTipText_Lambda([this]() { return IsPrompting() ? LOCTEXT("Stop", "停止") : LOCTEXT("Send", "发送"); })
				.OnClicked(this, &SShuttlegentChatPanel::OnSendClicked)
				[
					SNew(SImage)
					.Image(this, &SShuttlegentChatPanel::GetSendIcon)
					.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::OnPrimary))
				]
			]
		];
}

TSharedRef<SWidget> SShuttlegentChatPanel::BuildConfigCombo(TSharedPtr<SComboBox<FChoicePtr>>& OutCombo, TArray<FChoicePtr>& Choices, const FString& ConfigId, FString* CurrentValue)
{
	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;
	return SNew(SBox)
		.WidthOverride(ShuttlegentEditorMetric::PickerWidth)
		[
			SAssignNew(OutCombo, SComboBox<FChoicePtr>)
			.OptionsSource(&Choices)
			.OnGenerateWidget_Lambda([](FChoicePtr Choice) -> TSharedRef<SWidget>
			{
				return SNew(STextBlock)
					.Text(FText::FromString(Choice.IsValid() ? Choice->Name : FString()))
					.Font(FShuttlegentStyle::CaptionFont());
			})
			.OnSelectionChanged_Lambda([WeakModel, ConfigId](FChoicePtr Choice, ESelectInfo::Type SelectInfo)
			{
				const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
				if (Pinned.IsValid() && Choice.IsValid() && SelectInfo != ESelectInfo::Direct) Pinned->SetConfigValue(ConfigId, Choice->Value);
			})
			.IsEnabled_Lambda([&Choices]() { return Choices.Num() > 0; })
			[
				SNew(STextBlock)
				.Font(FShuttlegentStyle::CaptionFont())
				.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Text))
				.Text_Lambda([&Choices, CurrentValue]()
				{
					for (const FChoicePtr& Choice : Choices)
					{
						if (Choice.IsValid() && Choice->Value == *CurrentValue) return FText::FromString(Choice->Name);
					}
					return CurrentValue->IsEmpty() ? LOCTEXT("NoOption", "—") : FText::FromString(*CurrentValue);
				})
			]
		];
}

TSharedRef<SWidget> SShuttlegentChatPanel::BuildEmptyState()
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
				.ColorAndOpacity(this, &SShuttlegentChatPanel::GetMarkColor)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, ShuttlegentEditorMetric::Space4, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EmptyStateTitle", "今天要在编辑器里做什么？"))
				.Font(FShuttlegentStyle::SectionFont())
				.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Text))
			]
		];
}

TSharedRef<SWidget> SShuttlegentChatPanel::BuildHistoryMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);
	const TArray<TPair<FString, FString>> Sessions = Model->GetRecentSessions();
	if (Sessions.Num() == 0)
	{
		MenuBuilder.AddWidget(SNew(STextBlock).Text(LOCTEXT("NoHistory", "暂无历史会话")), FText::GetEmpty());
		return MenuBuilder.MakeWidget();
	}

	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;
	for (const TPair<FString, FString>& Session : Sessions)
	{
		const FString SessionId = Session.Key;
		MenuBuilder.AddMenuEntry(FText::FromString(Session.Value), FText::FromString(SessionId), FSlateIcon(), FUIAction(FExecuteAction::CreateLambda([WeakModel, SessionId]()
		{
			const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
			if (Pinned.IsValid()) Pinned->OpenSession(SessionId);
		})));
	}
	return MenuBuilder.MakeWidget();
}

TSharedRef<SWidget> SShuttlegentChatPanel::BuildRow(const TSharedPtr<FShuttlegentChatEntry>& Entry)
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

TSharedRef<SWidget> SShuttlegentChatPanel::BuildUserRow(const TSharedPtr<FShuttlegentChatEntry>& Entry)
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("You", "你"))
			.Font(FShuttlegentStyle::CaptionFont())
			.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Muted))
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, ShuttlegentEditorMetric::Space1, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Entry->Text))
			.Font(FShuttlegentStyle::SectionFont())
			.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Text))
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SShuttlegentChatPanel::BuildAssistantRow(const TSharedPtr<FShuttlegentChatEntry>& Entry)
{
	TWeakPtr<FShuttlegentChatEntry> WeakEntry = Entry;
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
				.WidthOverride(ShuttlegentEditorMetric::MarkSizeInline)
				.HeightOverride(ShuttlegentEditorMetric::MarkSizeInline)
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
				.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Muted))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorMetric::Space3, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::IconButton))
				.ContentPadding(0.0f)
				.Visibility_Lambda([WeakEntry]()
				{
					const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
					return Pinned.IsValid() && !Pinned->Thought.IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.OnClicked_Lambda([WeakEntry]()
				{
					const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
					if (Pinned.IsValid()) Pinned->bThoughtExpanded = !Pinned->bThoughtExpanded;
					return FReply::Handled();
				})
				[
					SNew(STextBlock)
					.Font(FShuttlegentStyle::CaptionFont())
					.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Muted))
					.Text_Lambda([WeakEntry]()
					{
						const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
						const int32 Chars = Pinned.IsValid() ? Pinned->Thought.Len() : 0;
						const bool bExpanded = Pinned.IsValid() && Pinned->bThoughtExpanded;
						return FText::Format(bExpanded ? LOCTEXT("ThoughtExpanded", "思考 {0} 字（收起）") : LOCTEXT("ThoughtCollapsed", "思考 {0} 字（展开）"), FText::AsNumber(Chars));
					})
				]
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
				MakeIconButton(SShuttlegentChatPanelPrivate::IconCopy, LOCTEXT("Copy", "复制"), FOnClicked::CreateLambda([WeakEntry]()
				{
					const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
					if (Pinned.IsValid()) FPlatformApplicationMisc::ClipboardCopy(*Pinned->Text);
					return FReply::Handled();
				}))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, ShuttlegentEditorMetric::Space1, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Font(FShuttlegentStyle::CaptionFont())
			.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Muted))
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
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, ShuttlegentEditorMetric::Space2, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Font(FShuttlegentStyle::BodyFont())
			.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Text))
			.AutoWrapText(true)
			.Text_Lambda([WeakEntry]()
			{
				const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
				return Pinned.IsValid() ? FText::FromString(Pinned->Text) : FText::GetEmpty();
			})
		];
}

TSharedRef<SWidget> SShuttlegentChatPanel::BuildToolCallRow(const TSharedPtr<FShuttlegentChatEntry>& Entry)
{
	TWeakPtr<FShuttlegentChatEntry> WeakEntry = Entry;
	return SNew(SBorder)
		.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Surface))
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
						const FString Status = Pinned.IsValid() ? Pinned->Status : FString();
						if (Status == SShuttlegentChatPanelPrivate::StatusCompleted) return FShuttlegentStyle::Icon(SShuttlegentChatPanelPrivate::IconCheck);
						if (Status == SShuttlegentChatPanelPrivate::StatusFailed) return FShuttlegentStyle::Icon(SShuttlegentChatPanelPrivate::IconAlert);
						return FShuttlegentStyle::Icon(SShuttlegentChatPanelPrivate::IconTools);
					})
					.ColorAndOpacity_Lambda([WeakEntry]()
					{
						const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
						const FString Status = Pinned.IsValid() ? Pinned->Status : FString();
						if (Status == SShuttlegentChatPanelPrivate::StatusCompleted) return SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Success);
						if (Status == SShuttlegentChatPanelPrivate::StatusFailed) return SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Error);
						return SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Muted);
					})
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorMetric::Space2, 0.0f)
			[
				SNew(STextBlock)
				.Font(FShuttlegentStyle::BodyFont())
				.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Text))
				.Text_Lambda([WeakEntry]()
				{
					const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
					return Pinned.IsValid() ? FText::FromString(Pinned->Title) : FText::GetEmpty();
				})
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(FShuttlegentStyle::CaptionFont())
				.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Muted))
				.Text_Lambda([WeakEntry]()
				{
					const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
					const FString Status = Pinned.IsValid() ? Pinned->Status : FString();
					if (Status == SShuttlegentChatPanelPrivate::StatusCompleted) return LOCTEXT("ToolCompleted", "已完成");
					if (Status == SShuttlegentChatPanelPrivate::StatusFailed) return LOCTEXT("ToolFailed", "执行失败");
					return LOCTEXT("ToolRunning", "正在执行");
				})
			]
		];
}

TSharedRef<SWidget> SShuttlegentChatPanel::BuildPermissionRow(const TSharedPtr<FShuttlegentChatEntry>& Entry)
{
	TWeakPtr<FShuttlegentChatEntry> WeakEntry = Entry;
	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;
	const int32 EntryId = Entry->Id;
	const FString Description = Entry->Detail.IsEmpty() ? Entry->Title : Entry->Title + TEXT(" · ") + Entry->Detail;

	return SNew(SBorder)
		.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Raised))
		.Padding(ShuttlegentEditorMetric::Space4)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PermissionTitle", "操作确认"))
				.Font(FShuttlegentStyle::EmphasisFont())
				.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Text))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, ShuttlegentEditorMetric::Space2)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Description))
				.Font(FShuttlegentStyle::CaptionFont())
				.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Muted))
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)
				.Visibility_Lambda([WeakEntry]()
				{
					const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
					return Pinned.IsValid() && !Pinned->bAnswered ? EVisibility::Visible : EVisibility::Collapsed;
				})

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::SecondaryButton))
					.OnClicked_Lambda([WeakModel, EntryId]()
					{
						const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
						if (Pinned.IsValid()) Pinned->AnswerPermission(EntryId, false);
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Reject", "取消"))
						.Font(FShuttlegentStyle::BodyFont())
						.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Text))
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::PrimaryButton))
					.OnClicked_Lambda([WeakModel, EntryId]()
					{
						const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
						if (Pinned.IsValid()) Pinned->AnswerPermission(EntryId, true);
						return FReply::Handled();
					})
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Allow", "确认执行"))
						.Font(FShuttlegentStyle::BodyFont())
						.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::OnPrimary))
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Font(FShuttlegentStyle::CaptionFont())
				.Visibility_Lambda([WeakEntry]()
				{
					const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
					return Pinned.IsValid() && Pinned->bAnswered ? EVisibility::Visible : EVisibility::Collapsed;
				})
				.ColorAndOpacity_Lambda([WeakEntry]()
				{
					const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
					return SShuttlegentChatPanelPrivate::Tint(Pinned.IsValid() && Pinned->bAllowed ? ShuttlegentEditorColor::Success : ShuttlegentEditorColor::Muted);
				})
				.Text_Lambda([WeakEntry]()
				{
					const TSharedPtr<FShuttlegentChatEntry> Pinned = WeakEntry.Pin();
					return Pinned.IsValid() && Pinned->bAllowed ? LOCTEXT("Allowed", "已允许") : LOCTEXT("Rejected", "已拒绝");
				})
			]
		];
}

TSharedRef<SWidget> SShuttlegentChatPanel::BuildNoticeRow(const TSharedPtr<FShuttlegentChatEntry>& Entry)
{
	const FColor& Token = Entry->bError ? ShuttlegentEditorColor::Error : ShuttlegentEditorColor::Muted;
	const TCHAR* IconName = Entry->bError ? SShuttlegentChatPanelPrivate::IconAlert : SShuttlegentChatPanelPrivate::IconChevron;

	return SNew(SHorizontalBox)

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Top)
		[
			SNew(SBox)
			.WidthOverride(ShuttlegentEditorMetric::IconSizeSmall)
			.HeightOverride(ShuttlegentEditorMetric::IconSizeSmall)
			[
				SNew(SImage)
				.Image(FShuttlegentStyle::Icon(IconName))
				.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(Token))
			]
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(Entry->Text))
			.Font(FShuttlegentStyle::CaptionFont())
			.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(Token))
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SShuttlegentChatPanel::MakeIconButton(const TCHAR* IconName, const FText& ToolTip, FOnClicked OnClicked) const
{
	return SNew(SButton)
		.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::IconButton))
		.ContentPadding(ShuttlegentEditorMetric::Space1)
		.ToolTipText(ToolTip)
		.OnClicked(OnClicked)
		[
			SNew(SBox)
			.WidthOverride(ShuttlegentEditorMetric::IconSizeSmall)
			.HeightOverride(ShuttlegentEditorMetric::IconSizeSmall)
			[
				SNew(SImage)
				.Image(FShuttlegentStyle::Icon(IconName))
				.ColorAndOpacity(SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Muted))
			]
		];
}

void SShuttlegentChatPanel::RebuildMessages()
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

	for (const TSharedPtr<FShuttlegentChatEntry>& Entry : Model->GetEntries())
	{
		const bool bSeparated = Entry->Kind == EShuttlegentChatEntryKind::User || Entry->Kind == EShuttlegentChatEntryKind::Assistant;
		MessageList->AddSlot()
			.AutoHeight()
			.Padding(0.0f, bSeparated ? ShuttlegentEditorMetric::Space4 : ShuttlegentEditorMetric::Space2, 0.0f, 0.0f)
			[
				BuildRow(Entry)
			];
	}
}

void SShuttlegentChatPanel::RefreshConfigChoices()
{
	TArray<FShuttlegentConfigChoice> Choices;

	Model->GetConfigChoices(SShuttlegentChatPanelPrivate::ConfigIdModel, Choices, CurrentModelValue);
	ModelChoices.Reset();
	for (const FShuttlegentConfigChoice& Choice : Choices)
	{
		ModelChoices.Add(MakeShared<FShuttlegentConfigChoice>(Choice));
	}

	Model->GetConfigChoices(SShuttlegentChatPanelPrivate::ConfigIdReasoning, Choices, CurrentReasoningValue);
	ReasoningChoices.Reset();
	for (const FShuttlegentConfigChoice& Choice : Choices)
	{
		ReasoningChoices.Add(MakeShared<FShuttlegentConfigChoice>(Choice));
	}

	if (ModelCombo.IsValid()) ModelCombo->RefreshOptions();
	if (ReasoningCombo.IsValid()) ReasoningCombo->RefreshOptions();
}

void SShuttlegentChatPanel::ScrollToEndIfFollowing(bool bWasAtEnd)
{
	if (bWasAtEnd && ScrollBox.IsValid()) ScrollBox->ScrollToEnd();
}

bool SShuttlegentChatPanel::IsScrolledToEnd() const
{
	if (!ScrollBox.IsValid()) return true;
	return ScrollBox->GetScrollOffsetOfEnd() - ScrollBox->GetScrollOffset() <= SShuttlegentChatPanelPrivate::FollowThreshold;
}

void SShuttlegentChatPanel::HandleStructureChanged()
{
	const bool bWasAtEnd = IsScrolledToEnd();
	RebuildMessages();
	ScrollToEndIfFollowing(bWasAtEnd);
}

void SShuttlegentChatPanel::HandleContentChanged()
{
	ScrollToEndIfFollowing(IsScrolledToEnd());
}

void SShuttlegentChatPanel::HandleStatusChanged()
{
	RefreshConfigChoices();

	if (!HeaderMark.IsValid()) return;

	const bool bPrompting = IsPrompting();
	if (bPrompting && !bWasPrompting) HeaderMark->Play(EShuttlegentMarkMotion::Working);

	// Left the turn without a turn-finished event, e.g. the runtime was lost
	if (!bPrompting && bWasPrompting && HeaderMark->GetMotion() == EShuttlegentMarkMotion::Working) HeaderMark->ShowRest();
	bWasPrompting = bPrompting;
}

void SShuttlegentChatPanel::HandleTurnFinished(bool bCompleted)
{
	if (!HeaderMark.IsValid()) return;

	if (bCompleted) HeaderMark->Play(EShuttlegentMarkMotion::Done);
	else HeaderMark->ShowRest();
}

void SShuttlegentChatPanel::SubmitInput()
{
	const FString Text = InputBox->GetText().ToString();
	if (Text.TrimStartAndEnd().IsEmpty()) return;

	InputBox->SetText(FText::GetEmpty());
	Model->SubmitPrompt(Text);
	ScrollBox->ScrollToEnd();
}

FReply SShuttlegentChatPanel::OnSendClicked()
{
	if (IsPrompting())
	{
		Model->Cancel();
		return FReply::Handled();
	}

	SubmitInput();
	return FReply::Handled();
}

FReply SShuttlegentChatPanel::OnNewConversationClicked()
{
	Model->NewConversation();
	return FReply::Handled();
}

FReply SShuttlegentChatPanel::OnSettingsClicked()
{
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings"));
	if (SettingsModule != nullptr) SettingsModule->ShowViewer(ShuttlegentEditorName::SettingsContainer, ShuttlegentEditorName::SettingsCategory, ShuttlegentEditorName::SettingsSection);
	return FReply::Handled();
}

FReply SShuttlegentChatPanel::OnReconnectClicked()
{
	Model->Reconnect();
	return FReply::Handled();
}

void SShuttlegentChatPanel::OnInputCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter && !IsPrompting()) SubmitInput();
}

FText SShuttlegentChatPanel::GetStatusText() const
{
	if (Model->IsRuntimeLost()) return LOCTEXT("StatusLost", "已断开");

	switch (Model->GetState())
	{
	case EShuttlegentAgentState::Starting:
		{
			return LOCTEXT("StatusStarting", "连接中");
		}
	case EShuttlegentAgentState::Ready:
		{
			return LOCTEXT("StatusReady", "已连接");
		}
	case EShuttlegentAgentState::Prompting:
		{
			return LOCTEXT("StatusPrompting", "正在生成");
		}
	case EShuttlegentAgentState::Stopped:
	default:
		{
			return LOCTEXT("StatusStopped", "未连接");
		}
	}
}

FSlateColor SShuttlegentChatPanel::GetStatusColor() const
{
	if (Model->IsRuntimeLost()) return SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Error);

	switch (Model->GetState())
	{
	case EShuttlegentAgentState::Starting:
		{
			return SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Warning);
		}
	case EShuttlegentAgentState::Ready:
		{
			return SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Success);
		}
	case EShuttlegentAgentState::Prompting:
		{
			return SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Info);
		}
	case EShuttlegentAgentState::Stopped:
	default:
		{
			return SShuttlegentChatPanelPrivate::Tint(ShuttlegentEditorColor::Muted);
		}
	}
}

FSlateColor SShuttlegentChatPanel::GetMarkColor() const
{
	const EShuttlegentAgentState State = Model->GetState();
	const bool bConnected = !Model->IsRuntimeLost() && (State == EShuttlegentAgentState::Ready || State == EShuttlegentAgentState::Prompting);
	return FSlateColor(bConnected ? FLinearColor::White : FLinearColor::White.CopyWithNewOpacity(ShuttlegentEditorMarkAnim::DisconnectedOpacity));
}

FText SShuttlegentChatPanel::GetUsageText() const
{
	if (Model->GetContextSize() <= 0) return FText::GetEmpty();

	return FText::Format(LOCTEXT("Usage", "上下文 {0} / {1}"), SShuttlegentChatPanelPrivate::FormatTokens(Model->GetUsedTokens()), SShuttlegentChatPanelPrivate::FormatTokens(Model->GetContextSize()));
}

const FSlateBrush* SShuttlegentChatPanel::GetSendIcon() const
{
	return FShuttlegentStyle::Icon(IsPrompting() ? SShuttlegentChatPanelPrivate::IconStop : SShuttlegentChatPanelPrivate::IconSend);
}

bool SShuttlegentChatPanel::IsPrompting() const
{
	return Model->GetState() == EShuttlegentAgentState::Prompting;
}

#undef LOCTEXT_NAMESPACE
