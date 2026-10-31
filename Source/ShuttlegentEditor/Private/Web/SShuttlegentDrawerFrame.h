#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 面板 Content Browser 抽屉的外框。在拖拽离开时发出通知，使抽屉能在资产被拖出的瞬间让开，
 * 与编辑器自带的内容抽屉行为一致（SContentBrowser::OnDragLeave 会将其收起）。
 * Slate 仅在光标离开整个外框时发送离开事件，在外框内部的控件之间移动时不会发送。
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
