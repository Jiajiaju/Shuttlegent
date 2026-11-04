#include "Tabs/FShuttlegentSessionTabRegistry.h"

#include "Containers/Ticker.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"
#include "LevelEditor.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/SWindow.h"

#include "Chat/FShuttlegentChatView.h"
#include "Chat/ShuttlegentChatModel.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentLog.h"
#include "Web/SShuttlegentWebPanel.h"

#define LOCTEXT_NAMESPACE "ShuttlegentSessionTabRegistry"

namespace FShuttlegentSessionTabRegistryPrivate
{
	/** 拖拽没能开始：把为拖出而藏起的新会话窗口显示出来，放到光标处（光标落在页签头上）。 */
	void RevealWindow(const TSharedRef<SDockTab>& Tab)
	{
		const TSharedPtr<SWindow> Window = Tab->GetParentWindow();
		if (!Window.IsValid() || Window->GetOpacity() >= 1.0f) return;

		Window->SetOpacity(1.0f);
		Window->MoveWindowTo(FSlateApplication::Get().GetCursorPos() - ShuttlegentEditorSessionTab::WindowGrabOffset);
	}

	/**
	 * 页签完成排版后交给原生页签拖拽；返回 false 表示下一帧再试。从页面之外发起拖拽须补齐引擎在正常输入分发中
	 * 自动准备的三项状态，缺一项即崩溃（设计文档 §17.1）：事件路径、带指针位置的路径、与拖拽起点成对的进出事件。
	 */
	bool TryStartDrag(const TWeakPtr<SDockTab>& WeakTab, int32& FramesLeft)
	{
		const TSharedPtr<SDockTab> Tab = WeakTab.Pin();
		if (!Tab.IsValid()) return true;

		FSlateApplication& Slate = FSlateApplication::Get();
		if (!Slate.GetPressedMouseButtons().Contains(EKeys::LeftMouseButton))
		{
			UE_LOG(LogShuttlegent, Log, TEXT("Session tab drag: the left mouse button was released before the drag started; the tab stays where it is."));
			RevealWindow(Tab.ToSharedRef());
			return true;
		}
		if (Slate.IsDragDropping())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Session tab drag: another drag and drop is in progress."));
			RevealWindow(Tab.ToSharedRef());
			return true;
		}

		// 跟随光标的预览窗口取页签所在区域此刻的尺寸（FDockingDragOperation 的 LastContentSize）；新窗口还没排版时为 0，预览窗口看不见
		FWidgetPath FoundPath;
		const FVector2D TabSize = Tab->GetTickSpaceGeometry().GetLocalSize();
		if (TabSize.X <= 0.0 || TabSize.Y <= 0.0 || !Slate.FindPathToWidget(Tab.ToSharedRef(), FoundPath))
		{
			--FramesLeft;
			if (FramesLeft > 0) return false;

			UE_LOG(LogShuttlegent, Warning, TEXT("Session tab drag: the tab was not laid out after %d frames."), ShuttlegentEditorSessionTab::MaxDragWaitFrames);
			RevealWindow(Tab.ToSharedRef());
			return true;
		}

		const FVector2D CursorPosition = Slate.GetCursorPos();
		FPointerEvent PointerEvent(FSlateApplicationBase::CursorPointerIndex, CursorPosition, Slate.GetLastCursorPos(), Slate.GetPressedMouseButtons(), EKeys::LeftMouseButton, 0.0f, Slate.GetModifierKeys());

