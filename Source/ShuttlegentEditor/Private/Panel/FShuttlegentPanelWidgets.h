#pragma once

#include "CoreMinimal.h"
#include "Framework/SlateDelegates.h"
#include "Misc/Attribute.h"
#include "Styling/SlateColor.h"

#include "Chat/EShuttlegentChatStatus.h"

class SButton;
class SWidget;

/** Building blocks shared by the panel's widgets, and the wording of conversation states. Pure functions. */
struct FShuttlegentPanelWidgets
{
	/** Icon-only button with an accessible name in its tooltip. */
	static TSharedRef<SButton> MakeIconButton(const TCHAR* IconName, const FText& ToolTip, FOnClicked OnClicked);

	/** Borderless text button with a caption font. */
	static TSharedRef<SButton> MakeFlatTextButton(const TAttribute<FText>& Label, const FText& ToolTip, FOnClicked OnClicked);

	/** Primary or secondary text button. */
	static TSharedRef<SButton> MakeTextButton(const FText& Label, bool bPrimary, FOnClicked OnClicked);

	/** 1 px horizontal line in the border color. */
	static TSharedRef<SWidget> MakeHorizontalDivider();

	/** 1 px vertical line in the border color. */
	static TSharedRef<SWidget> MakeVerticalDivider();

	/** Read-only, selectable monospaced text of tool parameters and results. Takes no IME input. */
	static TSharedRef<SWidget> MakeDetailText(const TAttribute<FText>& Text);

	/** Header status wording (prototype labels). */
	static FText GetStatusText(EShuttlegentChatStatus Status);

	/** Warning states use the warning color, the rest the secondary text color. */
	static FSlateColor GetStatusColor(EShuttlegentChatStatus Status);

	/** Session list time: "HH:mm" today, "昨天 HH:mm" yesterday, "MM-dd" earlier; local time. */
	static FText FormatSessionTime(const FDateTime& UtcTime);

	/** Run log time "HH:mm:ss" in local time. */
	static FText FormatLogTime(const FDateTime& UtcTime);
};
