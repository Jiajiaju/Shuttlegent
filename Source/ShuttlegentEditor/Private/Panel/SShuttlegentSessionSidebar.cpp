#include "Panel/SShuttlegentSessionSidebar.h"

#include "Framework/Application/SlateApplication.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Styling/StyleDefaults.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "Chat/ShuttlegentChatModel.h"
#include "Panel/FShuttlegentPanelWidgets.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentSessionRecord.h"
#include "ShuttlegentSettings.h"
#include "ShuttlegentStyle.h"

#define LOCTEXT_NAMESPACE "SShuttlegentSessionSidebar"

namespace SShuttlegentSessionSidebarPrivate
{
	// Icons of the footer and the brand row
	inline const TCHAR* IconSettings = TEXT("settings");
	inline const TCHAR* IconClose = TEXT("close");

	// Section of the session list a record falls into by its local update day
	enum class EDaySection : uint8
	{
		Today,

		Yesterday,

		Earlier
	};

	EDaySection GetSection(const FDateTime& UtcTime)
	{
		const FDateTime Local = UtcTime + (FDateTime::Now() - FDateTime::UtcNow());
		const FDateTime Today = FDateTime::Now().GetDate();
		if (Local.GetDate() == Today) return EDaySection::Today;
		if (Local.GetDate() == Today - FTimespan::FromDays(1.0)) return EDaySection::Yesterday;
		return EDaySection::Earlier;
	}

	FText GetSectionLabel(EDaySection Section)
	{
		switch (Section)
		{
		case EDaySection::Today:
			{
				return LOCTEXT("SectionToday", "今天");
			}
		case EDaySection::Yesterday:
			{
				return LOCTEXT("SectionYesterday", "昨天");
			}
		case EDaySection::Earlier:
		default:
			{
				return LOCTEXT("SectionEarlier", "更早");
			}
		}
	}

	FText GetRecordTitle(const FShuttlegentSessionRecord& Record)
	{
		return Record.Title.IsEmpty() ? LOCTEXT("Untitled", "新会话") : FText::FromString(Record.Title);
	}

	TSharedRef<SWidget> MakeSectionLabel(const FText& Label)
	{
		return SNew(SBox)
			.Padding(FMargin(ShuttlegentEditorMetric::Space2, ShuttlegentEditorMetric::Space4, ShuttlegentEditorMetric::Space2, ShuttlegentEditorMetric::Space2))
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FShuttlegentStyle::CaptionFont())
				.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
			];
	}
}

void SShuttlegentSessionSidebar::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());
	OnCollapse = InArgs._OnCollapse;
	OnSessionChosen = InArgs._OnSessionChosen;

	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;

	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(ShuttlegentEditorMetric::SidebarWidth)
		[
			SNew(SBorder)
			.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Pane))
			.Padding(FMargin(ShuttlegentEditorMetric::Space3, ShuttlegentEditorMetric::Space3))
			[
				SNew(SVerticalBox)

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
					.FillWidth(1.0f)
					.VAlign(VAlign_Center)
					.Padding(ShuttlegentEditorMetric::Space2, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Brand", "Shuttlegent"))
						.Font(FShuttlegentStyle::EmphasisFont())
						.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						FShuttlegentPanelWidgets::MakeIconButton(SShuttlegentSessionSidebarPrivate::IconClose, LOCTEXT("Collapse", "收起会话侧栏 (Ctrl+B)"), FOnClicked::CreateLambda([this]()
						{
							OnCollapse.ExecuteIfBound();
							return FReply::Handled();
						}))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, ShuttlegentEditorMetric::Space3, 0.0f, ShuttlegentEditorMetric::Space3)
				[
					SNew(SButton)
					.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::SecondaryButton))
					.ToolTipText(LOCTEXT("NewSessionTip", "开始一个新会话 (Ctrl+N)"))
					.OnClicked_Lambda([this, WeakModel]()
					{
						const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
						if (Pinned.IsValid()) Pinned->NewConversation();
						OnSessionChosen.ExecuteIfBound();
						return FReply::Handled();
					})
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("NewSession", "＋ 新会话"))
							.Font(FShuttlegentStyle::BodyFont())
							.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock)
							.Text(LOCTEXT("NewSessionKey", "Ctrl N"))
							.Font(FShuttlegentStyle::CaptionFont())
							.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(SearchBox, SEditableTextBox)
					.Style(&FShuttlegentStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>(ShuttlegentEditorStyleKey::SearchBox))
					.HintText(LOCTEXT("SearchHint", "搜索会话 (Ctrl+K)"))
					.OnTextChanged_Lambda([this](const FText& Text)
					{
						SearchText = Text.ToString().TrimStartAndEnd();
						RebuildList();
					})
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(SessionList, SVerticalBox)
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildFooter()
				]
			]
		]
	];

	Model->OnSessionsChanged.AddSP(this, &SShuttlegentSessionSidebar::HandleSessionsChanged);
	RebuildList();
}