		// FindPathToWidget 的路径不带各层的虚拟指针位置，ProcessReply 沿它分发 OnDragEnter 时会越界；
		// 与引擎检测拖拽时一样（FSlateUser::DetectDrag）经弱路径重建
		const FWidgetPath TabPath = FWeakWidgetPath(FoundPath).ToWidgetPath(FWeakWidgetPath::EInterruptedPathHandling::ReturnInvalid, &PointerEvent);
		if (!TabPath.IsValid())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Session tab drag: the tab path could not be rebuilt."));
			RevealWindow(Tab.ToSharedRef());
			return true;
		}

		// 页签栏开始拖拽时要沿事件路径找自己的几何信息；自己构造的事件没有路径，须补上。
		// 抓取点按页签头的中心算（引擎把抓取点限制在页签内，SDockTab.cpp:92）：光标不在藏起的新窗口上，预览窗口仍让光标落在页签头中央
		PointerEvent.SetEventPath(TabPath);
		const FGeometry& TabGeometry = TabPath.Widgets.Last().Geometry;
		FPointerEvent GrabEvent(FSlateApplicationBase::CursorPointerIndex, TabGeometry.GetAbsolutePositionAtCoordinates(FVector2D(0.5, 0.5)), Slate.GetLastCursorPos(), Slate.GetPressedMouseButtons(), EKeys::LeftMouseButton, 0.0f, Slate.GetModifierKeys());
		GrabEvent.SetEventPath(TabPath);
		const FReply Reply = Tab->OnDragDetected(TabGeometry, GrabEvent);
		if (!Reply.GetDragDropContent().IsValid())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Session tab drag: the tab well did not start a docking drag (the tab cannot leave its well, or its tab manager does not allow dragging)."));
			RevealWindow(Tab.ToSharedRef());
			return true;
		}

		const FWidgetPath UnderCursor = Slate.LocateWindowUnderMouse(CursorPosition, Slate.GetInteractiveTopLevelWindows());
		if (!UnderCursor.IsValid())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Session tab drag: no window under the cursor; the docking drag cannot start."));
			RevealWindow(Tab.ToSharedRef());
			return true;
		}

		Slate.ProcessReply(TabPath, Reply, &UnderCursor, &PointerEvent);

		// ProcessReply 让页签路径上的控件收到 OnDragEnter；引擎原生检测拖拽时会把“上一帧光标下的控件”设为这条路径，
		// 下一次移动便给其中已不在光标下的控件发 OnDragLeave（SlateApplication.cpp:5714、:5752-5796）。
		// 这里光标下的是页面，引擎不会补发，页签栏会收到第二次 OnDragEnter 而把同一页签加两遍；故在此按原生顺序补齐。
		// 新会话窗口放在光标之外，其页签栏随即收到 OnDragLeave：页签离开页签栏，空窗口关闭，跟随光标的预览窗口出现（SDockingTabWell.cpp:538）
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

		UE_LOG(LogShuttlegent, Log, TEXT("Session tab drag: docking drag started (content %.0fx%.0f)."), TabSize.X, TabSize.Y);
		return true;
	}
}

void FShuttlegentSessionTabRegistry::Initialize(const TSharedRef<FShuttlegentChatModel>& InModel, const TSharedRef<FShuttlegentChatView>& InHubView)
{
	Model = InModel;
	HubView = InHubView;

	FLevelEditorModule& LevelEditor = FModuleManager::LoadModuleChecked<FLevelEditorModule>(ShuttlegentEditorSessionTab::LevelEditorModule);
	RegisterTabsHandle = LevelEditor.OnRegisterTabs().AddSP(this, &FShuttlegentSessionTabRegistry::RegisterRestoredTabs);
	LevelEditorCreatedHandle = LevelEditor.OnLevelEditorCreated().AddSP(this, &FShuttlegentSessionTabRegistry::HandleLevelEditorCreated);

	// 插件在关卡编辑器之后才加载（如在编辑器中启用）时，本次已错过布局恢复，记下的页签无从接回
	if (LevelEditor.GetLevelEditorTabManager().IsValid()) FinishRestore();
}

void FShuttlegentSessionTabRegistry::Shutdown()
{
	FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(ShuttlegentEditorSessionTab::LevelEditorModule);
	if (LevelEditor != nullptr)
	{
		LevelEditor->OnRegisterTabs().Remove(RegisterTabsHandle);
		LevelEditor->OnLevelEditorCreated().Remove(LevelEditorCreatedHandle);
	}

	// 编辑器退出时先由引擎保存布局；绑定沿用最后一次写入的配置，不在此处改写
	for (const FShuttlegentSessionTab& Entry : Tabs)
	{
		ReleaseTab(Entry);
	}
	Tabs.Reset();
	for (const FShuttlegentSessionBinding& Pending : PendingRestore)
	{
		UnregisterTab(MakeTabType(Pending.ViewId), PendingTabManager);
	}
	PendingRestore.Reset();
	Model.Reset();
}

