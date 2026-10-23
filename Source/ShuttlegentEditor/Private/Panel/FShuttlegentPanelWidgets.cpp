#include "Panel/FShuttlegentPanelWidgets.h"

#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/SMultiLineEditableText.h"
#include "Widgets/Text/STextBlock.h"

#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentStyle.h"

#define LOCTEXT_NAMESPACE "ShuttlegentPanelWidgets"

namespace FShuttlegentPanelWidgetsPrivate
{
	// Time formats of the session list and the run log (FDateTime::ToString)
	inline const TCHAR* FormatClock = TEXT("%H:%M");
	inline const TCHAR* FormatClockSeconds = TEXT("%H:%M:%S");
	inline const TCHAR* FormatMonthDay = TEXT("%m-%d");
}

TSharedRef<SButton> FShuttlegentPanelWidgets::MakeIconButton(const TCHAR* IconName, const FText& ToolTip, FOnClicked OnClicked)
{
	return SNew(SButton)
		.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
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
				.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
			]
		];
}

TSharedRef<SButton> FShuttlegentPanelWidgets::MakeFlatTextButton(const TAttribute<FText>& Label, const FText& ToolTip, FOnClicked OnClicked)
{
	return SNew(SButton)
		.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(ShuttlegentEditorStyleKey::FlatButton))
		.ContentPadding(FMargin(ShuttlegentEditorMetric::Space2, ShuttlegentEditorMetric::Space1))
		.ToolTipText(ToolTip)
		.OnClicked(OnClicked)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(FShuttlegentStyle::CaptionFont())
			.ColorAndOpacity(FShuttlegentStyle::Tint(ShuttlegentEditorColor::Muted))
		];
}

TSharedRef<SButton> FShuttlegentPanelWidgets::MakeTextButton(const FText& Label, bool bPrimary, FOnClicked OnClicked)
{
	return SNew(SButton)
		.ButtonStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FButtonStyle>(bPrimary ? ShuttlegentEditorStyleKey::PrimaryButton : ShuttlegentEditorStyleKey::SecondaryButton))
		.OnClicked(OnClicked)
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(FShuttlegentStyle::BodyFont())
			.ColorAndOpacity(FShuttlegentStyle::Tint(bPrimary ? ShuttlegentEditorColor::OnPrimary : ShuttlegentEditorColor::Text))
		];
}

TSharedRef<SWidget> FShuttlegentPanelWidgets::MakeHorizontalDivider()
{
	return SNew(SBox)
		.HeightOverride(ShuttlegentEditorMetric::BorderWidth)
		[
			SNew(SImage)
			.Image(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Divider))
		];
}

TSharedRef<SWidget> FShuttlegentPanelWidgets::MakeVerticalDivider()
{
	return SNew(SBox)
		.WidthOverride(ShuttlegentEditorMetric::BorderWidth)
		[
			SNew(SImage)
			.Image(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Divider))
		];
}

TSharedRef<SWidget> FShuttlegentPanelWidgets::MakeDetailText(const TAttribute<FText>& Text)
{
	return SNew(SBorder)
		.BorderImage(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Background))
		.Padding(ShuttlegentEditorMetric::Space2)
		[
			SNew(SMultiLineEditableText)
			.Text(Text)
			.TextStyle(&FShuttlegentStyle::Get().GetWidgetStyle<FTextBlockStyle>(ShuttlegentEditorStyleKey::MarkdownCodeBlock))
			.IsReadOnly(true)
			.AutoWrapText(true)
		];
}

FText FShuttlegentPanelWidgets::GetStatusText(EShuttlegentChatStatus Status)
{
	switch (Status)
	{
	case EShuttlegentChatStatus::Connecting:
		{
			return LOCTEXT("StatusConnecting", "正在连接");
		}
	case EShuttlegentChatStatus::Connected:
		{
			return LOCTEXT("StatusConnected", "已连接");
		}
	case EShuttlegentChatStatus::Querying:
		{
			return LOCTEXT("StatusQuerying", "正在查询");
		}
	case EShuttlegentChatStatus::AwaitingPermission:
		{
			return LOCTEXT("StatusAwaitingPermission", "等待本次授权");
		}
	case EShuttlegentChatStatus::Stopping:
		{
			return LOCTEXT("StatusStopping", "正在停止");
		}
	case EShuttlegentChatStatus::Stopped:
		{
			return LOCTEXT("StatusStopped", "已停止");
		}
	case EShuttlegentChatStatus::Failed:
		{
			return LOCTEXT("StatusFailed", "查询失败");
		}
	case EShuttlegentChatStatus::Lost:
		{
			return LOCTEXT("StatusLost", "连接已断开");
		}
	case EShuttlegentChatStatus::Resuming:
		{
			return LOCTEXT("StatusResuming", "正在恢复会话");
		}
	case EShuttlegentChatStatus::NotConnected:
	default:
		{
			return LOCTEXT("StatusNotConnected", "未连接");
		}
	}
}

FSlateColor FShuttlegentPanelWidgets::GetStatusColor(EShuttlegentChatStatus Status)
{
	const bool bWarning = Status == EShuttlegentChatStatus::AwaitingPermission || Status == EShuttlegentChatStatus::Failed || Status == EShuttlegentChatStatus::Lost;
	return FShuttlegentStyle::Tint(bWarning ? ShuttlegentEditorColor::Warning : ShuttlegentEditorColor::Muted);
}

FText FShuttlegentPanelWidgets::FormatSessionTime(const FDateTime& UtcTime)
{
	const FDateTime Local = UtcTime + (FDateTime::Now() - FDateTime::UtcNow());
	const FDateTime Today = FDateTime::Now().GetDate();

	if (Local.GetDate() == Today) return FText::FromString(Local.ToString(FShuttlegentPanelWidgetsPrivate::FormatClock));
	if (Local.GetDate() == Today - FTimespan::FromDays(1.0)) return FText::Format(LOCTEXT("Yesterday", "昨天 {0}"), FText::FromString(Local.ToString(FShuttlegentPanelWidgetsPrivate::FormatClock)));
	return FText::FromString(Local.ToString(FShuttlegentPanelWidgetsPrivate::FormatMonthDay));
}

FText FShuttlegentPanelWidgets::FormatLogTime(const FDateTime& UtcTime)
{
	const FDateTime Local = UtcTime + (FDateTime::Now() - FDateTime::UtcNow());
	return FText::FromString(Local.ToString(FShuttlegentPanelWidgetsPrivate::FormatClockSeconds));
}

#undef LOCTEXT_NAMESPACE