SShuttlegentSessionSidebar::~SShuttlegentSessionSidebar()
{
	if (Model.IsValid()) Model->OnSessionsChanged.RemoveAll(this);
}

void SShuttlegentSessionSidebar::FocusSearch()
{
	if (SearchBox.IsValid()) FSlateApplication::Get().SetKeyboardFocus(SearchBox, EFocusCause::SetDirectly);
}

TSharedRef<SWidget> SShuttlegentSessionSidebar::BuildFooter()
{
	return SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 0.0f, 0.0f, ShuttlegentEditorMetric::Space3)
		[
			FShuttlegentPanelWidgets::MakeHorizontalDivider()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(ArchivedButton, SComboButton)
			.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
			.HasDownArrow(false)
			.ToolTipText(LOCTEXT("ArchivedTip", "查看并恢复已归档的会话"))
			.OnGetMenuContent(this, &SShuttlegentSessionSidebar::BuildArchivedMenu)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(this, &SShuttlegentSessionSidebar::GetArchivedText)
				.Font(FShuttlegentStyle::CaptionFont())
				.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, ShuttlegentEditorMetric::Space2, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SShuttlegentSessionSidebar::GetPermissionText)
				.ToolTipText(LOCTEXT("PermissionTip", "运行时文件与命令工具的权限预设；在设置中修改"))
				.Font(FShuttlegentStyle::CaptionFont())
				.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				FShuttlegentPanelWidgets::MakeIconButton(SShuttlegentSessionSidebarPrivate::IconSettings, LOCTEXT("Settings", "设置"), FOnClicked::CreateLambda([]()
				{
					ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(TEXT("Settings"));
					if (SettingsModule != nullptr) SettingsModule->ShowViewer(ShuttlegentEditorName::SettingsContainer, ShuttlegentEditorName::SettingsCategory, ShuttlegentEditorName::SettingsSection);
					return FReply::Handled();
				}))
			]
		];
}

TSharedRef<SWidget> SShuttlegentSessionSidebar::BuildArchivedMenu()
{
	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;
	TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);

	const TArray<FShuttlegentSessionRecord> Archived = Model->GetSessions(true);
	if (Archived.IsEmpty())
	{
		Rows->AddSlot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoArchived", "没有已归档的会话"))
				.Font(FShuttlegentStyle::CaptionFont())
				.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
			];
	}

	for (const FShuttlegentSessionRecord& Record : Archived)
	{
		const FString SessionId = Record.SessionId;
		Rows->AddSlot()
			.AutoHeight()
			.Padding(0.0f, ShuttlegentEditorMetric::Space1)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(SShuttlegentSessionSidebarPrivate::GetRecordTitle(Record))
					.Font(FShuttlegentStyle::BodyFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
					.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(ShuttlegentEditorMetric::Space2, 0.0f, 0.0f, 0.0f)
				[
					FShuttlegentPanelWidgets::MakeTextButton(LOCTEXT("Restore", "恢复"), false, FOnClicked::CreateLambda([this, WeakModel, SessionId]()
					{
						if (ArchivedButton.IsValid()) ArchivedButton->SetIsOpen(false);

						const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
						if (Pinned.IsValid()) Pinned->RestoreSession(SessionId);
						return FReply::Handled();
					}))
				]
			];
	}

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
				.Padding(0.0f, 0.0f, 0.0f, ShuttlegentEditorMetric::Space2)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("ArchivedTitle", "已归档会话"))
					.Font(FShuttlegentStyle::EmphasisFont())
					.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					Rows
				]
			]
		];
}