void FShuttlegentSessionTabRegistry::BeginDrag(const FString& SessionId)
{
	if (SessionId.IsEmpty() || !Model.IsValid()) return;

	TSharedPtr<SDockTab> Tab = FindTab(SessionId);
	if (!Tab.IsValid()) Tab = CreateTab(SessionId);
	if (!Tab.IsValid()) return;

	// 新建的页签要等布局后才有几何信息
	const TWeakPtr<SDockTab> WeakTab = Tab;
	const TSharedRef<int32> FramesLeft = MakeShared<int32>(ShuttlegentEditorSessionTab::MaxDragWaitFrames);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([WeakTab, FramesLeft](float DeltaTime)
	{
		return !FShuttlegentSessionTabRegistryPrivate::TryStartDrag(WeakTab, *FramesLeft);
	}));
}

bool FShuttlegentSessionTabRegistry::FocusSession(const FString& SessionId) const
{
	const TSharedPtr<SDockTab> Tab = FindTab(SessionId);
	if (!Tab.IsValid()) return false;

	// DrawAttention 只在所在窗口已激活或正在处理输入时才把窗口提到前面；点会话列表是页面经 Bridge 发来的，须自己提
	Tab->DrawAttention();
	const TSharedPtr<SWindow> Window = Tab->GetParentWindow();
	if (Window.IsValid()) Window->BringToFront(true);
	return true;
}

void FShuttlegentSessionTabRegistry::UpdateVisibility()
{
	for (const FShuttlegentSessionTab& Entry : Tabs)
	{
		const TSharedPtr<SDockTab> Tab = Entry.Tab.Pin();
		const bool bForeground = Tab.IsValid() && Tab->IsForeground();
		if (Entry.View.IsValid()) Entry.View->SetVisible(bForeground);
		if (bForeground) SyncWindowTitle(Entry.ViewId, Tab.ToSharedRef());
	}
	SaveBindings();
}

void FShuttlegentSessionTabRegistry::RestoreHub()
{
	const TSharedPtr<FShuttlegentChatView> Hub = HubView.Pin();
	if (!Hub.IsValid() || !Model.IsValid()) return;

	FString SessionId;
	GConfig->GetString(ShuttlegentEditorWeb::ConfigSection, ShuttlegentEditorSessionTab::ConfigKeyHubSession, SessionId, GEditorPerProjectIni);
	if (!Model->IsSessionAvailable(SessionId)) return;

	// 一个会话只在一处显示：绑定在会话页签上的会话归那个页签
	const TArray<FShuttlegentSessionBinding> Bindings = LoadBindings();
	if (Bindings.ContainsByPredicate([&SessionId](const FShuttlegentSessionBinding& Binding) { return Binding.SessionId == SessionId; })) return;

	Hub->OpenSession(SessionId, true);
	UE_LOG(LogShuttlegent, Log, TEXT("Main tab restored session %s; it resumes when the tab is first shown."), *SessionId);
}

void FShuttlegentSessionTabRegistry::RegisterRestoredTabs(TSharedPtr<FTabManager> TabManager)
{
	if (!TabManager.IsValid() || !Model.IsValid()) return;

	// 关卡编辑器重建（加载布局）时旧管理器里的页签随之销毁；按配置在新管理器上接回
	for (const FShuttlegentSessionTab& Entry : Tabs)
	{
		ReleaseTab(Entry);
	}
	Tabs.Reset();
	for (const FShuttlegentSessionBinding& Pending : PendingRestore)
	{
		UnregisterTab(MakeTabType(Pending.ViewId), PendingTabManager);
	}

	PendingRestore = LoadBindings();
	PendingTabManager = TabManager;
	const TWeakPtr<FShuttlegentSessionTabRegistry> WeakThis = AsShared();
	const FSlateIcon Icon(ShuttlegentEditorName::StyleSetName, ShuttlegentEditorStyleKey::TabIcon);
	for (const FShuttlegentSessionBinding& Pending : PendingRestore)
	{
		const FGuid ViewId = Pending.ViewId;
		const FName TabType = MakeTabType(ViewId);
		TabManager->RegisterTabSpawner(TabType, FOnSpawnTab::CreateLambda([WeakThis, ViewId](const FSpawnTabArgs& Args)
		{
			const TSharedPtr<FShuttlegentSessionTabRegistry> This = WeakThis.Pin();
			return This.IsValid() ? This->SpawnRestoredTab(ViewId) : SNew(SDockTab);
		}))
			.SetIcon(Icon)
			.SetMenuType(ETabSpawnerMenuType::Hidden);
		FTabManager::RegisterDefaultTabWindowSize(FTabId(TabType), ShuttlegentEditorSessionTab::WindowSize);
	}
	UE_LOG(LogShuttlegent, Log, TEXT("Session tabs: %d saved tab(s) registered for the level editor layout."), PendingRestore.Num());
}

