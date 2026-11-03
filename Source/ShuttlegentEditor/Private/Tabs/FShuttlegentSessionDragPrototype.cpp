#include "Tabs/FShuttlegentSessionDragPrototype.h"

#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/IConsoleManager.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Text/STextBlock.h"

#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentLog.h"

#define LOCTEXT_NAMESPACE "ShuttlegentSessionDragPrototype"

namespace FShuttlegentSessionDragPrototypePrivate
{
	TAutoConsoleVariable<bool> CVarSessionDrag(
		ShuttlegentEditorPrototype::SessionDragVariable,
		false,
		TEXT("Prototype: dragging a session out of the Shuttlegent session list starts a native dock tab drag with a placeholder tab."));

	/** 页签取得控件路径后把它交给原生页签拖拽；返回 false 表示下一帧再试。 */
	bool TryStartDrag(const TWeakPtr<SDockTab>& WeakTab, int32& FramesLeft)
	{
		const TSharedPtr<SDockTab> Tab = WeakTab.Pin();
		if (!Tab.IsValid()) return true;

		FSlateApplication& Slate = FSlateApplication::Get();
		if (!Slate.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
		{
			UE_LOG(LogShuttlegent, Log, TEXT("Session drag prototype: the left mouse button was released before the drag started; the placeholder tab stays next to the Shuttlegent tab."));
			return true;
		}
		if (Slate.IsDragDropping())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Session drag prototype: another drag and drop is in progress."));
			return true;
		}

		FWidgetPath FoundPath;
		if (!Slate.FindPathToWidget(Tab.ToSharedRef(), FoundPath))
		{
			--FramesLeft;
			if (FramesLeft > 0) return false;

			UE_LOG(LogShuttlegent, Warning, TEXT("Session drag prototype: the placeholder tab has no widget path after %d frames."), ShuttlegentEditorPrototype::MaxDragWaitFrames);
			return true;
		}

		const FVector2D CursorPosition = Slate.GetCursorPos();
		FPointerEvent PointerEvent(FSlateApplicationBase::CursorPointerIndex, CursorPosition, Slate.GetLastCursorPos(), Slate.GetPressedMouseButtons(), EKeys::LeftMouseButton, 0.0f, Slate.GetModifierKeys());

		// FindPathToWidget 的路径不带各层的虚拟指针位置，ProcessReply 沿它分发 OnDragEnter 时会越界；
		// 与引擎检测拖拽时一样（FSlateUser::DetectDrag）经弱路径重建
		const FWidgetPath TabPath = FWeakWidgetPath(FoundPath).ToWidgetPath(FWeakWidgetPath::EInterruptedPathHandling::ReturnInvalid, &PointerEvent);
		if (!TabPath.IsValid())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Session drag prototype: the placeholder tab path could not be rebuilt."));
			return true;
		}

		// 页签栏开始拖拽时要沿事件路径找自己的几何信息；自己构造的事件没有路径，须补上
		PointerEvent.SetEventPath(TabPath);
		const FReply Reply = Tab->OnDragDetected(TabPath.Widgets.Last().Geometry, PointerEvent);
		if (!Reply.GetDragDropContent().IsValid())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Session drag prototype: the tab well did not start a docking drag (the tab cannot leave its well, or its tab manager does not allow dragging)."));
			return true;
		}

		const FWidgetPath UnderCursor = Slate.LocateWindowUnderMouse(CursorPosition, Slate.GetInteractiveTopLevelWindows());
		if (!UnderCursor.IsValid())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Session drag prototype: no window under the cursor; the docking drag cannot start."));
			return true;
		}

		Slate.ProcessReply(TabPath, Reply, &UnderCursor, &PointerEvent);

		// ProcessReply 让页签路径上的控件收到 OnDragEnter；引擎原生检测拖拽时会把“上一帧光标下的控件”设为这条路径，
		// 下一次移动便给其中已不在光标下的控件发 OnDragLeave（SlateApplication.cpp:5714、:5752-5796）。
		// 这里光标下的是页面，引擎不会补发，页签栏会收到第二次 OnDragEnter 而把同一页签加两遍；故在此按原生顺序补齐
		const FDragDropEvent DragDropEvent(PointerEvent, Reply.GetDragDropContent());
		for (int32 Index = TabPath.Widgets.Num() - 1; Index >= 0; --Index)
		{
			const TSharedRef<SWidget>& Widget = TabPath.Widgets[Index].Widget;
			if (!UnderCursor.ContainsWidget(&Widget.Get())) Widget->OnDragLeave(DragDropEvent);
		}

		// 光标下、但不在页签路径上的控件（页面一侧）同样按原生顺序收到 OnDragEnter，之后离开时的 OnDragLeave 才成对
		for (int32 Index = UnderCursor.Widgets.Num() - 1; Index >= 0; --Index)
		{
			const FArrangedWidget& Arranged = UnderCursor.Widgets[Index];
			if (!TabPath.ContainsWidget(&Arranged.Widget.Get())) Arranged.Widget->OnDragEnter(Arranged.Geometry, DragDropEvent);
		}

		UE_LOG(LogShuttlegent, Log, TEXT("Session drag prototype: docking drag started (%d frame(s) after the tab was inserted)."), ShuttlegentEditorPrototype::MaxDragWaitFrames - FramesLeft + 1);
		return true;
	}
}

void FShuttlegentSessionDragPrototype::Begin(const TSharedPtr<SDockTab>& HubTab, const FString& SessionId, const FString& Title)
{
	if (!FShuttlegentSessionDragPrototypePrivate::CVarSessionDrag.GetValueOnGameThread()) return;

	const TSharedPtr<FTabManager> TabManager = HubTab.IsValid() ? HubTab->GetTabManagerPtr() : nullptr;
	if (!TabManager.IsValid())
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Session drag prototype: the Shuttlegent tab has no tab manager."));
		return;
	}

	const FText Label = FText::FromString(Title.IsEmpty() ? SessionId : Title);
	const TSharedRef<SDockTab> Tab = SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(Label)
		[
			SNew(STextBlock).Text(FText::Format(LOCTEXT("Placeholder", "原型：{0}"), Label))
		];
	TabManager->InsertNewDocumentTab(ShuttlegentEditorName::ChatTabId, ShuttlegentEditorPrototype::SessionDragTabId, FTabManager::FLiveTabSearch(ShuttlegentEditorName::ChatTabId), Tab);
	UE_LOG(LogShuttlegent, Log, TEXT("Session drag prototype: inserted a placeholder tab for session %s."), *SessionId);

	// 页签刚插入，要等布局后才有几何信息
	const TWeakPtr<SDockTab> WeakTab = Tab;
	const TSharedRef<int32> FramesLeft = MakeShared<int32>(ShuttlegentEditorPrototype::MaxDragWaitFrames);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakTab, FramesLeft](float DeltaTime)
	{
		return !FShuttlegentSessionDragPrototypePrivate::TryStartDrag(WeakTab, *FramesLeft);
	}));
}

#undef LOCTEXT_NAMESPACE
