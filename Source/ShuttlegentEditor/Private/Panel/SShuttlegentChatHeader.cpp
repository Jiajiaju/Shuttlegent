#include "Panel/SShuttlegentChatHeader.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "Chat/ShuttlegentChatModel.h"
#include "Mark/EShuttlegentMarkMotion.h"
#include "Mark/EShuttlegentMarkTier.h"
#include "Mark/SShuttlegentMark.h"
#include "Panel/FShuttlegentPanelWidgets.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentStyle.h"

#define LOCTEXT_NAMESPACE "SShuttlegentChatHeader"

namespace SShuttlegentChatHeaderPrivate
{
	// Icons of the pane toggles
	inline const TCHAR* IconSidebar = TEXT("chat");
	inline const TCHAR* IconInspector = TEXT("tools");
}

void SShuttlegentChatHeader::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());
	OnToggleSidebar = InArgs._OnToggleSidebar;
	OnToggleInspector = InArgs._OnToggleInspector;

	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;

	ChildSlot
	[
		SNew(SBox)
		.HeightOverride(ShuttlegentEditorMetric::HeaderHeight)
		.Padding(FMargin(ShuttlegentEditorMetric::Space3, 0.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(SidebarToggle, SButton)
				.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
				.ContentPadding(ShuttlegentEditorMetric::Space1)
				.ToolTipText(LOCTEXT("ToggleSidebar", "会话侧栏 (Ctrl+B)"))
				.OnClicked_Lambda([this]()
				{
					OnToggleSidebar.ExecuteIfBound();
					return FReply::Handled();
				})
				[
					SNew(SBox)
					.WidthOverride(ShuttlegentEditorMetric::IconSizeSmall)
					.HeightOverride(ShuttlegentEditorMetric::IconSizeSmall)
					[
						SNew(SImage)
						.Image(FShuttlegentStyle::Icon(SShuttlegentChatHeaderPrivate::IconSidebar))
						.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
					]
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(Mark, SShuttlegentMark)
				.Tier(EShuttlegentMarkTier::Small)
				.ColorAndOpacity(this, &SShuttlegentChatHeader::GetMarkColor)
			]

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorMetric::Space2, 0.0f)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(this, &SShuttlegentChatHeader::GetTitleText)
					.Font(FShuttlegentStyle::EmphasisFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Font(FShuttlegentStyle::CaptionFont())
					.Text_Lambda([WeakModel]()
					{
						const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
						return Pinned.IsValid() ? FShuttlegentPanelWidgets::GetStatusText(Pinned->GetStatus()) : FText::GetEmpty();
					})
					.ColorAndOpacity_Lambda([WeakModel]()
					{
						const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
						return Pinned.IsValid() ? FShuttlegentPanelWidgets::GetStatusColor(Pinned->GetStatus()) : FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted);
					})
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(RenameButton, SComboButton)
				.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
				.HasDownArrow(false)
				.ToolTipText(LOCTEXT("Rename", "重命名会话"))
				.IsEnabled_Lambda([WeakModel]()
				{
					const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
					return Pinned.IsValid() && !Pinned->GetCurrentSessionId().IsEmpty();
				})
				.OnGetMenuContent(this, &SShuttlegentChatHeader::BuildRenameMenu)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RenameLabel", "重命名"))
					.Font(FShuttlegentStyle::CaptionFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(ArchiveButton, SComboButton)
				.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
				.HasDownArrow(false)
				.ToolTipText_Lambda([WeakModel]()
				{
					const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
					const bool bBusy = Pinned.IsValid() && !Pinned->GetCurrentSessionId().IsEmpty() && !Pinned->CanArchive(Pinned->GetCurrentSessionId());
					return bBusy ? LOCTEXT("ArchiveBusy", "运行中或等待授权的会话不能归档") : LOCTEXT("Archive", "归档会话");
				})
				.IsEnabled_Lambda([WeakModel]()
				{
					const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
					return Pinned.IsValid() && Pinned->CanArchive(Pinned->GetCurrentSessionId());
				})
				.OnGetMenuContent(this, &SShuttlegentChatHeader::BuildArchiveMenu)
				.ButtonContent()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ArchiveLabel", "归档"))
					.Font(FShuttlegentStyle::CaptionFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorMetric::Space1, 0.0f, 0.0f, 0.0f)
			[
				SAssignNew(InspectorToggle, SButton)
				.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
				.ContentPadding(ShuttlegentEditorMetric::Space1)
				.ToolTipText(LOCTEXT("ToggleInspector", "检视窗格 (Ctrl+I)"))
				.OnClicked_Lambda([this]()
				{
					OnToggleInspector.ExecuteIfBound();
					return FReply::Handled();
				})
				[
					SNew(SBox)
					.WidthOverride(ShuttlegentEditorMetric::IconSizeSmall)
					.HeightOverride(ShuttlegentEditorMetric::IconSizeSmall)
					[
						SNew(SImage)
						.Image(FShuttlegentStyle::Icon(SShuttlegentChatHeaderPrivate::IconInspector))
						.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
					]
				]
			]
		]
	];

	Model->OnStatusChanged.AddSP(this, &SShuttlegentChatHeader::HandleStatusChanged);
	Model->OnTurnFinished.AddSP(this, &SShuttlegentChatHeader::HandleTurnFinished);

	bWasPrompting = IsPrompting();
	if (bWasPrompting) Mark->Play(EShuttlegentMarkMotion::Working);
}