void FShuttlegentSessionTabRegistry::HandleLevelEditorCreated(TSharedPtr<ILevelEditor> LevelEditor)
{
	FinishRestore();
}

void FShuttlegentSessionTabRegistry::FinishRestore()
{
	for (const FShuttlegentSessionBinding& Pending : PendingRestore)
	{
		UnregisterTab(MakeTabType(Pending.ViewId), PendingTabManager);
		UE_LOG(LogShuttlegent, Log, TEXT("Session tab %s is not in the restored layout; its binding is dropped."), *Pending.ViewId.ToString(EGuidFormats::Digits));
	}
	PendingRestore.Reset();
	PendingTabManager.Reset();
	bRestored = true;
	SaveBindings();

	// 已不存在的会话页签留下的新对话草稿在此清理；关闭页签时不删，编辑器退出时页签同样会被关闭
	TSet<FString> KeepViewKeys;
	for (const FShuttlegentSessionTab& Entry : Tabs)
	{
		KeepViewKeys.Add(Entry.ViewId.ToString(EGuidFormats::Digits));
	}
	if (Model.IsValid()) Model->GetDrafts().RemoveViewsExcept(KeepViewKeys);
}

FLinearColor FShuttlegentSessionTabRegistry::GetAttentionColor(EShuttlegentAttention Attention)
{
	if (Attention == EShuttlegentAttention::Input) return ShuttlegentEditorColor::AttentionInput;
	if (Attention == EShuttlegentAttention::Done) return ShuttlegentEditorColor::AttentionDone;
	return FLinearColor::Transparent;
}

const FShuttlegentSessionTab* FShuttlegentSessionTabRegistry::FindByView(const FGuid& ViewId) const
{
	return Tabs.FindByPredicate([&ViewId](const FShuttlegentSessionTab& Entry) { return Entry.ViewId == ViewId; });
}

TSharedPtr<SDockTab> FShuttlegentSessionTabRegistry::FindTab(const FString& SessionId) const
{
	if (SessionId.IsEmpty()) return nullptr;

	const FShuttlegentSessionTab* Found = Tabs.FindByPredicate([&SessionId](const FShuttlegentSessionTab& Entry) { return Entry.View.IsValid() && Entry.View->GetCurrentSessionId() == SessionId; });
	return Found != nullptr ? Found->Tab.Pin() : nullptr;
}

FName FShuttlegentSessionTabRegistry::MakeTabType(const FGuid& ViewId)
{
	// 类型名带 ViewId：布局按类型名保存，重启后据此接回原位置
	return FName(*(FString(ShuttlegentEditorSessionTab::TabTypePrefix) + ViewId.ToString(EGuidFormats::Digits)));
}

TArray<FShuttlegentSessionBinding> FShuttlegentSessionTabRegistry::LoadBindings()
{
	TArray<FString> Values;
	GConfig->GetArray(ShuttlegentEditorWeb::ConfigSection, ShuttlegentEditorSessionTab::ConfigKeySessionTabs, Values, GEditorPerProjectIni);

	TArray<FShuttlegentSessionBinding> Bindings;
	for (const FString& Value : Values)
	{
		FString ViewText;
		FString SessionId;
		if (!Value.Split(ShuttlegentEditorSessionTab::BindingSeparator, &ViewText, &SessionId)) ViewText = Value;

		FShuttlegentSessionBinding Binding;
		Binding.SessionId = SessionId;
		if (!FGuid::Parse(ViewText, Binding.ViewId) || Bindings.ContainsByPredicate([&Binding](const FShuttlegentSessionBinding& Existing) { return Existing.ViewId == Binding.ViewId; }))
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Session tabs: ignored the saved entry '%s'."), *Value);
			continue;
		}
		Bindings.Add(Binding);
	}
	return Bindings;
}

