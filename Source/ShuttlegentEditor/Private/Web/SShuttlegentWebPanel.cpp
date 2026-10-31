#include "Web/SShuttlegentWebPanel.h"

#include "ContentBrowserModule.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "DragAndDrop/ActorDragDropOp.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/CompositeDragDropOp.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "IContentBrowserSingleton.h"
#include "ISettingsModule.h"
#include "IWebBrowserWindow.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "SWebBrowser.h"
#include "Serialization/JsonWriter.h"
#include "Styling/AppStyle.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

#include "Chat/ShuttlegentChatModel.h"
#include "Reference/FShuttlegentReferenceFinder.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentStyle.h"
#include "Web/FShuttlegentPanelCommands.h"
#include "Web/FShuttlegentWebJson.h"
#include "Web/SShuttlegentDrawerFrame.h"
#include "Web/FShuttlegentWebProtocol.h"
#include "Web/ShuttlegentWebBridge.h"

#define LOCTEXT_NAMESPACE "SShuttlegentWebPanel"

namespace SShuttlegentWebPanelPrivate
{
	// 以占位颜色填充的编辑器画刷，及其说明文字的字体
	inline const FName WhiteBrush = TEXT("WhiteBrush");
	inline const TCHAR* TypefaceRegular = TEXT("Regular");
}

SShuttlegentWebPanel::SShuttlegentWebPanel() = default;

void SShuttlegentWebPanel::Construct(const FArguments& InArgs)
{
	Model = InArgs._Model;
	check(Model.IsValid());

	ChildSlot
	[
		SNew(SOverlay)
		.Clipping(EWidgetClipping::ClipToBounds)

		+ SOverlay::Slot()
		[
			SAssignNew(BrowserSlot, SBox)
		]

		// 在页面报告就绪前覆盖页面：首次加载与编辑器启动重叠，可能耗时数秒乃至更久
		+ SOverlay::Slot()
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return bPageReady ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				BuildPlaceholder()
			]
		]

		// Content Browser 抽屉，与编辑器状态栏抽屉一样从页面底部滑出覆盖
		+ SOverlay::Slot()
		.VAlign(VAlign_Bottom)
		[
			SAssignNew(DrawerSlot, SBox)
			.HeightOverride(this, &SShuttlegentWebPanel::GetDrawerHeight)
			.Visibility_Lambda([this]() { return bDrawerShown ? EVisibility::Visible : EVisibility::Collapsed; })
			.RenderTransform(this, &SShuttlegentWebPanel::GetDrawerTransform)
		]
	];

	DrawerSlide = FCurveSequence(0.0f, ShuttlegentEditorWeb::DrawerSlideSeconds, ECurveEaseFunction::QuadOut);
	DrawerHeightShare = ShuttlegentEditorWeb::DrawerHeightShare;
	GConfig->GetFloat(ShuttlegentEditorWeb::ConfigSection, ShuttlegentEditorWeb::ConfigKeyDrawerHeightShare, DrawerHeightShare, GEditorPerProjectIni);

	Protocol = MakeShared<FShuttlegentWebProtocol>();
	const TWeakPtr<SShuttlegentWebPanel> WeakThis = SharedThis(this);
	Protocol->Initialize(Model.ToSharedRef(), [WeakThis](const FString& Json)
	{
		const TSharedPtr<SShuttlegentWebPanel> This = WeakThis.Pin();
		if (This.IsValid()) This->CallPage(ShuttlegentEditorWeb::JsApply, Json);
	});

	ReferenceFinder = MakeShared<FShuttlegentReferenceFinder>();
	ReferenceFinder->Initialize();
	ReferenceFinder->OnReferencesChanged.AddSP(this, &SShuttlegentWebPanel::PushReferenceValidity);

	// 活动计时器仅在控件被绘制后才运行，因此恢复在其他标签页之后的面板不会创建浏览器
	RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SShuttlegentWebPanel::HandleCreateBrowser));
	Model->EnsureRuntime();
}

