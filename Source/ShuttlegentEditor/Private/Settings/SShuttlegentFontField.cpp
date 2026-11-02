#include "Settings/SShuttlegentFontField.h"

#include "DetailLayoutBuilder.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#include "Settings/FShuttlegentSystemFonts.h"
#include "ShuttlegentEditorConstants.h"

#define LOCTEXT_NAMESPACE "SShuttlegentFontField"

namespace SShuttlegentFontFieldPrivate
{
	TSharedPtr<FShuttlegentFontItem> MakeItem(const FString& Family, const FText& Label, bool bHeader, bool bDefault)
	{
		TSharedPtr<FShuttlegentFontItem> Item = MakeShared<FShuttlegentFontItem>();
		Item->Family = Family;
		Item->Label = Label;
		Item->bHeader = bHeader;
		Item->bDefault = bDefault;
		return Item;
	}

	/** 框里逗号分隔的各个字体名，去掉首尾空白与引号，丢掉空项。 */
	TArray<FString> SplitFamilies(const FString& Text)
	{
		TArray<FString> Parts;
		Text.ParseIntoArray(Parts, ShuttlegentEditorSettingsUi::FontSeparator, true);
		TArray<FString> Families;
		for (FString& Part : Parts)
		{
			Part.TrimStartAndEndInline();
			Part.TrimQuotesInline();
			if (!Part.IsEmpty()) Families.Add(Part);
		}
		return Families;
	}

	bool IsGeneric(const FString& Family)
	{
		for (const TCHAR* Generic : ShuttlegentEditorSettingsUi::GenericFontFamilies)
		{
			if (Family.Equals(Generic, ESearchCase::IgnoreCase)) return true;
		}
		return false;
	}
}

void SShuttlegentFontField::Construct(const FArguments& InArgs)
{
	Value = InArgs._Value;
	Recommended = InArgs._Recommended;
	bMonospaceOnly = InArgs._bMonospaceOnly;
	DefaultLabel = InArgs._DefaultLabel;
	OnCommitted = InArgs._OnCommitted;

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(MenuAnchor, SMenuAnchor)
			.Placement(MenuPlacement_ComboBox)
			.UseApplicationMenuStack(true)
			.MenuContent
			(
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush(ShuttlegentEditorSettingsUi::FontListBackgroundBrush))
				[
					SNew(SBox)
					.MaxDesiredHeight(ShuttlegentEditorSettingsUi::FontListMaxHeight)
					[
						SAssignNew(ListView, SListView<TSharedPtr<FShuttlegentFontItem>>)
						.ListItemsSource(&Items)
						.SelectionMode(ESelectionMode::Single)
						.OnGenerateRow(this, &SShuttlegentFontField::GenerateRow)
						.OnMouseButtonClick(this, &SShuttlegentFontField::HandleItemClicked)
					]
				]
			)
			[
				SNew(SOverlay)
				+ SOverlay::Slot()
				[
					SAssignNew(TextBox, SEditableTextBox)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text_Lambda([this]() { return FText::FromString(Value.Get()); })
					.HintText(DefaultLabel)
					.OnTextChanged(this, &SShuttlegentFontField::HandleTextChanged)
					.OnTextCommitted(this, &SShuttlegentFontField::HandleTextCommitted)
					.OnKeyDownHandler(this, &SShuttlegentFontField::HandleKeyDown)
				]
				+ SOverlay::Slot()
				.HAlign(HAlign_Right)
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, ShuttlegentEditorSettingsUi::FontFieldChevronInset, 0.0f)
				[
					SNew(SImage)
					.Visibility(EVisibility::HitTestInvisible)
					.DesiredSizeOverride(FVector2D(ShuttlegentEditorSettingsUi::FontFieldChevronSize))
					.Image(FAppStyle::GetBrush(ShuttlegentEditorSettingsUi::FontChevronBrush))
					.ColorAndOpacity(FSlateColor::UseSubduedForeground())
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, ShuttlegentEditorSettingsUi::FontWarningGap, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			.Visibility_Lambda([this]() { return GetWarning().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, ShuttlegentEditorSettingsUi::FontWarningGap, 0.0f)
			[
				SNew(SImage)
				.DesiredSizeOverride(FVector2D(ShuttlegentEditorSettingsUi::FontFieldChevronSize))
				.Image(FAppStyle::GetBrush(ShuttlegentEditorSettingsUi::FontWarningBrush))
				.ColorAndOpacity(FAppStyle::GetSlateColor(ShuttlegentEditorSettingsUi::FontWarningColor))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.ColorAndOpacity(FAppStyle::GetSlateColor(ShuttlegentEditorSettingsUi::FontWarningColor))
				.AutoWrapText(true)
				.Text_Lambda([this]() { return GetWarning(); })
			]
		]
	];
}