void FShuttlegentSessionTabRegistry::UnregisterTab(const FName& TabType, const TWeakPtr<FTabManager>& TabManager)
{
	const TSharedPtr<FTabManager> Pinned = TabManager.Pin();
	if (Pinned.IsValid()) Pinned->UnregisterTabSpawner(TabType);
	FTabManager::UnregisterDefaultTabWindowSize(FTabId(TabType));
}

void FShuttlegentSessionTabRegistry::ReleaseTab(const FShuttlegentSessionTab& Entry)
{
	// 视图交还对话：空闲的被释放，回合仍在运行的转入后台继续
	if (Entry.Panel.IsValid()) Entry.Panel->SetHostTabOpen(false);
	if (Entry.View.IsValid()) Entry.View->Shutdown();
	UnregisterTab(Entry.TabType, Entry.TabManager);
}

TSharedRef<SDockTab> FShuttlegentSessionTabRegistry::BuildTab(const FGuid& ViewId, const TSharedRef<FTabManager>& TabManager, const FString& SessionId, bool bDeferResume)
{
	FShuttlegentSessionTab Entry;
	Entry.ViewId = ViewId;
	Entry.TabType = MakeTabType(ViewId);
	Entry.TabManager = TabManager;
	Entry.View = MakeShared<FShuttlegentChatView>();
	Entry.View->SetDraftViewKey(ViewId.ToString(EGuidFormats::Digits));
	Entry.View->Initialize(Model.ToSharedRef(), EShuttlegentViewMode::Session);
	Entry.View->OpenSession(SessionId, bDeferResume);

	// 页签先接手会话，主页签再切走，会话不会被释放后又重新打开
	const TSharedPtr<FShuttlegentChatView> Hub = HubView.Pin();
	if (Hub.IsValid() && !SessionId.IsEmpty() && Hub->GetCurrentSessionId() == SessionId) Hub->NewConversation();

	Entry.Panel = SNew(SShuttlegentWebPanel).View(Entry.View);
	Entry.Panel->SetHostTabOpen(true);

	const TWeakPtr<FShuttlegentSessionTabRegistry> WeakThis = AsShared();
	const TSharedRef<SDockTab> Tab = SNew(SDockTab)
		// PanelTab 只能停靠到所属管理器（关卡编辑器）的区域：能与 Details、Outliner、视口叠放与分割，进不了全局管理器的大页签栏；
		// NomadTab 拖出后归全局管理器，引擎不允许其他 Nomad 页签用停靠十字分割它（FDockingDragOperation::CanDockInNode）
		.TabRole(ETabRole::PanelTab)
		.Label_Lambda([WeakThis, ViewId]()
		{
			const TSharedPtr<FShuttlegentSessionTabRegistry> This = WeakThis.Pin();
			return This.IsValid() ? This->GetTabLabel(ViewId) : FText::GetEmpty();
		})
		.TabColorScale_Lambda([WeakThis, ViewId]()
		{
			const TSharedPtr<FShuttlegentSessionTabRegistry> This = WeakThis.Pin();
			return This.IsValid() ? This->GetTabColor(ViewId) : FLinearColor::Transparent;
		})
		.OnTabClosed_Lambda([WeakThis, ViewId](TSharedRef<SDockTab> ClosedTab)
		{
			const TSharedPtr<FShuttlegentSessionTabRegistry> This = WeakThis.Pin();
			if (This.IsValid()) This->HandleTabClosed(ViewId, ClosedTab);
		})
		[
			Entry.Panel.ToSharedRef()
		];
	Tab->SetTabIcon(FSlateIcon(ShuttlegentEditorName::StyleSetName, ShuttlegentEditorStyleKey::TabIcon).GetIcon());
	Entry.Tab = Tab;
	Entry.Panel->SetHostTab(Tab);
	Tabs.Add(MoveTemp(Entry));
	return Tab;
}