SShuttlegentWebPanel::~SShuttlegentWebPanel()
{
	if (DrawerFocusHandle.IsValid() && FSlateApplication::IsInitialized()) FSlateApplication::Get().OnFocusChanging().Remove(DrawerFocusHandle);
	if (DrawerSlideTimer.IsValid()) FSlateThrottleManager::Get().LeaveResponsiveMode(DrawerSlideThrottle);

	SetHostTabOpen(false);
	if (ReferenceFinder.IsValid())
	{
		ReferenceFinder->OnReferencesChanged.RemoveAll(this);
		ReferenceFinder->Shutdown();
	}
	ReferenceFinder.Reset();
	if (Protocol.IsValid()) Protocol->Shutdown();
	Protocol.Reset();
	if (!Bridge.IsValid()) return;

	if (Browser.IsValid()) Browser->UnbindUObject(ShuttlegentEditorWeb::BridgeName, Bridge.Get());
	Bridge->OnReady = nullptr;
	Bridge->OnSubmit = nullptr;
	Bridge->OnCancel = nullptr;
	Bridge->OnNewConversation = nullptr;
	Bridge->OnReconnect = nullptr;
	Bridge->OnSetConfig = nullptr;
	Bridge->OnCopyText = nullptr;
	Bridge->OnToggleContentBrowser = nullptr;
	Bridge->OnOpenSession = nullptr;
	Bridge->OnRenameSession = nullptr;
	Bridge->OnArchiveSession = nullptr;
	Bridge->OnRestoreSession = nullptr;
	Bridge->OnDeleteSession = nullptr;
	Bridge->OnSetPaneWidths = nullptr;
	Bridge->OnAnswerPermission = nullptr;
	Bridge->OnOpenSettings = nullptr;
	Bridge->OnQueryReferences = nullptr;
	Bridge->OnWatchReferences = nullptr;
	Bridge->OnRemoveQueued = nullptr;
	Bridge->OnTakeQueued = nullptr;
	Bridge->OnResumeQueue = nullptr;
	Bridge->OnRetry = nullptr;
	Bridge->OnAnswerWrite = nullptr;
	Bridge->OnAnswerQuestion = nullptr;
	Bridge->OnUndoWrite = nullptr;
	Bridge->OnRewindTo = nullptr;
	Bridge->OnBranchFrom = nullptr;
	Bridge->OnRetryStart = nullptr;
	Bridge->OnRetryResume = nullptr;
	Bridge->OnAcknowledgeWrite = nullptr;
	Bridge->OnOpenPluginFolder = nullptr;
	Bridge.Reset();
}

void SShuttlegentWebPanel::SetHostTabOpen(bool bOpen)
{
	if (bHostTabOpen == bOpen) return;

	// 没有标签页显示面板时，请求回退到 UShuttlegentSettings::PermissionPolicy 处理；待处理的请求被拒绝
	bHostTabOpen = bOpen;
	if (Model.IsValid()) Model->SetPermissionHandlerAttached(bOpen);
}

FReply SShuttlegentWebPanel::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// CEF 会消耗其收到的每个按键，因此面板自身的组合键必须在此处、先于页面截获
	if (InKeyEvent.GetKey() == EKeys::Escape && bDrawerOpen)
	{
		CloseDrawer(TEXT("Esc"));
		return FReply::Handled();
	}

	// 页面就绪前同样生效，因此加载失败的页面也可以重新加载
	if (IsReloadPageChord(InKeyEvent))
	{
		ReloadPage();
		return FReply::Handled();
	}

	// 无论焦点在页面还是抽屉，面板快捷键都以命令形式发送给页面
	const TCHAR* Command = FindPanelCommand(InKeyEvent);
	if (Command != nullptr && bPageReady)
	{
		// 将光标置于页面中的命令需要浏览器拥有键盘焦点（焦点可能位于抽屉中）
		const bool bFocusesPage = Command == ShuttlegentEditorWeb::CommandSearchSessions || Command == ShuttlegentEditorWeb::CommandFocusInput || Command == ShuttlegentEditorWeb::CommandNewSession;
		if (bFocusesPage && Browser.IsValid()) FSlateApplication::Get().SetKeyboardFocus(Browser, EFocusCause::SetDirectly);

		CallPage(ShuttlegentEditorWeb::JsRunCommand, FString::Printf(TEXT("\"%s\""), Command));
		return FReply::Handled();
	}

	const bool bCtrlSpace = InKeyEvent.GetKey() == EKeys::SpaceBar && InKeyEvent.IsControlDown() && !InKeyEvent.IsAltDown() && !InKeyEvent.IsShiftDown();
	if (!bCtrlSpace) return SCompoundWidget::OnPreviewKeyDown(MyGeometry, InKeyEvent);

	// 在任何宿主窗口中，Ctrl+Space 都打开面板自有的 Content Browser 抽屉；不使用状态栏抽屉
	ToggleDrawer();
	return FReply::Handled();
}

FReply SShuttlegentWebPanel::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// 在抽屉以外的页面区域点击会关闭抽屉；该点击仍会传递到页面
	if (!bDrawerOpen || !DrawerSlot.IsValid()) return FReply::Unhandled();

	// 以光标下实际的控件路径为准，而非缓存的几何信息（抽屉通过渲染变换滑动）
	const FWidgetPath PathUnderCursor = FSlateApplication::Get().LocateWindowUnderMouse(MouseEvent.GetScreenSpacePosition(), FSlateApplication::Get().GetInteractiveTopLevelWindows());
	if (PathUnderCursor.ContainsWidget(DrawerSlot.Get())) return FReply::Unhandled();

	LastClickCloseSeconds = FPlatformTime::Seconds();
	CloseDrawer(TEXT("click outside the drawer"));
	return FReply::Unhandled();
}

void SShuttlegentWebPanel::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	if (!GetDraggedItems(DragDropEvent).IsEmpty()) CallPage(ShuttlegentEditorWeb::JsOnDragState, ShuttlegentEditorWeb::JsTrue);
}

