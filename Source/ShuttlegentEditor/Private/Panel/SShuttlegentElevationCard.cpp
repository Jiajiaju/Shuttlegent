#include "Panel/SShuttlegentElevationCard.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Chat/FShuttlegentToolPresentation.h"
#include "Chat/ShuttlegentChatModel.h"
#include "Panel/FShuttlegentPanelWidgets.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentStyle.h"

#define LOCTEXT_NAMESPACE "SShuttlegentElevationCard"

namespace SShuttlegentElevationCardPrivate
{
	TSharedRef<SWidget> MakeLine(const FText& Text, const FSlateFontInfo& Font, const FColor& Color)
	{
		return SNew(STextBlock)
			.Text(Text)
			.Font(Font)
			.ColorAndOpacity(FShuttlegentStyle::Tint(Color))
			.AutoWrapText(true);
	}
}

void SShuttlegentElevationCard::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::DecisionCard))
		.Padding(ShuttlegentEditorMetric::Space4)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					// Focus target: activating it runs nothing, so an Enter meant for the input cannot approve
					SAssignNew(Title, SButton)
					.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
					.ContentPadding(0.0f)
					.OnClicked_Lambda([]() { return FReply::Handled(); })
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Title", "运行时请求额外权限"))
						.Font(FShuttlegentStyle::EmphasisFont())
						.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Irreversible", "不可撤销"))
					.Font(FShuttlegentStyle::CaptionFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Warning))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, ShuttlegentEditorMetric::Space2)
			[
				SAssignNew(Body, SBox)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					FShuttlegentPanelWidgets::MakeTextButton(LOCTEXT("Reject", "拒绝"), false, FOnClicked::CreateLambda([this]()
					{
						Answer(false);
						return FReply::Handled();
					}))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
				[
					FShuttlegentPanelWidgets::MakeTextButton(LOCTEXT("AllowOnce", "允许本次"), true, FOnClicked::CreateLambda([this]()
					{
						Answer(true);
						return FReply::Handled();
					}))
				]
			]
		]
	];
}

void SShuttlegentElevationCard::ShowRequest(const TSharedPtr<FShuttlegentChatEntry>& Permission)
{
	EntryId = Permission.IsValid() ? Permission->Id : 0;
	if (Body.IsValid()) Body->SetContent(BuildBody(Permission));
}

void SShuttlegentElevationCard::FocusTitle()
{
	if (Title.IsValid()) FSlateApplication::Get().SetKeyboardFocus(Title, EFocusCause::SetDirectly);
}

FReply SShuttlegentElevationCard::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() != EKeys::Escape) return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);

	Answer(false);
	return FReply::Handled();
}

TSharedRef<SWidget> SShuttlegentElevationCard::BuildBody(const TSharedPtr<FShuttlegentChatEntry>& Permission)
{
	TSharedRef<SVerticalBox> Lines = SNew(SVerticalBox);
	if (!Permission.IsValid()) return Lines;

	const TSharedPtr<FShuttlegentChatEntry> ToolCall = Model->FindToolCallOf(Permission);
	const FString ToolName = ToolCall.IsValid() ? ToolCall->Title : Permission->Title;
	const FString Input = ToolCall.IsValid() ? ToolCall->Input : FString();
	const FString Target = FShuttlegentToolPresentation::GetSummary(Input);
	const FString Content = FShuttlegentToolPresentation::GetContentPreview(Input);

	Lines->AddSlot()
		.AutoHeight()
		[
			SShuttlegentElevationCardPrivate::MakeLine(FText::Format(LOCTEXT("Operation", "操作：{0}"), FShuttlegentToolPresentation::GetDisplayName(ToolName)), FShuttlegentStyle::CaptionFont(), ShuttlegentEditorColor::Muted)
		];

	if (!Target.IsEmpty())
	{
		Lines->AddSlot()
			.AutoHeight()
			.Padding(0.0f, ShuttlegentEditorMetric::Space1, 0.0f, 0.0f)
			[
				SShuttlegentElevationCardPrivate::MakeLine(FText::FromString(Target), FShuttlegentStyle::MonoCaptionFont(), ShuttlegentEditorColor::Text)
			];
	}

	if (!Content.IsEmpty())
	{
		Lines->AddSlot()
			.AutoHeight()
			.Padding(0.0f, ShuttlegentEditorMetric::Space1, 0.0f, 0.0f)
			[
				SShuttlegentElevationCardPrivate::MakeLine(FText::Format(LOCTEXT("Content", "内容：{0}"), FText::FromString(Content)), FShuttlegentStyle::CaptionFont(), ShuttlegentEditorColor::Muted)
			];
	}

	if (!Permission->Detail.IsEmpty())
	{
		Lines->AddSlot()
			.AutoHeight()
			.Padding(0.0f, ShuttlegentEditorMetric::Space1, 0.0f, 0.0f)
			[
				SShuttlegentElevationCardPrivate::MakeLine(FText::Format(LOCTEXT("Reason", "说明：{0}"), FText::FromString(Permission->Detail)), FShuttlegentStyle::CaptionFont(), ShuttlegentEditorColor::Muted)
			];
	}

	Lines->AddSlot()
		.AutoHeight()
		.Padding(0.0f, ShuttlegentEditorMetric::Space2, 0.0f, 0.0f)
		[
			SShuttlegentElevationCardPrivate::MakeLine(LOCTEXT("FinePrint", "本次授权只覆盖这一个请求，不会开放后续请求。拒绝后保留任务记录。"), FShuttlegentStyle::CaptionFont(), ShuttlegentEditorColor::Muted)
		];
	return Lines;
}

void SShuttlegentElevationCard::Answer(bool bAllow)
{
	if (EntryId == 0) return;

	const int32 Answered = EntryId;
	EntryId = 0;
	Model->AnswerPermission(Answered, bAllow);
}

#undef LOCTEXT_NAMESPACE
