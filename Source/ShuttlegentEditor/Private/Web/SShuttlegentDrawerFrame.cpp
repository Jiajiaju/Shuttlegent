#include "Web/SShuttlegentDrawerFrame.h"

void SShuttlegentDrawerFrame::Construct(const FArguments& InArgs)
{
	OnDragLeft = InArgs._OnDragLeft;

	ChildSlot
	[
		InArgs._Content.Widget
	];
}

void SShuttlegentDrawerFrame::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	SCompoundWidget::OnDragLeave(DragDropEvent);
	OnDragLeft.ExecuteIfBound();
}