void SShuttlegentWebPanel::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	CallPage(ShuttlegentEditorWeb::JsOnDragState, ShuttlegentEditorWeb::JsFalse);
}

FReply SShuttlegentWebPanel::OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	return GetDraggedItems(DragDropEvent).IsEmpty() ? FReply::Unhandled() : FReply::Handled();
}

FReply SShuttlegentWebPanel::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const FString Items = GetDraggedItems(DragDropEvent);
	if (Items.IsEmpty()) return FReply::Unhandled();

	CallPage(ShuttlegentEditorWeb::JsOnDragState, ShuttlegentEditorWeb::JsFalse);
	CallPage(ShuttlegentEditorWeb::JsOnDrop, Items);
	UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent panel: drop received %s"), *Items);
	if (bDrawerOpen) CloseDrawer(TEXT("asset dropped on the page"));
	return FReply::Handled();
}

TSharedRef<SWidget> SShuttlegentWebPanel::BuildPlaceholder() const
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::Get().GetBrush(SShuttlegentWebPanelPrivate::WhiteBrush))
		.BorderBackgroundColor(FLinearColor(ShuttlegentEditorColor::Background))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(ShuttlegentEditorMetric::MarkSizePlaceholder)
				.HeightOverride(ShuttlegentEditorMetric::MarkSizePlaceholder)
				[
					SNew(SImage)
					.Image(FShuttlegentStyle::Brush(ShuttlegentEditorStyleKey::Mark))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Center)
			.Padding(0.0f, ShuttlegentEditorMetric::PlaceholderSpacing, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Loading", "正在加载…"))
				.Font(FCoreStyle::GetDefaultFontStyle(SShuttlegentWebPanelPrivate::TypefaceRegular, ShuttlegentEditorMetric::FontCaption))
				.ColorAndOpacity(FSlateColor(FLinearColor(ShuttlegentEditorColor::Muted)))
			]
		];
}

EActiveTimerReturnType SShuttlegentWebPanel::HandleCreateBrowser(double InCurrentTime, float InDeltaTime)
{
	// SWebBrowserView 只检查模块是否已加载；缺少这一步，视图会一直处于加载转圈状态
	FModuleManager::Get().LoadModuleChecked(ShuttlegentEditorWeb::WebBrowserModule);

	Bridge.Reset(NewObject<UShuttlegentWebBridge>(GetTransientPackage()));
	BindBridge();

	BrowserSlot->SetContent(
		SAssignNew(Browser, SWebBrowser)
		.InitialURL(GetPageUrl())
		.ShowControls(false)
		.ShowAddressBar(false)
		.ShowInitialThrobber(false)
		.BackgroundColor(ShuttlegentEditorColor::Background)
		.BrowserFrameRate(ShuttlegentEditorWeb::BrowserFrameRate)
		.OnBeforeNavigation(this, &SShuttlegentWebPanel::HandleBeforeNavigation)
		.OnBeforePopup(this, &SShuttlegentWebPanel::HandleBeforePopup)
		.OnConsoleMessage(this, &SShuttlegentWebPanel::HandleConsoleMessage)
		.OnSuppressContextMenu_Lambda([]() { return true; }));

	// 浏览器窗口在渲染线程上异步创建其 1x1 共享视图纹理（CEFWebBrowserWindow.cpp:523）。
	// 其首次调整尺寸发生在同一帧内，需要该纹理已存在；否则引擎会释放它且不再重建，
	// 页面将保持空白并报 "EnsureShareable() - Invalid SlateTexture" 错误（CEFWebBrowserWindowRHIHelper.cpp:242、:186）
	FlushRenderingCommands();

	Browser->BindUObject(ShuttlegentEditorWeb::BridgeName, Bridge.Get());
	UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent panel: browser created, loading %s"), *GetPageUrl());
	return EActiveTimerReturnType::Stop;
}

