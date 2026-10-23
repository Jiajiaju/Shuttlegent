#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FShuttlegentChatModel;
class SBox;
class SButton;
struct FShuttlegentChatEntry;

/**
 * Runtime escalation card (Shuttlegent UI interaction spec section 5.1) shown in place of the
 * composer while session/request_permission waits. Focus lands on the title, which runs nothing;
 * only a button reached with Tab, or clicked, answers. Esc rejects.
 */
class SShuttlegentElevationCard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShuttlegentElevationCard) {}
		SLATE_ARGUMENT(TSharedPtr<FShuttlegentChatModel>, Model)
	SLATE_END_ARGS()

private:
	TSharedPtr<FShuttlegentChatModel> Model;
	TSharedPtr<SBox> Body;
	TSharedPtr<SButton> Title;

	/** Permission entry the card answers; 0 when none. */
	int32 EntryId = 0;

public:
	void Construct(const FArguments& InArgs);

	/** Fills the card from a pending permission entry. */
	void ShowRequest(const TSharedPtr<FShuttlegentChatEntry>& Permission);

	/** Moves keyboard focus to the title. */
	void FocusTitle();

	//~ Begin SWidget Interface.
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	//~ End SWidget Interface.

private:
	TSharedRef<SWidget> BuildBody(const TSharedPtr<FShuttlegentChatEntry>& Permission);
	void Answer(bool bAllow);
};
