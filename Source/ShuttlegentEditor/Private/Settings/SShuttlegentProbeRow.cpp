#include "Settings/SShuttlegentProbeRow.h"

#include "DetailLayoutBuilder.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#include "FShuttlegentConnectionTester.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentSettings.h"

#define LOCTEXT_NAMESPACE "ShuttlegentProbeRow"

namespace SShuttlegentProbeRowPrivate
{
	EShuttlegentConnectionStatus GetStatus(int32 Index)
	{
		const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
		return IsValid(Settings) && Settings->Connections.IsValidIndex(Index) ? Settings->Connections[Index].Status : EShuttlegentConnectionStatus::Untested;
	}
}

void SShuttlegentProbeRow::Construct(const FArguments& InArgs)
{
	ConnectionIndex = InArgs._ConnectionIndex;

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SButton)
			.ButtonStyle(FAppStyle::Get(), ShuttlegentEditorSettingsUi::ProbeButtonStyle)
			.ToolTipText(LOCTEXT("ProbeTooltip", "Test connection: detect the protocol and list the models. Details go to the Output Log (LogShuttlegent)."))
			.IsEnabled_Lambda([this]() { return !IsTesting(); })
			.OnClicked(this, &SShuttlegentProbeRow::HandleClicked)
			[
				SNew(SImage)
				.Image(FAppStyle::GetBrush(ShuttlegentEditorSettingsUi::ProbeIcon))
				.ColorAndOpacity(FSlateColor::UseForeground())
			]
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ShuttlegentEditorSettingsUi::ProbeButtonGap, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(ShuttlegentEditorSettingsUi::ProbeStatusDot))
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(this, &SShuttlegentProbeRow::GetStatusColor)
		]
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(ShuttlegentEditorSettingsUi::ProbeLineGap, 0.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SShuttlegentProbeRow::GetStatusText)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.ColorAndOpacity(this, &SShuttlegentProbeRow::GetStatusColor)
		]
	];
}

FReply SShuttlegentProbeRow::HandleClicked() const
{
	FShuttlegentConnectionTester::Test(ConnectionIndex.Get());
	return FReply::Handled();
}

bool SShuttlegentProbeRow::IsTesting() const
{
	return SShuttlegentProbeRowPrivate::GetStatus(ConnectionIndex.Get()) == EShuttlegentConnectionStatus::Testing;
}

FText SShuttlegentProbeRow::GetStatusText() const
{
	switch (SShuttlegentProbeRowPrivate::GetStatus(ConnectionIndex.Get()))
	{
	case EShuttlegentConnectionStatus::Testing:
		{
			return LOCTEXT("Testing", "Testing...");
		}
	case EShuttlegentConnectionStatus::Connected:
		{
			return LOCTEXT("Connected", "Connected");
		}
	case EShuttlegentConnectionStatus::Failed:
		{
			return LOCTEXT("Failed", "Failed");
		}
	case EShuttlegentConnectionStatus::Untested:
	default:
		{
			return LOCTEXT("Untested", "Untested");
		}
	}
}

FSlateColor SShuttlegentProbeRow::GetStatusColor() const
{
	switch (SShuttlegentProbeRowPrivate::GetStatus(ConnectionIndex.Get()))
	{
	case EShuttlegentConnectionStatus::Connected:
		{
			return FStyleColors::Success;
		}
	case EShuttlegentConnectionStatus::Failed:
		{
			return FStyleColors::Error;
		}
	default:
		{
			return FSlateColor::UseSubduedForeground();
		}
	}
}

#undef LOCTEXT_NAMESPACE