void SShuttlegentWebPanel::BindBridge()
{
	const TWeakPtr<SShuttlegentWebPanel> WeakThis = SharedThis(this);
	const TWeakPtr<FShuttlegentChatModel> WeakModel = Model;

	Bridge->OnReady = [WeakThis]()
	{
		const TSharedPtr<SShuttlegentWebPanel> This = WeakThis.Pin();
		if (This.IsValid()) This->HandlePageReady();
	};
	Bridge->OnSubmit = [WeakThis](const FString& Text, const FString& ReferencesJson)
	{
		const TSharedPtr<SShuttlegentWebPanel> This = WeakThis.Pin();
		if (This.IsValid()) This->HandleSubmit(Text, ReferencesJson);
	};
	Bridge->OnRemoveQueued = [WeakModel](int32 ItemId)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->RemoveQueued(ItemId);
	};
	Bridge->OnTakeQueued = [WeakThis](int32 ItemId, const FString& DraftText, const FString& DraftReferencesJson)
	{
		const TSharedPtr<SShuttlegentWebPanel> This = WeakThis.Pin();
		if (This.IsValid()) This->HandleTakeQueued(ItemId, DraftText, DraftReferencesJson);
	};
	Bridge->OnResumeQueue = [WeakModel]()
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->ResumeQueue();
	};
	Bridge->OnRetry = [WeakModel]()
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->Retry();
	};
	Bridge->OnAnswerWrite = [WeakModel](int32 RequestId, bool bApprove)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->AnswerWrite(RequestId, bApprove);
	};
	Bridge->OnAnswerQuestion = [WeakModel](int32 QuestionId, const FString& Answer, bool bSkip)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->AnswerQuestion(QuestionId, Answer, bSkip);
	};
	Bridge->OnUndoWrite = [WeakModel](int32 EntryId)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->UndoWrite(EntryId);
	};
	Bridge->OnRewindTo = [WeakModel](int32 EntryId)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->RewindTo(EntryId);
	};
	Bridge->OnBranchFrom = [WeakModel](int32 EntryId)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->BranchFrom(EntryId);
	};
	Bridge->OnRetryStart = [WeakModel]()
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->RetryStart();
	};
	Bridge->OnRetryResume = [WeakModel]()
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->RetryResume();
	};
	Bridge->OnAcknowledgeWrite = [WeakModel](int32 EntryId)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->AcknowledgeUnknownWrite(EntryId);
	};
	Bridge->OnOpenPluginFolder = []()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ShuttlegentPath::PluginName);
		if (Plugin.IsValid()) FPlatformProcess::ExploreFolder(*FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir()));
	};
	Bridge->OnQueryReferences = [WeakThis](const FString& Query)
	{
		const TSharedPtr<SShuttlegentWebPanel> This = WeakThis.Pin();
		if (This.IsValid()) This->HandleQueryReferences(Query);
	};
	Bridge->OnWatchReferences = [WeakThis](const FString& ReferencesJson)
	{
		const TSharedPtr<SShuttlegentWebPanel> This = WeakThis.Pin();
		if (This.IsValid()) This->HandleWatchReferences(ReferencesJson);
	};
	Bridge->OnCancel = [WeakModel]()
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->Cancel();
	};
	Bridge->OnNewConversation = [WeakModel]()
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->NewConversation();
	};
	Bridge->OnReconnect = [WeakModel]()
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->Reconnect();
	};
	Bridge->OnSetConfig = [WeakModel](const FString& ConfigId, const FString& Value)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->SetConfigValue(ConfigId, Value);
	};
	Bridge->OnCopyText = [](const FString& Text)
	{
		FPlatformApplicationMisc::ClipboardCopy(*Text);
	};
	Bridge->OnToggleContentBrowser = [WeakThis]()
	{
		const TSharedPtr<SShuttlegentWebPanel> This = WeakThis.Pin();
		if (This.IsValid()) This->HandlePageToggleDrawer();
	};
	Bridge->OnOpenSession = [WeakModel](const FString& SessionId)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid() && SessionId != Pinned->GetCurrentSessionId()) Pinned->OpenSession(SessionId);
	};
	Bridge->OnRenameSession = [WeakModel](const FString& SessionId, const FString& Title)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->RenameSession(SessionId, Title);
	};
	Bridge->OnArchiveSession = [WeakModel](const FString& SessionId)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->ArchiveSession(SessionId);
	};
	Bridge->OnRestoreSession = [WeakModel](const FString& SessionId)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->RestoreSession(SessionId);
	};
	Bridge->OnDeleteSession = [WeakModel](const FString& SessionId)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->DeleteSession(SessionId);
	};
	Bridge->OnSetPaneWidths = [](int32 SidebarWidth, int32 InspectorWidth)
	{
		GConfig->SetInt(ShuttlegentEditorWeb::ConfigSection, ShuttlegentEditorWeb::ConfigKeySidebarWidth, SidebarWidth, GEditorPerProjectIni);
		GConfig->SetInt(ShuttlegentEditorWeb::ConfigSection, ShuttlegentEditorWeb::ConfigKeyInspectorWidth, InspectorWidth, GEditorPerProjectIni);
	};
	Bridge->OnAnswerPermission = [WeakModel](int32 EntryId, bool bAllow)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->AnswerPermission(EntryId, bAllow);
	};
	Bridge->OnOpenSettings = []()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>(ShuttlegentEditorName::SettingsModule);
		if (SettingsModule != nullptr) SettingsModule->ShowViewer(ShuttlegentEditorName::SettingsContainer, ShuttlegentEditorName::SettingsCategory, ShuttlegentEditorName::SettingsSection);
	};
}