SShuttlegentChatHeader::~SShuttlegentChatHeader()
{
	if (!Model.IsValid()) return;

	Model->OnStatusChanged.RemoveAll(this);
	Model->OnTurnFinished.RemoveAll(this);
}

TSharedPtr<SWidget> SShuttlegentChatHeader::GetSidebarToggle() const
{
	return SidebarToggle;
}

TSharedPtr<SWidget> SShuttlegentChatHeader::GetInspectorToggle() const
{
	return InspectorToggle;
}

TSharedRef<SWidget> SShuttlegentChatHeader::BuildRenameMenu()
{
	bRenameRejected = false;

	const TSharedRef<SWidget> Menu = SNew(SBorder)
		.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Raised))
		.Padding(ShuttlegentEditorMetric::Space3)
		[
			SNew(SBox)
			.WidthOverride(ShuttlegentEditorMetric::PopupWidth)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RenameTitle", "重命名会话"))
					.Font(FShuttlegentStyle::EmphasisFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, ShuttlegentEditorMetric::Space2)
				[
					SAssignNew(RenameBox, SEditableTextBox)
					.Style(&FShuttlegentStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(ShuttlegentEditorStyleKey::SearchBox))
					.Text(FText::FromString(Model->GetCurrentTitle()))
					.SelectAllTextWhenFocused(true)
					.OnVerifyTextChanged_Lambda([](const FText& Text, FText& OutError)
					{
						if (Text.ToString().Len() <= ShuttlegentSessionConstant::RenameMaxChars) return true;

						OutError = FText::Format(LOCTEXT("RenameTooLong", "最多 {0} 个字符"), FText::AsNumber(ShuttlegentSessionConstant::RenameMaxChars));
						return false;
					})
					.OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type CommitType)
					{
						if (CommitType == ETextCommit::OnEnter) CommitRename();
					})
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(FText::Format(LOCTEXT("RenameRejected", "名称不能为空，最多 {0} 个字符。"), FText::AsNumber(ShuttlegentSessionConstant::RenameMaxChars)))
					.Font(FShuttlegentStyle::CaptionFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Warning))
					.Visibility_Lambda([this]() { return bRenameRejected ? EVisibility::Visible : EVisibility::Collapsed; })
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(0.0f, ShuttlegentEditorMetric::Space2, 0.0f, 0.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						FShuttlegentPanelWidgets::MakeTextButton(LOCTEXT("Cancel", "取消"), false, FOnClicked::CreateLambda([this]()
						{
							if (RenameButton.IsValid()) RenameButton->SetIsOpen(false);
							return FReply::Handled();
						}))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
					[
						FShuttlegentPanelWidgets::MakeTextButton(LOCTEXT("Save", "保存"), true, FOnClicked::CreateLambda([this]()
						{
							CommitRename();
							return FReply::Handled();
						}))
					]
				]
			]
		];

	RenameButton->SetMenuContentWidgetToFocus(RenameBox);
	return Menu;
}