FReply SShuttlegentFontField::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// 点进框里就列出字体；事件照常交给输入框，光标落在点击处
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && !MenuAnchor->IsOpen()) OpenList(FString());
	return FReply::Unhandled();
}

void SShuttlegentFontField::RebuildItems(const FString& Filter)
{
	Items.Reset();
	const TArray<FShuttlegentSystemFont>& Fonts = FShuttlegentSystemFonts::Get();
	const auto Listed = [this](const FShuttlegentSystemFont& Font) { return !bMonospaceOnly || Font.bMonospace; };

	if (!Filter.IsEmpty())
	{
		for (const FShuttlegentSystemFont& Font : Fonts)
		{
			if (Listed(Font) && Font.Family.Contains(Filter, ESearchCase::IgnoreCase)) Items.Add(SShuttlegentFontFieldPrivate::MakeItem(Font.Family, FText::FromString(Font.Family), false, false));
		}
		return;
	}

	Items.Add(SShuttlegentFontFieldPrivate::MakeItem(FString(), DefaultLabel, false, true));

	TArray<TSharedPtr<FShuttlegentFontItem>> RecommendedItems;
	for (const FString& Family : Recommended)
	{
		if (!Family.IsEmpty() && FShuttlegentSystemFonts::IsInstalled(Family)) RecommendedItems.Add(SShuttlegentFontFieldPrivate::MakeItem(Family, FText::FromString(Family), false, false));
	}
	if (!RecommendedItems.IsEmpty())
	{
		Items.Add(SShuttlegentFontFieldPrivate::MakeItem(FString(), LOCTEXT("RecommendedHeader", "推荐（本机已安装）"), true, false));
		Items.Append(RecommendedItems);
	}

	TArray<TSharedPtr<FShuttlegentFontItem>> AllItems;
	for (const FShuttlegentSystemFont& Font : Fonts)
	{
		if (Listed(Font)) AllItems.Add(SShuttlegentFontFieldPrivate::MakeItem(Font.Family, FText::FromString(Font.Family), false, false));
	}
	if (AllItems.IsEmpty()) return;

	const FText AllHeader = bMonospaceOnly ? LOCTEXT("MonoHeader", "本机等宽字体（共 {0} 个）") : LOCTEXT("AllHeader", "全部字体（共 {0} 个）");
	Items.Add(SShuttlegentFontFieldPrivate::MakeItem(FString(), FText::Format(AllHeader, FText::AsNumber(AllItems.Num())), true, false));
	Items.Append(AllItems);
}

void SShuttlegentFontField::OpenList(const FString& Filter)
{
	RebuildItems(Filter);
	ListView->RequestListRefresh();
	ListView->ClearSelection();
	if (Items.IsEmpty())
	{
		CloseList();
		return;
	}

	// 键盘焦点留在输入框，继续输入时列表跟着筛选
	if (!MenuAnchor->IsOpen()) MenuAnchor->SetIsOpen(true, false);
}

void SShuttlegentFontField::CloseList()
{
	if (MenuAnchor.IsValid() && MenuAnchor->IsOpen()) MenuAnchor->SetIsOpen(false);
}

TSharedRef<ITableRow> SShuttlegentFontField::GenerateRow(TSharedPtr<FShuttlegentFontItem> Item, const TSharedRef<STableViewBase>& OwnerTable) const
{
	const bool bHeader = Item->bHeader;
	return SNew(STableRow<TSharedPtr<FShuttlegentFontItem>>, OwnerTable)
		.ShowSelection(!bHeader)
		.Padding(FMargin(ShuttlegentEditorSettingsUi::FontRowPaddingX, ShuttlegentEditorSettingsUi::FontRowPaddingY))
		[
			SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(bHeader ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground())
			.Text(Item->Label)
		];
}

void SShuttlegentFontField::HandleItemClicked(TSharedPtr<FShuttlegentFontItem> Item)
{
	if (Item.IsValid() && !Item->bHeader) ApplyItem(Item);
}