const TCHAR* SShuttlegentWebPanel::FindPanelCommand(const FKeyEvent& InKeyEvent) const
{
	if (!FShuttlegentPanelCommands::IsRegistered()) return nullptr;

	const FInputChord Chord(InKeyEvent.GetKey(), EModifierKey::FromBools(InKeyEvent.IsControlDown(), InKeyEvent.IsAltDown(), InKeyEvent.IsShiftDown(), InKeyEvent.IsCommandDown()));
	const FShuttlegentPanelCommands& Commands = FShuttlegentPanelCommands::Get();
	if (Commands.ToggleSidebar.IsValid() && Commands.ToggleSidebar->HasActiveChord(Chord)) return ShuttlegentEditorWeb::CommandToggleSidebar;
	if (Commands.ToggleInspector.IsValid() && Commands.ToggleInspector->HasActiveChord(Chord)) return ShuttlegentEditorWeb::CommandToggleInspector;
	if (Commands.NewSession.IsValid() && Commands.NewSession->HasActiveChord(Chord)) return ShuttlegentEditorWeb::CommandNewSession;
	if (Commands.SearchSessions.IsValid() && Commands.SearchSessions->HasActiveChord(Chord)) return ShuttlegentEditorWeb::CommandSearchSessions;
	if (Commands.FocusInput.IsValid() && Commands.FocusInput->HasActiveChord(Chord)) return ShuttlegentEditorWeb::CommandFocusInput;
	return nullptr;
}

bool SShuttlegentWebPanel::IsReloadPageChord(const FKeyEvent& InKeyEvent) const
{
	if (!FShuttlegentPanelCommands::IsRegistered()) return false;

	const FInputChord Chord(InKeyEvent.GetKey(), EModifierKey::FromBools(InKeyEvent.IsControlDown(), InKeyEvent.IsAltDown(), InKeyEvent.IsShiftDown(), InKeyEvent.IsCommandDown()));
	const TSharedPtr<FUICommandInfo>& Command = FShuttlegentPanelCommands::Get().ReloadPage;
	return Command.IsValid() && Command->HasActiveChord(Chord);
}

void SShuttlegentWebPanel::ReloadPage()
{
	if (!Browser.IsValid()) return;

	UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent panel: reloading the page"));
	bPageReady = false;
	Browser->Reload();
}

FString SShuttlegentWebPanel::GetPageUrl() const
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ShuttlegentPath::PluginName);
	const FString BaseDir = Plugin.IsValid() ? Plugin->GetBaseDir() : FString();
	const FString PagePath = FPaths::ConvertRelativePathToFull(FPaths::Combine(BaseDir, ShuttlegentEditorResource::ResourcesDir, ShuttlegentEditorWeb::PageDir, ShuttlegentEditorWeb::PageFile));
	return ShuttlegentEditorWeb::FileUrlPrefix + PagePath;
}

FString SShuttlegentWebPanel::GetDraggedItems(const FDragDropEvent& DragDropEvent) const
{
	TArray<TSharedPtr<FJsonValue>> Items;

	const TSharedPtr<FAssetDragDropOp> AssetOp = DragDropEvent.GetOperationAs<FAssetDragDropOp>();
	if (AssetOp.IsValid())
	{
		for (const FAssetData& Asset : AssetOp->GetAssets())
		{
			Items.Add(FShuttlegentWebJson::MakeReference(FShuttlegentReferenceFinder::FromAsset(Asset)));
		}
	}

	// 来自 Outliner 的拖拽以携带 Actor 操作的复合操作形式到达
	TSharedPtr<FActorDragDropOp> ActorOp = DragDropEvent.GetOperationAs<FActorDragDropOp>();
	const TSharedPtr<FCompositeDragDropOp> CompositeOp = DragDropEvent.GetOperationAs<FCompositeDragDropOp>();
	if (!ActorOp.IsValid() && CompositeOp.IsValid()) ActorOp = CompositeOp->GetSubOp<FActorDragDropOp>();
	if (!AssetOp.IsValid() && ActorOp.IsValid())
	{
		for (const TWeakObjectPtr<AActor>& Actor : ActorOp->Actors)
		{
			if (Actor.IsValid()) Items.Add(FShuttlegentWebJson::MakeReference(FShuttlegentReferenceFinder::FromActor(*Actor)));
		}
	}

	return Items.IsEmpty() ? FString() : FShuttlegentWebJson::ToJson(Items);
}

void SShuttlegentWebPanel::HandleSubmit(const FString& Text, const FString& ReferencesJson)
{
	if (!Model.IsValid()) return;

	// 已不存在的引用会阻止发送；页面将其标记出来并保留草稿
	TArray<FString> InvalidPaths;
	const EShuttlegentSubmitResult Outcome = Model->SubmitDraft(Text, FShuttlegentWebJson::ParseReferences(ReferencesJson), InvalidPaths);

	TArray<TSharedPtr<FJsonValue>> Invalid;
	for (const FString& Path : InvalidPaths)
	{
		Invalid.Add(MakeShared<FJsonValueString>(Path));
	}

	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetBoolField(ShuttlegentEditorWebField::Accepted, Outcome != EShuttlegentSubmitResult::Rejected);
	Result->SetBoolField(ShuttlegentEditorWebField::Queued, Outcome == EShuttlegentSubmitResult::Queued);
	Result->SetArrayField(ShuttlegentEditorWebField::Invalid, Invalid);
	CallPage(ShuttlegentEditorWeb::JsOnSubmitResult, FShuttlegentWebJson::ToJson(Result));
}