TSharedRef<SWidget> SShuttlegentSessionSidebar::BuildSessionRow(const FShuttlegentSessionRecord& Record)
{
	TWeakPtr<FShuttlegentChatModel> WeakModel = Model;
	const FString SessionId = Record.SessionId;
	const bool bCurrent = SessionId == Model->GetCurrentSessionId();
	const FText Detail = Record.ModelName.IsEmpty()
		? FShuttlegentPanelWidgets::FormatSessionTime(Record.UpdatedAt)
		: FText::Format(LOCTEXT("SessionDetail", "{0} · {1}"), FText::FromString(Record.ModelName), FShuttlegentPanelWidgets::FormatSessionTime(Record.UpdatedAt));

	return SNew(SOverlay)

		+ SOverlay::Slot()
		[
			SNew(SButton)
			.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
			.ContentPadding(FMargin(ShuttlegentEditorMetric::Space3, ShuttlegentEditorMetric::Space2))
			.ToolTipText(SShuttlegentSessionSidebarPrivate::GetRecordTitle(Record))
			.OnClicked_Lambda([this, WeakModel, SessionId, bCurrent]()
			{
				const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
				if (Pinned.IsValid() && !bCurrent) Pinned->OpenSession(SessionId);
				OnSessionChosen.ExecuteIfBound();
				return FReply::Handled();
			})
			[
				SNew(SBorder)
				.BorderImage(bCurrent ? FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::CurrentSession) : FStyleDefaults::GetNoBrush())
				.Padding(FMargin(ShuttlegentEditorMetric::Space2, ShuttlegentEditorMetric::Space1))
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(SShuttlegentSessionSidebarPrivate::GetRecordTitle(Record))
						.Font(FShuttlegentStyle::BodyFont())
						.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Text))
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, ShuttlegentEditorMetric::Space1, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Font(FShuttlegentStyle::CaptionFont())
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis)
						.Text_Lambda([WeakModel, bCurrent, Detail]()
						{
							const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
							return bCurrent && Pinned.IsValid() ? FShuttlegentPanelWidgets::GetStatusText(Pinned->GetStatus()) : Detail;
						})
						.ColorAndOpacity_Lambda([WeakModel, bCurrent]()
						{
							const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
							return bCurrent && Pinned.IsValid() ? FShuttlegentPanelWidgets::GetStatusColor(Pinned->GetStatus()) : FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted);
						})
					]
				]
			]
		]

		+ SOverlay::Slot()
		.HAlign(HAlign_Left)
		[
			SNew(SBox)
			.WidthOverride(ShuttlegentEditorMetric::AccentLineWidth)
			.Visibility(bCurrent ? EVisibility::HitTestInvisible : EVisibility::Collapsed)
			[
				SNew(SImage)
				.Image(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::AccentLine))
			]
		];
}

void SShuttlegentSessionSidebar::RebuildList()
{
	if (!SessionList.IsValid()) return;

	SessionList->ClearChildren();

	TOptional<SShuttlegentSessionSidebarPrivate::EDaySection> LastSection;
	int32 Shown = 0;
	for (const FShuttlegentSessionRecord& Record : Model->GetSessions(false))
	{
		const FString Title = SShuttlegentSessionSidebarPrivate::GetRecordTitle(Record).ToString();
		if (!SearchText.IsEmpty() && !Title.Contains(SearchText) && !Record.ModelName.Contains(SearchText)) continue;

		const SShuttlegentSessionSidebarPrivate::EDaySection Section = SShuttlegentSessionSidebarPrivate::GetSection(Record.UpdatedAt);
		if (!LastSection.IsSet() || LastSection.GetValue() != Section)
		{
			SessionList->AddSlot()
				.AutoHeight()
				[
					SShuttlegentSessionSidebarPrivate::MakeSectionLabel(SShuttlegentSessionSidebarPrivate::GetSectionLabel(Section))
				];
			LastSection = Section;
		}

		SessionList->AddSlot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, ShuttlegentEditorMetric::Space1)
			[
				BuildSessionRow(Record)
			];
		++Shown;
	}

	if (Shown > 0) return;

	SessionList->AddSlot()
		.AutoHeight()
		.Padding(ShuttlegentEditorMetric::Space2, ShuttlegentEditorMetric::Space4)
		[
			SNew(STextBlock)
			.Text(SearchText.IsEmpty() ? LOCTEXT("NoSessions", "还没有会话") : LOCTEXT("NoMatch", "没有匹配的会话"))
			.Font(FShuttlegentStyle::CaptionFont())
			.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
		];
}

FText SShuttlegentSessionSidebar::GetPermissionText() const
{
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	const bool bWorkspaceWrite = IsValid(Settings) && Settings->PermissionMode == EShuttlegentPermissionMode::WorkspaceWrite;
	return bWorkspaceWrite ? LOCTEXT("PermissionWorkspaceWrite", "权限预设：工作区可写") : LOCTEXT("PermissionReadOnly", "权限预设：只读");
}

FText SShuttlegentSessionSidebar::GetArchivedText() const
{
	return FText::Format(LOCTEXT("Archived", "已归档 {0}"), FText::AsNumber(Model->GetSessions(true).Num()));
}

void SShuttlegentSessionSidebar::HandleSessionsChanged()
{
	RebuildList();
}

#undef LOCTEXT_NAMESPACE