void SShuttlegentFontField::ApplyItem(const TSharedPtr<FShuttlegentFontItem>& Item)
{
	FString NewValue;
	if (!Item->bDefault)
	{
		TArray<FString> Families = SShuttlegentFontFieldPrivate::SplitFamilies(TextBox->GetText().ToString());
		if (Families.IsEmpty()) Families.Add(Item->Family);
		else Families.Last() = Item->Family;
		NewValue = FString::Join(Families, ShuttlegentEditorSettingsUi::FontJoiner);
	}

	// 输入框的文字绑定在设置值上，提交后随之更新
	bApplyingItem = true;
	OnCommitted.ExecuteIfBound(NewValue);
	CloseList();
	bApplyingItem = false;
}

void SShuttlegentFontField::MoveSelection(int32 Direction)
{
	if (Items.IsEmpty()) return;

	const TArray<TSharedPtr<FShuttlegentFontItem>> Selected = ListView->GetSelectedItems();
	int32 Index = Selected.IsEmpty() ? (Direction > 0 ? -1 : Items.Num()) : Items.IndexOfByKey(Selected[0]);
	for (int32 Step = 0; Step < Items.Num(); ++Step)
	{
		Index = (Index + Direction + Items.Num()) % Items.Num();
		if (Items[Index]->bHeader) continue;

		ListView->SetSelection(Items[Index], ESelectInfo::OnKeyPress);
		ListView->RequestScrollIntoView(Items[Index]);
		return;
	}
}

void SShuttlegentFontField::HandleTextChanged(const FText& Text)
{
	// 设置值变化带来的文字更新不算输入
	if (bApplyingItem || !TextBox->HasKeyboardFocus() || Text.ToString() == Value.Get()) return;

	OpenList(LastSegment(Text.ToString()));
}

void SShuttlegentFontField::HandleTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	if (bApplyingItem) return;

	// 列表开着时失焦多半是去点列表里的一项：先提交手输的内容但不收起列表，点中的一项随后覆盖；点到别处时菜单自行关闭
	const bool bKeepList = CommitType == ETextCommit::OnUserMovedFocus && MenuAnchor->IsOpen();
	if (!bKeepList) CloseList();

	const FString NewValue = FString::Join(SShuttlegentFontFieldPrivate::SplitFamilies(Text.ToString()), ShuttlegentEditorSettingsUi::FontJoiner);
	if (NewValue != Value.Get()) OnCommitted.ExecuteIfBound(NewValue);
}

FReply SShuttlegentFontField::HandleKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent)
{
	const FKey Key = KeyEvent.GetKey();
	if (Key == EKeys::Down || Key == EKeys::Up)
	{
		if (!MenuAnchor->IsOpen()) OpenList(LastSegment(TextBox->GetText().ToString()));
		MoveSelection(Key == EKeys::Down ? 1 : -1);
		return FReply::Handled();
	}
	if (!MenuAnchor->IsOpen()) return FReply::Unhandled();

	if (Key == EKeys::Escape)
	{
		CloseList();
		return FReply::Handled();
	}

	const TArray<TSharedPtr<FShuttlegentFontItem>> Selected = ListView->GetSelectedItems();
	if (Key == EKeys::Enter && !Selected.IsEmpty())
	{
		ApplyItem(Selected[0]);
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FText SShuttlegentFontField::GetWarning() const
{
	if (FShuttlegentSystemFonts::Get().IsEmpty()) return FText::GetEmpty();

	const TArray<FString> Families = SShuttlegentFontFieldPrivate::SplitFamilies(Value.Get());
	if (Families.IsEmpty()) return FText::GetEmpty();

	for (const FString& Family : Families)
	{
		if (SShuttlegentFontFieldPrivate::IsGeneric(Family) || FShuttlegentSystemFonts::IsInstalled(Family)) return FText::GetEmpty();
	}
	return FText::Format(LOCTEXT("NotInstalled", "本机未安装“{0}”，面板会退回默认字体"), FText::FromString(FString::Join(Families, ShuttlegentEditorSettingsUi::FontJoiner)));
}

FString SShuttlegentFontField::LastSegment(const FString& Text)
{
	FString Segment;
	if (!Text.Split(ShuttlegentEditorSettingsUi::FontSeparator, nullptr, &Segment, ESearchCase::CaseSensitive, ESearchDir::FromEnd)) Segment = Text;
	return Segment.TrimStartAndEnd();
}

#undef LOCTEXT_NAMESPACE