void SShuttlegentWebPanel::HandleTakeQueued(int32 ItemId, const FString& DraftText, const FString& DraftReferencesJson)
{
	FShuttlegentQueuedPrompt Item;
	if (!Model.IsValid() || !Model->TakeQueued(ItemId, DraftText, FShuttlegentWebJson::ParseReferences(DraftReferencesJson), Item)) return;

	const TSharedRef<FJsonObject> Taken = MakeShared<FJsonObject>();
	Taken->SetStringField(ShuttlegentEditorWebField::Text, Item.Text);
	Taken->SetArrayField(ShuttlegentEditorWebField::References, FShuttlegentWebJson::MakeReferences(Item.References));
	CallPage(ShuttlegentEditorWeb::JsOnQueueItemTaken, FShuttlegentWebJson::ToJson(Taken));
}

void SShuttlegentWebPanel::HandleQueryReferences(const FString& Query)
{
	if (!ReferenceFinder.IsValid()) return;

	TArray<FShuttlegentReference> Actors;
	TArray<FShuttlegentReference> Assets;
	ReferenceFinder->Search(Query, Actors, Assets);

	// 仅在用户缩小菜单范围之前提供选中项快照
	const TArray<FShuttlegentReference> Selection = Query.TrimStartAndEnd().IsEmpty() ? ReferenceFinder->GetSelection() : TArray<FShuttlegentReference>();

	const TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(ShuttlegentEditorWebField::Query, Query);
	Result->SetArrayField(ShuttlegentEditorWebField::Actors, FShuttlegentWebJson::MakeReferences(Actors));
	Result->SetArrayField(ShuttlegentEditorWebField::Assets, FShuttlegentWebJson::MakeReferences(Assets));
	Result->SetArrayField(ShuttlegentEditorWebField::Selection, FShuttlegentWebJson::MakeReferences(Selection));
	CallPage(ShuttlegentEditorWeb::JsOnReferenceResults, FShuttlegentWebJson::ToJson(Result));
}

void SShuttlegentWebPanel::HandleWatchReferences(const FString& ReferencesJson)
{
	WatchedReferences = FShuttlegentWebJson::ParseReferences(ReferencesJson);
	PushReferenceValidity();
}

void SShuttlegentWebPanel::PushReferenceValidity()
{
	const TSharedRef<FJsonObject> Validity = MakeShared<FJsonObject>();
	for (const FShuttlegentReference& Reference : WatchedReferences)
	{
		Validity->SetBoolField(Reference.Path, FShuttlegentReferenceFinder::Exists(Reference));
	}
	CallPage(ShuttlegentEditorWeb::JsOnReferenceValidity, FShuttlegentWebJson::ToJson(Validity));
}

void SShuttlegentWebPanel::ExecuteJavascript(const FString& Script) const
{
	if (Browser.IsValid()) Browser->ExecuteJavascript(Script);
}

void SShuttlegentWebPanel::CallPage(const TCHAR* Entry, const FString& ArgumentLiteral) const
{
	ExecuteJavascript(FString(ShuttlegentEditorWeb::JsCallPrefix) + Entry + ShuttlegentEditorWeb::JsCallOpen + ArgumentLiteral + ShuttlegentEditorWeb::JsCallSuffix);
}

void SShuttlegentWebPanel::HandlePageReady()
{
	// 页面在每次重新加载后都会再次请求，因此快照从不依赖 OnLoadCompleted（它在导航被取消后也会触发）
	UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent panel: page ready%s"), bPageReady ? TEXT(" again") : TEXT(""));
	bPageReady = true;

	if (Protocol.IsValid()) Protocol->HandlePageReady();
	CallPage(ShuttlegentEditorWeb::JsOnDrawerState, bDrawerOpen ? ShuttlegentEditorWeb::JsTrue : ShuttlegentEditorWeb::JsFalse);
}

bool SShuttlegentWebPanel::HandleBeforeNavigation(const FString& Url, const FWebNavigationRequest& Request)
{
	const bool bExternal = Url.StartsWith(ShuttlegentEditorWeb::HttpPrefix) || Url.StartsWith(ShuttlegentEditorWeb::HttpsPrefix);
	if (!bExternal) return false;

	FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
	UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent panel: external link opened in the system browser: %s"), *Url);
	return true;
}

bool SShuttlegentWebPanel::HandleBeforePopup(FString Url, FString Frame)
{
	const bool bExternal = Url.StartsWith(ShuttlegentEditorWeb::HttpPrefix) || Url.StartsWith(ShuttlegentEditorWeb::HttpsPrefix);
	if (bExternal) FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);

	// 面板从不打开第二个浏览器窗口
	return true;
}