TSharedPtr<SDockTab> FShuttlegentSessionTabRegistry::CreateTab(const FString& SessionId)
{
	// 关卡编辑器的页签管理器随关卡编辑器重建，每次开页签时现取
	FLevelEditorModule* LevelEditor = FModuleManager::GetModulePtr<FLevelEditorModule>(ShuttlegentEditorSessionTab::LevelEditorModule);
	const TSharedPtr<FTabManager> TabManager = LevelEditor != nullptr ? LevelEditor->GetLevelEditorTabManager() : nullptr;
	if (!TabManager.IsValid())
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Session tab: the level editor tab manager is not available; the session stays in the main tab."));
		return nullptr;
	}

	const FGuid ViewId = FGuid::NewGuid();
	const TSharedRef<SDockTab> Tab = BuildTab(ViewId, TabManager.ToSharedRef(), SessionId, false);

	// Spawner 交出已建好的页签；管理器里还没有这个页签时 TryInvokeTab 为它新开一个浮动窗口，大小取默认窗口尺寸
	const FName TabType = MakeTabType(ViewId);
	TabManager->RegisterTabSpawner(TabType, FOnSpawnTab::CreateLambda([Tab](const FSpawnTabArgs& Args) { return Tab; }))
		.SetIcon(FSlateIcon(ShuttlegentEditorName::StyleSetName, ShuttlegentEditorStyleKey::TabIcon))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
	FTabManager::RegisterDefaultTabWindowSize(FTabId(TabType), ShuttlegentEditorSessionTab::WindowSize);
	TabManager->TryInvokeTab(FTabId(TabType));

	// 新窗口只是拖拽的起点：藏起并放在光标之外，拖拽一开始页签就离开它，由跟随光标的预览窗口接手；拖拽没能开始时再显示（RevealWindow）
	const TSharedPtr<SWindow> Window = Tab->GetParentWindow();
	if (Window.IsValid())
	{
		Window->SetOpacity(0.0f);
		Window->MoveWindowTo(FSlateApplication::Get().GetCursorPos() + ShuttlegentEditorSessionTab::HiddenWindowOffset);
	}
	UE_LOG(LogShuttlegent, Log, TEXT("Session tab %s opened for session %s."), *TabType.ToString(), *SessionId);
	return Tab;
}

TSharedRef<SDockTab> FShuttlegentSessionTabRegistry::SpawnRestoredTab(const FGuid& ViewId)
{
	// 已接回的页签再次被请求时交出同一个
	const FShuttlegentSessionTab* Existing = FindByView(ViewId);
	const TSharedPtr<SDockTab> ExistingTab = Existing != nullptr ? Existing->Tab.Pin() : nullptr;
	if (ExistingTab.IsValid()) return ExistingTab.ToSharedRef();

	const int32 Index = PendingRestore.IndexOfByPredicate([&ViewId](const FShuttlegentSessionBinding& Pending) { return Pending.ViewId == ViewId; });
	const TSharedPtr<FTabManager> TabManager = PendingTabManager.Pin();
	if (Index == INDEX_NONE || !TabManager.IsValid() || !Model.IsValid()) return SNew(SDockTab);

	// 会话已删除、已归档，或已显示在另一个页签里时，页签显示空白新对话
	FString SessionId = PendingRestore[Index].SessionId;
	PendingRestore.RemoveAt(Index);
	if (!SessionId.IsEmpty() && (!Model->IsSessionAvailable(SessionId) || FindTab(SessionId).IsValid()))
	{
		UE_LOG(LogShuttlegent, Log, TEXT("Session tab %s: session %s is no longer available; the tab shows a new conversation."), *ViewId.ToString(EGuidFormats::Digits), *SessionId);
		SessionId.Reset();
	}

	UE_LOG(LogShuttlegent, Log, TEXT("Session tab %s restored for session %s; it resumes when the tab is first shown."), *ViewId.ToString(EGuidFormats::Digits), *SessionId);
	return BuildTab(ViewId, TabManager.ToSharedRef(), SessionId, true);
}

