#include "Panel/SShuttlegentComposer.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Chat/ShuttlegentChatModel.h"
#include "Panel/SShuttlegentElevationCard.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentStyle.h"

#define LOCTEXT_NAMESPACE "SShuttlegentComposer"

namespace SShuttlegentComposerPrivate
{
	// Config option ids offered as pickers
	inline const TCHAR* ConfigIdModel = TEXT("model");
	inline const TCHAR* ConfigIdReasoning = TEXT("reasoning_effort");

	// Widget switcher pages
	inline constexpr int32 PageComposer = 0;
	inline constexpr int32 PageElevation = 1;
}

void SShuttlegentComposer::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());
	OnSubmitted = InArgs._OnSubmitted;

	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;

	ChildSlot
	.HAlign(HAlign_Center)
	.Padding(FMargin(ShuttlegentEditorMetric::ConversationPaddingSide, 0.0f, ShuttlegentEditorMetric::ConversationPaddingSide, ShuttlegentEditorMetric::Space4))
	[
		SNew(SBox)
		.WidthOverride(this, &SShuttlegentComposer::GetColumnWidth)
		[
			SNew(SWidgetSwitcher)
			.WidgetIndex_Lambda([WeakModel]()
			{
				const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
				return Pinned.IsValid() && Pinned->GetPendingPermission().IsValid() ? SShuttlegentComposerPrivate::PageElevation : SShuttlegentComposerPrivate::PageComposer;
			})

			+ SWidgetSwitcher::Slot()
			[
				BuildInput()
			]

			+ SWidgetSwitcher::Slot()
			[
				SAssignNew(ElevationCard, SShuttlegentElevationCard)
				.Model(Model)
			]
		]
	];

	Model->OnStatusChanged.AddSP(this, &SShuttlegentComposer::HandleStatusChanged);
	RefreshConfigChoices();
}

SShuttlegentComposer::~SShuttlegentComposer()
{
	if (Model.IsValid()) Model->OnStatusChanged.RemoveAll(this);
}

void SShuttlegentComposer::FocusInput()
{
	if (InputBox.IsValid()) FSlateApplication::Get().SetKeyboardFocus(InputBox, EFocusCause::SetDirectly);
}

void SShuttlegentComposer::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	ViewWidth = AllottedGeometry.GetLocalSize().X;

	const TSharedPtr<FShuttlegentChatEntry> Pending = Model->GetPendingPermission();
	const int32 PendingId = Pending.IsValid() ? Pending->Id : 0;
	if (PendingId == ShownPermissionId) return;

	// Focus follows the decision only when it was in the composer, so a request never pulls focus out of the viewport
	const bool bHadFocus = HasFocusedDescendants();
	ShownPermissionId = PendingId;
	if (PendingId != 0)
	{
		ElevationCard->ShowRequest(Pending);
		if (bHadFocus) ElevationCard->FocusTitle();
		return;
	}

	if (bHadFocus) FocusInput();
}

TSharedRef<SWidget> SShuttlegentComposer::BuildInput()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage_Lambda([this]()
			{
				const bool bFocused = InputBox.IsValid() && InputBox->HasKeyboardFocus();
				return FShuttlegentStyle::Brush(bFocused ? ShuttlegentEditorStyleKey::ComposerFocused : ShuttlegentEditorStyleKey::Composer);
			})
			.Padding(FMargin(ShuttlegentEditorMetric::Space3, ShuttlegentEditorMetric::Space2))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(InputBox, SEditableTextBox)
					.Style(&FShuttlegentStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(ShuttlegentEditorStyleKey::InputBox))
					.HintText(LOCTEXT("InputHint", "描述你想在编辑器里完成的事…"))
					.ClearKeyboardFocusOnCommit(false)
					.OnTextChanged_Lambda([this](const FText& Text) { bHasDraft = !Text.ToString().TrimStartAndEnd().IsEmpty(); })
					.OnTextCommitted(this, &SShuttlegentComposer::OnInputCommitted)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, ShuttlegentEditorMetric::Space2, 0.0f, 0.0f)
				[
					BuildToolbar()
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(ShuttlegentEditorMetric::Space1, ShuttlegentEditorMetric::Space2, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("InputKeys", "Enter 发送 · Shift+Enter 编辑长文本 · @ 引用 / 模板"))
			.Font(FShuttlegentStyle::CaptionFont())
			.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
			.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
		];
}