void SShuttlegentWebPanel::HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line, EWebBrowserConsoleLogSeverity Severity)
{
	const bool bProblem = Severity == EWebBrowserConsoleLogSeverity::Error || Severity == EWebBrowserConsoleLogSeverity::Fatal || Severity == EWebBrowserConsoleLogSeverity::Warning;
	if (bProblem)
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Shuttlegent page: %s (%s:%d)"), *Message, *FPaths::GetCleanFilename(Source), Line);
	}
	else
	{
		UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent page: %s (%s:%d)"), *Message, *FPaths::GetCleanFilename(Source), Line);
	}
}

void SShuttlegentWebPanel::ToggleDrawer()
{
	if (bDrawerOpen)
	{
		CloseDrawer(TEXT("toggled"));
		return;
	}

	OpenDrawer();
}

void SShuttlegentWebPanel::HandlePageToggleDrawer()
{
	if (!bDrawerOpen && FPlatformTime::Seconds() - LastClickCloseSeconds < ShuttlegentEditorWeb::ClickCloseGuardSeconds) return;

	ToggleDrawer();
}

void SShuttlegentWebPanel::OpenDrawer()
{
	if (!ContentBrowser.IsValid())
	{
		// 不作为主浏览器：SyncBrowserTo 与 "Browse to asset" 仍指向编辑器自身的 Content Browser
		FContentBrowserConfig Config;
		Config.bCanSetAsPrimaryBrowser = false;

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(ShuttlegentEditorWeb::ContentBrowserModule);
		ContentBrowser = ContentBrowserModule.Get().CreateContentBrowser(ShuttlegentEditorWeb::ContentBrowserInstance, nullptr, &Config);
		DrawerSlot->SetContent(
			SNew(SShuttlegentDrawerFrame)
			.OnDragLeft(this, &SShuttlegentWebPanel::HandleDrawerDragLeft)
			[
				SNew(SVerticalBox)

				// 顶边的调整尺寸手柄，拖动方式与编辑器抽屉的边缘相同
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(DrawerGrip, SBorder)
					.BorderImage(FAppStyle::Get().GetBrush(ShuttlegentEditorWeb::DrawerGripBrush))
					.Padding(0.0f)
					.Cursor(EMouseCursor::ResizeUpDown)
					.OnMouseButtonDown(this, &SShuttlegentWebPanel::HandleGripMouseDown)
					.OnMouseMove(this, &SShuttlegentWebPanel::HandleGripMouseMove)
					.OnMouseButtonUp(this, &SShuttlegentWebPanel::HandleGripMouseUp)
					[
						SNew(SBox)
						.HeightOverride(ShuttlegentEditorWeb::DrawerGripHeight)
					]
				]

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					SNew(SBorder)
					.BorderImage(FAppStyle::Get().GetBrush(ShuttlegentEditorWeb::DrawerBodyBrush))
					.Padding(0.0f)
					[
						ContentBrowser.ToSharedRef()
					]
				]
			]);
	}

	// 键盘焦点保持原位；仅当用户点击进入抽屉时抽屉才获得焦点
	bDrawerOpen = true;
	bDrawerShown = true;
	DrawerSlide.Play(AsShared(), false, DrawerSlide.IsPlaying() ? DrawerSlide.GetSequenceTime() : 0.0f, false);
	PlayDrawerSlide();
	if (!DrawerFocusHandle.IsValid()) DrawerFocusHandle = FSlateApplication::Get().OnFocusChanging().AddSP(this, &SShuttlegentWebPanel::HandleDrawerFocusChanging);
	CallPage(ShuttlegentEditorWeb::JsOnDrawerState, ShuttlegentEditorWeb::JsTrue);
}

void SShuttlegentWebPanel::CloseDrawer(const TCHAR* Reason)
{
	if (!bDrawerOpen) return;

	const bool bFocusInDrawer = DrawerSlot.IsValid() && DrawerSlot->HasFocusedDescendants();
	bDrawerOpen = false;
	bResizingDrawer = false;
	if (DrawerFocusHandle.IsValid())
	{
		FSlateApplication::Get().OnFocusChanging().Remove(DrawerFocusHandle);
		DrawerFocusHandle.Reset();
	}

	if (DrawerSlide.IsForward()) DrawerSlide.Reverse();
	PlayDrawerSlide();
	CallPage(ShuttlegentEditorWeb::JsOnDrawerState, ShuttlegentEditorWeb::JsFalse);

	// 原本位于抽屉内的焦点会随抽屉一同丢失；将其交还给页面的草稿输入框
	if (bFocusInDrawer && Browser.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(Browser, EFocusCause::SetDirectly);
		CallPage(ShuttlegentEditorWeb::JsFocusDraft, ShuttlegentEditorWeb::JsNoArgument);
	}
	UE_LOG(LogShuttlegent, Verbose, TEXT("Shuttlegent panel: content browser drawer closing: %s"), Reason);
}

void SShuttlegentWebPanel::HandleDrawerDragLeft()
{
	// 与编辑器的内容抽屉相同：离开抽屉的拖拽（资产正被拖向页面或视口）会将其收起
	if (bDrawerOpen && FSlateApplication::Get().IsDragDropping()) CloseDrawer(TEXT("drag left the drawer"));
}