void FShuttlegentSessionTabRegistry::SaveBindings()
{
	if (!bRestored) return;

	TArray<FString> Values;
	for (const FShuttlegentSessionTab& Entry : Tabs)
	{
		const FString SessionId = Entry.View.IsValid() ? Entry.View->GetCurrentSessionId() : FString();
		Values.Add(Entry.ViewId.ToString(EGuidFormats::Digits) + ShuttlegentEditorSessionTab::BindingSeparator + SessionId);
	}
	const TSharedPtr<FShuttlegentChatView> Hub = HubView.Pin();
	const FString HubSessionId = Hub.IsValid() ? Hub->GetCurrentSessionId() : FString();

	const FString Snapshot = FString::Join(Values, TEXT("\n")) + TEXT("\n") + HubSessionId;
	if (Snapshot == SavedSnapshot) return;

	// 绑定变化不频繁（开关页签、发出第一条消息、分支、切换会话），即时写盘，编辑器意外退出也不丢
	SavedSnapshot = Snapshot;
	GConfig->SetArray(ShuttlegentEditorWeb::ConfigSection, ShuttlegentEditorSessionTab::ConfigKeySessionTabs, Values, GEditorPerProjectIni);
	GConfig->SetString(ShuttlegentEditorWeb::ConfigSection, ShuttlegentEditorSessionTab::ConfigKeyHubSession, *HubSessionId, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);
}

FText FShuttlegentSessionTabRegistry::GetTabLabel(const FGuid& ViewId) const
{
	const FShuttlegentSessionTab* Entry = FindByView(ViewId);
	const FString Title = Entry != nullptr && Entry->View.IsValid() ? Entry->View->GetCurrentTitle() : FString();
	return Title.IsEmpty() ? LOCTEXT("NewConversationLabel", "新对话") : FText::FromString(Title);
}

void FShuttlegentSessionTabRegistry::SyncWindowTitle(const FGuid& ViewId, const TSharedRef<SDockTab>& Tab) const
{
	// 引擎只在切换前台页签等少数时机把页签名写成浮动窗口标题，拖出的窗口常留空标题，任务栏上没有名字；主窗口标题归引擎
	const TSharedPtr<SWindow> Window = Tab->GetParentWindow();
	if (!Window.IsValid() || Window == FGlobalTabmanager::Get()->GetRootWindow()) return;

	const FText Label = GetTabLabel(ViewId);
	if (!Window->GetTitle().EqualTo(Label)) Window->SetTitle(Label);
}

FLinearColor FShuttlegentSessionTabRegistry::GetTabColor(const FGuid& ViewId) const
{
	// 在前台时用户看得见这个会话，不画色条
	const FShuttlegentSessionTab* Entry = FindByView(ViewId);
	const TSharedPtr<SDockTab> Tab = Entry != nullptr ? Entry->Tab.Pin() : nullptr;
	if (!Tab.IsValid() || Tab->IsForeground() || !Entry->View.IsValid()) return FLinearColor::Transparent;

	return GetAttentionColor(Entry->View->GetTabAttention());
}

void FShuttlegentSessionTabRegistry::HandleTabClosed(const FGuid& ViewId, const TSharedRef<SDockTab>& ClosedTab)
{
	// 关卡编辑器重建后，同一 ViewId 可能已在新管理器里接回；旧页签迟到的关闭事件不能关掉新页签
	const int32 Index = Tabs.IndexOfByPredicate([&ViewId, &ClosedTab](const FShuttlegentSessionTab& Entry) { return Entry.ViewId == ViewId && Entry.Tab.Pin() == ClosedTab; });
	if (Index == INDEX_NONE) return;

	const FShuttlegentSessionTab Closed = Tabs[Index];
	Tabs.RemoveAt(Index);
	ReleaseTab(Closed);
	UE_LOG(LogShuttlegent, Log, TEXT("Session tab %s closed."), *ViewId.ToString(EGuidFormats::Digits));
}

#undef LOCTEXT_NAMESPACE
