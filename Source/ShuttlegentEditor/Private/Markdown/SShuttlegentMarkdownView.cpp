#include "Markdown/SShuttlegentMarkdownView.h"

#include "Framework/Text/RichTextLayoutMarshaller.h"
#include "Framework/Text/SlateHyperlinkRun.h"
#include "Framework/Text/TextDecorators.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"

#include "Chat/FShuttlegentChatEntry.h"
#include "Markdown/FShuttlegentMarkdown.h"
#include "Markdown/FShuttlegentMarkdownBlock.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentStyle.h"

#define LOCTEXT_NAMESPACE "SShuttlegentMarkdownView"

namespace SShuttlegentMarkdownViewPrivate
{
	// Link metadata key and the only schemes a click opens
	inline const TCHAR* HrefKey = TEXT("href");
	inline const TCHAR* OpenableSchemes[] = { TEXT("https://"), TEXT("http://") };

	// Headings from this level down share the smallest heading style
	inline constexpr int32 SmallestHeadingLevel = 3;

	void OpenLink(const FSlateHyperlinkRun::FMetadata& Metadata)
	{
		const FString* Url = Metadata.Find(HrefKey);
		if (Url == nullptr) return;

		for (const TCHAR* Scheme : OpenableSchemes)
		{
			if (!Url->StartsWith(Scheme, ESearchCase::IgnoreCase)) continue;

			FPlatformProcess::LaunchURL(**Url, nullptr, nullptr);
			return;
		}
	}

	const TCHAR* HeadingStyle(int32 Level)
	{
		if (Level <= 1) return ShuttlegentEditorStyleKey::MarkdownHeading1;
		if (Level < SmallestHeadingLevel) return ShuttlegentEditorStyleKey::MarkdownHeading2;
		return ShuttlegentEditorStyleKey::MarkdownHeading3;
	}

	FSlateColor Tint(const FColor& Token)
	{
		return FSlateColor(FShuttlegentStyle::Color(Token));
	}
}

void SShuttlegentMarkdownView::Construct(const FArguments& InArgs)
{
	Entry = InArgs._Entry;

	ChildSlot
	[
		SAssignNew(BlockList, SVerticalBox)
	];

	Rebuild();
}

void SShuttlegentMarkdownView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	const TSharedPtr<FShuttlegentChatEntry> Pinned = Entry.Pin();
	if (Pinned.IsValid() && Pinned->Text.Len() != ShownLength) Rebuild();
}

void SShuttlegentMarkdownView::Rebuild()
{
	if (!BlockList.IsValid()) return;

	BlockList->ClearChildren();

	const TSharedPtr<FShuttlegentChatEntry> Pinned = Entry.Pin();
	if (!Pinned.IsValid()) return;

	ShownLength = Pinned->Text.Len();
	const TArray<FShuttlegentMarkdownBlock> Blocks = FShuttlegentMarkdown::Parse(Pinned->Text);
	for (int32 Index = 0; Index < Blocks.Num(); ++Index)
	{
		BlockList->AddSlot()
			.AutoHeight()
			.Padding(0.0f, Index > 0 ? ShuttlegentEditorMetric::Space2 : 0.0f, 0.0f, 0.0f)
			[
				MakeBlock(Blocks[Index])
			];
	}
}