void SShuttlegentWebPanel::PlayDrawerSlide()
{
	if (DrawerSlideTimer.IsValid()) return;

	DrawerSlideThrottle = FSlateThrottleManager::Get().EnterResponsiveMode();
	DrawerSlideTimer = RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SShuttlegentWebPanel::HandleDrawerSlideTick));
}

EActiveTimerReturnType SShuttlegentWebPanel::HandleDrawerSlideTick(double InCurrentTime, float InDeltaTime)
{
	if (DrawerSlide.IsPlaying()) return EActiveTimerReturnType::Continue;

	// 关闭滑动已回到起点：隐藏抽屉
	if (DrawerSlide.IsAtStart()) bDrawerShown = false;
	FSlateThrottleManager::Get().LeaveResponsiveMode(DrawerSlideThrottle);
	DrawerSlideTimer.Reset();
	return EActiveTimerReturnType::Stop;
}

TOptional<FSlateRenderTransform> SShuttlegentWebPanel::GetDrawerTransform() const
{
	// 底部对齐的抽屉按其尚未滑入的高度部分向下偏移，与 SDrawerOverlay 的做法相同
	const float Height = GetDrawerHeight().Get();
	return FSlateRenderTransform(FVector2f(0.0f, Height * (1.0f - DrawerSlide.GetLerp())));
}

void SShuttlegentWebPanel::HandleDrawerFocusChanging(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldPath, const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewPath, const TSharedPtr<SWidget>& NewWidget)
{
	if (!bDrawerOpen || NewPath.Widgets.Num() == 0 || NewPath.ContainsWidget(this)) return;

	// 点击面板中不接受焦点的元素（调整尺寸手柄、边框）会将焦点交给最近的可聚焦祖先，
	// 通常是承载面板的 SDockingTabStack：这仍属于本面板，不算离开
	FWidgetPath PanelPath;
	if (NewWidget.IsValid() && FSlateApplication::Get().GeneratePathToWidgetUnchecked(AsShared(), PanelPath, EVisibility::Visible) && PanelPath.ContainsWidget(NewWidget.Get())) return;

	// 从抽屉内部打开的菜单（例如 Content Browser 上下文菜单）会使抽屉保持打开，与编辑器抽屉的行为一致
	const TSharedPtr<SWidget> MenuHost = FSlateApplication::Get().GetMenuHostWidget();
	if (MenuHost.IsValid())
	{
		FWidgetPath MenuHostPath;
		FSlateApplication::Get().GeneratePathToWidgetUnchecked(MenuHost.ToSharedRef(), MenuHostPath, EVisibility::Visible);
		if (DrawerSlot.IsValid() && MenuHostPath.ContainsWidget(DrawerSlot.Get())) return;
	}

	CloseDrawer(TEXT("focus left the panel"));
}

FOptionalSize SShuttlegentWebPanel::GetDrawerHeight() const
{
	const float PanelHeight = GetTickSpaceGeometry().GetLocalSize().Y;
	const float MaxHeight = PanelHeight * ShuttlegentEditorWeb::DrawerMaxShare;
	return FMath::Min(FMath::Max(PanelHeight * DrawerHeightShare, ShuttlegentEditorWeb::DrawerMinHeight), FMath::Max(MaxHeight, ShuttlegentEditorWeb::DrawerMinHeight));
}

FReply SShuttlegentWebPanel::HandleGripMouseDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !DrawerGrip.IsValid()) return FReply::Unhandled();

	bResizingDrawer = true;
	return FReply::Handled().CaptureMouse(DrawerGrip.ToSharedRef());
}

FReply SShuttlegentWebPanel::HandleGripMouseMove(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (!bResizingDrawer) return FReply::Unhandled();

	// 抽屉底部对齐：其高度为光标到面板底边的距离
	const FGeometry PanelGeometry = GetTickSpaceGeometry();
	const float PanelHeight = PanelGeometry.GetLocalSize().Y;
	if (PanelHeight <= 0.0f) return FReply::Handled();

	const float CursorY = PanelGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition()).Y;
	const float MinShare = FMath::Min(ShuttlegentEditorWeb::DrawerMinHeight / PanelHeight, ShuttlegentEditorWeb::DrawerMaxShare);
	DrawerHeightShare = FMath::Clamp((PanelHeight - CursorY) / PanelHeight, MinShare, ShuttlegentEditorWeb::DrawerMaxShare);
	return FReply::Handled();
}

FReply SShuttlegentWebPanel::HandleGripMouseUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
	if (!bResizingDrawer || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();

	bResizingDrawer = false;
	GConfig->SetFloat(ShuttlegentEditorWeb::ConfigSection, ShuttlegentEditorWeb::ConfigKeyDrawerHeightShare, DrawerHeightShare, GEditorPerProjectIni);
	return FReply::Handled().ReleaseMouseCapture();
}

#undef LOCTEXT_NAMESPACE