TSharedRef<SWidget> SShuttlegentComposer::BuildToolbar()
{
	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;

	return SNew(SHorizontalBox)

		// Pickers shrink before the toolbar overflows at narrow widths
		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.MaxWidth(ShuttlegentEditorMetric::ComposerModelWidth)
		.VAlign(VAlign_Center)
		[
			BuildConfigCombo(ModelCombo, ModelChoices, SShuttlegentComposerPrivate::ConfigIdModel, &CurrentModelValue)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.MaxWidth(ShuttlegentEditorMetric::ComposerReasoningWidth)
		.VAlign(VAlign_Center)
		.Padding(ShuttlegentEditorMetric::Space1, 0.0f, 0.0f, 0.0f)
		[
			BuildConfigCombo(ReasoningCombo, ReasoningChoices, SShuttlegentComposerPrivate::ConfigIdReasoning, &CurrentReasoningValue)
		]

		+ SHorizontalBox::Slot()
		.FillWidth(1.0f)
		[
			SNew(SSpacer)
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ShuttlegentEditorMetric::Space2, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SShuttlegentComposer::GetUsageText)
			.ToolTipText(this, &SShuttlegentComposer::GetUsageToolTip)
			.Font(FShuttlegentStyle::CaptionFont())
			.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::PrimaryButton))
			.ToolTipText(LOCTEXT("Send", "发送 (Enter)"))
			.Visibility_Lambda([this]() { return IsPrompting() ? EVisibility::Collapsed : EVisibility::Visible; })
			.IsEnabled_Lambda([this]() { return bHasDraft; })
			.OnClicked_Lambda([this]()
			{
				SubmitInput();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SendLabel", "↑"))
				.Font(FShuttlegentStyle::EmphasisFont())
				.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::OnPrimary))
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::StopButton))
			.ToolTipText(LOCTEXT("StopTip", "停止本轮；已执行的修改不会回滚"))
			.Visibility_Lambda([this]() { return IsPrompting() ? EVisibility::Visible : EVisibility::Collapsed; })
			.IsEnabled_Lambda([WeakModel]()
			{
				const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
				return Pinned.IsValid() && !Pinned->IsStopping();
			})
			.OnClicked_Lambda([WeakModel]()
			{
				const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
				if (Pinned.IsValid()) Pinned->Cancel();
				return FReply::Handled();
			})
			[
				SNew(STextBlock)
				.Font(FShuttlegentStyle::CaptionFont())
				.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
				.Text_Lambda([WeakModel]()
				{
					const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
					return Pinned.IsValid() && Pinned->IsStopping() ? LOCTEXT("Stopping", "停止中") : LOCTEXT("Stop", "停止");
				})
			]
		];
}

TSharedRef<SWidget> SShuttlegentComposer::BuildConfigCombo(TSharedPtr<SComboBox<FChoicePtr>>& OutCombo, TArray<FChoicePtr>& Choices, const FString& ConfigId, FString* CurrentValue)
{
	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;
	return SNew(SBox)
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
			.IsEnabled_Lambda([this, &Choices]() { return Choices.Num() > 0 && !IsPrompting(); })
			[
				SNew(STextBlock)
				.Font(FShuttlegentStyle::CaptionFont())
				.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
				.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
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

void SShuttlegentComposer::RefreshConfigChoices()
{
	TArray<FShuttlegentConfigChoice> Choices;

	Model->GetConfigChoices(SShuttlegentComposerPrivate::ConfigIdModel, Choices, CurrentModelValue);
	ModelChoices.Reset();
	for (const FShuttlegentConfigChoice& Choice : Choices)
	{
		ModelChoices.Add(MakeShared<FShuttlegentConfigChoice>(Choice));
	}

	Model->GetConfigChoices(SShuttlegentComposerPrivate::ConfigIdReasoning, Choices, CurrentReasoningValue);
	ReasoningChoices.Reset();
	for (const FShuttlegentConfigChoice& Choice : Choices)
	{
		ReasoningChoices.Add(MakeShared<FShuttlegentConfigChoice>(Choice));
	}

	if (ModelCombo.IsValid()) ModelCombo->RefreshOptions();
	if (ReasoningCombo.IsValid()) ReasoningCombo->RefreshOptions();
}

void SShuttlegentComposer::SubmitInput()
{
	const FString Text = InputBox->GetText().ToString();
	if (Text.TrimStartAndEnd().IsEmpty() || IsPrompting()) return;

	InputBox->SetText(FText::GetEmpty());
	bHasDraft = false;
	Model->SubmitPrompt(Text);
	OnSubmitted.ExecuteIfBound();
}

void SShuttlegentComposer::OnInputCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (CommitType == ETextCommit::OnEnter) SubmitInput();
}

FOptionalSize SShuttlegentComposer::GetColumnWidth() const
{
	const float Available = ViewWidth - 2.0f * ShuttlegentEditorMetric::ConversationPaddingSide;
	if (Available <= 0.0f) return FOptionalSize();

	return FMath::Min(Available, ShuttlegentEditorMetric::ConversationMaxWidth);
}

FText SShuttlegentComposer::GetUsageText() const
{
	if (Model->GetContextSize() <= 0) return LOCTEXT("UsageUnknown", "—");

	FNumberFormattingOptions Options;
	Options.MaximumFractionalDigits = 0;
	const double Percent = static_cast<double>(Model->GetUsedTokens()) / static_cast<double>(Model->GetContextSize()) * ShuttlegentEditorMetric::PercentScale;
	return FText::Format(LOCTEXT("UsagePercent", "{0}%"), FText::AsNumber(Percent, &Options));
}

FText SShuttlegentComposer::GetUsageToolTip() const
{
	if (Model->GetContextSize() <= 0) return LOCTEXT("UsageUnknownTip", "上下文用量未知");

	return FText::Format(LOCTEXT("UsageTip", "已用 {0} / {1} tokens"), FText::AsNumber(Model->GetUsedTokens()), FText::AsNumber(Model->GetContextSize()));
}

bool SShuttlegentComposer::IsPrompting() const
{
	return Model->GetState() == EShuttlegentAgentState::Prompting;
}

void SShuttlegentComposer::HandleStatusChanged()
{
	RefreshConfigChoices();
}

#undef LOCTEXT_NAMESPACE