TSharedRef<SWidget> SShuttlegentMarkdownView::MakeBlock(const FShuttlegentMarkdownBlock& Block) const
{
	switch (Block.Kind)
	{
	case EShuttlegentMarkdownBlockKind::Heading:
		{
			return MakeRichText(Block.Markup, SShuttlegentMarkdownViewPrivate::HeadingStyle(Block.Level));
		}
	case EShuttlegentMarkdownBlockKind::ListItem:
		{
			return SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(Block.Level * ShuttlegentEditorMetric::Space4, 0.0f, ShuttlegentEditorMetric::Space2, 0.0f)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Block.Marker))
					.Font(FShuttlegentStyle::BodyFont())
					.ColorAndOpacity(SShuttlegentMarkdownViewPrivate::Tint(ShuttlegentEditorColor::Muted))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					MakeRichText(Block.Markup, ShuttlegentEditorStyleKey::MarkdownBody)
				];
		}
	case EShuttlegentMarkdownBlockKind::Quote:
		{
			return SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(ShuttlegentEditorMetric::Space1)
					[
						SNew(SImage)
						.Image(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Divider))
					]
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(ShuttlegentEditorMetric::Space3, 0.0f, 0.0f, 0.0f)
				[
					MakeRichText(Block.Markup, ShuttlegentEditorStyleKey::MarkdownQuote)
				];
		}
	case EShuttlegentMarkdownBlockKind::Code:
		{
			return MakeVerbatim(Block.Verbatim, Block.Language);
		}
	case EShuttlegentMarkdownBlockKind::Table:
		{
			return MakeVerbatim(Block.Verbatim, LOCTEXT("TableLabel", "表格").ToString());
		}
	case EShuttlegentMarkdownBlockKind::Rule:
		{
			return SNew(SBox)
				.HeightOverride(ShuttlegentEditorMetric::BorderWidth)
				[
					SNew(SImage)
					.Image(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Divider))
				];
		}
	case EShuttlegentMarkdownBlockKind::Paragraph:
	default:
		{
			return MakeRichText(Block.Markup, ShuttlegentEditorStyleKey::MarkdownBody);
		}
	}
}

TSharedRef<SWidget> SShuttlegentMarkdownView::MakeRichText(const FString& Markup, const TCHAR* DefaultStyle) const
{
	TArray<TSharedRef<ITextDecorator>> Decorators;
	Decorators.Add(FHyperlinkDecorator::Create(ShuttlegentEditorStyleKey::MarkdownLinkId, FSlateHyperlinkRun::FOnClick::CreateStatic(&SShuttlegentMarkdownViewPrivate::OpenLink)));

	return SNew(SMultiLineEditableText)
		.Text(FText::FromString(Markup))
		.TextStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FTextBlockStyle>(DefaultStyle))
		.Marshaller(FRichTextLayoutMarshaller::Create(Decorators, &FShuttlegentStyle::Get()))
		.IsReadOnly(true)
		.AutoWrapText(true);
}

TSharedRef<SWidget> SShuttlegentMarkdownView::MakeVerbatim(const FString& Text, const FString& Label) const
{
	return SNew(SBorder)
		.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Surface))
		.Padding(ShuttlegentEditorMetric::Space3)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(Label))
					.Font(FShuttlegentStyle::CaptionFont())
					.ColorAndOpacity(SShuttlegentMarkdownViewPrivate::Tint(ShuttlegentEditorColor::Muted))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::IconButton))
					.ContentPadding(ShuttlegentEditorMetric::Space1)
					.ToolTipText(LOCTEXT("CopyCode", "复制"))
					.OnClicked_Lambda([Text]()
					{
						FPlatformApplicationMisc::ClipboardCopy(*Text);
						return FReply::Handled();
					})
					[
						SNew(SBox)
						.WidthOverride(ShuttlegentEditorMetric::IconSizeSmall)
						.HeightOverride(ShuttlegentEditorMetric::IconSizeSmall)
						[
							SNew(SImage)
							.Image(FShuttlegentStyle::Icon(ShuttlegentEditorResource::IconCopy))
							.ColorAndOpacity(SShuttlegentMarkdownViewPrivate::Tint(ShuttlegentEditorColor::Muted))
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, ShuttlegentEditorMetric::Space1, 0.0f, 0.0f)
			[
				SNew(SMultiLineEditableText)
				.Text(FText::FromString(Text))
				.TextStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FTextBlockStyle>(ShuttlegentEditorStyleKey::MarkdownCodeBlock))
				.IsReadOnly(true)
				.AutoWrapText(true)
			]
		];
}

#undef LOCTEXT_NAMESPACE