TSharedRef<SWidget> SShuttlegentChatHeader::BuildArchiveMenu()
{
	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;

	return SNew(SBorder)
		.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Raised))
		.Padding(ShuttlegentEditorMetric::Space3)
		[
			SNew(SBox)
			.WidthOverride(ShuttlegentEditorMetric::PopupWidth)
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ArchiveTitle", "归档当前会话"))
					.Font(FShuttlegentStyle::EmphasisFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, ShuttlegentEditorMetric::Space2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ArchiveBody", "仅从本地列表隐藏，记录保留，可从“已归档”恢复。此操作不会删除运行时上下文。"))
					.Font(FShuttlegentStyle::CaptionFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
					.AutoWrapText(true)
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						FShuttlegentPanelWidgets::MakeTextButton(LOCTEXT("Cancel", "取消"), false, FOnClicked::CreateLambda([this]()
						{
							if (ArchiveButton.IsValid()) ArchiveButton->SetIsOpen(false);
							return FReply::Handled();
						}))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
					[
						FShuttlegentPanelWidgets::MakeTextButton(LOCTEXT("ArchiveConfirm", "归档"), true, FOnClicked::CreateLambda([this, WeakModel]()
						{
							if (ArchiveButton.IsValid()) ArchiveButton->SetIsOpen(false);

							const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
							if (Pinned.IsValid()) Pinned->ArchiveSession(Pinned->GetCurrentSessionId());
							return FReply::Handled();
						}))
					]
				]
			]
		];
}

void SShuttlegentChatHeader::CommitRename()
{
	if (!RenameBox.IsValid()) return;

	bRenameRejected = !Model->RenameSession(Model->GetCurrentSessionId(), RenameBox->GetText().ToString());
	if (!bRenameRejected && RenameButton.IsValid()) RenameButton->SetIsOpen(false);
}

FText SShuttlegentChatHeader::GetTitleText() const
{
	const FString Title = Model->GetCurrentTitle();
	return Title.IsEmpty() ? LOCTEXT("NewSessionTitle", "新会话") : FText::FromString(Title);
}

FSlateColor SShuttlegentChatHeader::GetMarkColor() const
{
	const EShuttlegentAgentState State = Model->GetState();
	const bool bConnected = !Model->IsRuntimeLost() && (State == EShuttlegentAgentState::Ready || State == EShuttlegentAgentState::Prompting);
	return FSlateColor(bConnected ? FLinearColor::White : FLinearColor::White.CopyWithNewOpacity(ShuttlegentEditorMarkAnim::DisconnectedOpacity));
}

bool SShuttlegentChatHeader::IsPrompting() const
{
	return Model->GetState() == EShuttlegentAgentState::Prompting;
}

void SShuttlegentChatHeader::HandleStatusChanged()
{
	if (!Mark.IsValid()) return;

	const bool bPrompting = IsPrompting();
	if (bPrompting && !bWasPrompting) Mark->Play(EShuttlegentMarkMotion::Working);

	// Left the turn without a turn-finished event, e.g. the runtime was lost
	if (!bPrompting && bWasPrompting && Mark->GetMotion() == EShuttlegentMarkMotion::Working) Mark->ShowRest();
	bWasPrompting = bPrompting;
}

void SShuttlegentChatHeader::HandleTurnFinished(bool bCompleted)
{
	if (!Mark.IsValid()) return;

	if (bCompleted) Mark->Play(EShuttlegentMarkMotion::Done);
	else Mark->ShowRest();
}

#undef LOCTEXT_NAMESPACE
