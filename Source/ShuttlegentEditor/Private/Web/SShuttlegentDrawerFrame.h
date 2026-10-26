#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Frame of the panel's Content Browser drawer. Reports when a drag leaves it, so the drawer can get out of the way the
 * moment an asset is dragged out, as the editor's own content drawer does (SContentBrowser::OnDragLeave dismisses it).
 * Slate sends the leave only when the cursor exits the whole frame, not when it moves between widgets inside it.
 */
class SShuttlegentDrawerFrame : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShuttlegentDrawerFrame) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_EVENT(FSimpleDelegate, OnDragLeft)
	SLATE_END_ARGS()

private:
	FSimpleDelegate OnDragLeft;

public:
	void Construct(const FArguments& InArgs);

	//~ Begin SWidget Interface.
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	//~ End SWidget Interface.
};
