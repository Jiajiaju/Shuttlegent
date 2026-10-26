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
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "SWebBrowser.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
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
	// Editor brush filled with the placeholder colour, and the typeface of its caption
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

		// Covers the page until it reports ready: the first load overlaps editor startup and can take many seconds
		+ SOverlay::Slot()
		[
			SNew(SBox)
			.Visibility_Lambda([this]() { return bPageReady ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				BuildPlaceholder()
			]
		]

		// Content Browser drawer, slid over the bottom of the page like the editor's status bar drawer
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

	// Active timers only run once the widget is painted, so a panel restored behind another tab creates no browser
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
	Bridge->OnUndoWrite = nullptr;
	Bridge->OnRetryStart = nullptr;
	Bridge->OnRetryResume = nullptr;
	Bridge->OnAcknowledgeWrite = nullptr;
	Bridge->OnOpenPluginFolder = nullptr;
	Bridge.Reset();
}

void SShuttlegentWebPanel::SetHostTabOpen(bool bOpen)
{
	if (bHostTabOpen == bOpen) return;

	// While no tab shows the panel, requests fall back to UShuttlegentSettings::PermissionPolicy; pending ones are rejected
	bHostTabOpen = bOpen;
	if (Model.IsValid()) Model->SetPermissionHandlerAttached(bOpen);
}

FReply SShuttlegentWebPanel::OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	// CEF consumes every key it receives, so the panel's own chords must be taken here, before the page
	if (InKeyEvent.GetKey() == EKeys::Escape && bDrawerOpen)
	{
		CloseDrawer(TEXT("Esc"));
		return FReply::Handled();
	}

	// Works before the page is ready too, so a page that failed to load can be reloaded
	if (IsReloadPageChord(InKeyEvent))
	{
		ReloadPage();
		return FReply::Handled();
	}

	// Panel shortcuts go to the page as commands, whether the page or the drawer has focus
	const TCHAR* Command = FindPanelCommand(InKeyEvent);
	if (Command != nullptr && bPageReady)
	{
		// Commands that put the caret in the page need the browser to own keyboard focus (it may sit in the drawer)
		const bool bFocusesPage = Command == ShuttlegentEditorWeb::CommandSearchSessions || Command == ShuttlegentEditorWeb::CommandFocusInput || Command == ShuttlegentEditorWeb::CommandNewSession;
		if (bFocusesPage && Browser.IsValid()) FSlateApplication::Get().SetKeyboardFocus(Browser, EFocusCause::SetDirectly);

		CallPage(ShuttlegentEditorWeb::JsRunCommand, FString::Printf(TEXT("\"%s\""), Command));
		return FReply::Handled();
	}

	const bool bCtrlSpace = InKeyEvent.GetKey() == EKeys::SpaceBar && InKeyEvent.IsControlDown() && !InKeyEvent.IsAltDown() && !InKeyEvent.IsShiftDown();
	if (!bCtrlSpace) return SCompoundWidget::OnPreviewKeyDown(MyGeometry, InKeyEvent);

	// Ctrl+Space opens the panel's own Content Browser drawer in every host window; the status bar drawer is not used
	ToggleDrawer();
	return FReply::Handled();
}

FReply SShuttlegentWebPanel::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// A click on the page outside the drawer closes it; the click still reaches the page
	if (!bDrawerOpen || !DrawerSlot.IsValid()) return FReply::Unhandled();

	// The widget path actually under the cursor decides, not cached geometry (the drawer slides with a render transform)
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
	// SWebBrowserView only checks whether the module is loaded; without this the view spins forever
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

	// The browser window creates its 1x1 shared view texture asynchronously on the render thread (CEFWebBrowserWindow.cpp:523).
	// Its first resize, in this same frame, needs that texture to exist; otherwise the engine releases it and never recreates it,
	// and the page stays blank with "EnsureShareable() - Invalid SlateTexture" errors (CEFWebBrowserWindowRHIHelper.cpp:242, :186)
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
	Bridge->OnUndoWrite = [WeakModel](int32 EntryId)
	{
		const TSharedPtr<FShuttlegentChatModel> Pinned = WeakModel.Pin();
		if (Pinned.IsValid()) Pinned->UndoWrite(EntryId);
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

	// Outliner drags arrive as a composite operation that carries the actor operation
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

	// A reference that no longer exists blocks the send; the page marks it and keeps the draft
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

	// The selection snapshot is offered only before the user narrows the menu
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
	// The page asks again after any reload, so the snapshot never relies on OnLoadCompleted (it also fires after a cancelled navigation)
	UE_LOG(LogShuttlegent, Log, TEXT("Shuttlegent panel: page ready%s"), bPageReady ? TEXT(" again") : TEXT(""));
	bPageReady = true;

	const FString Manifest = LoadMarkManifest();
	if (!Manifest.IsEmpty()) CallPage(ShuttlegentEditorWeb::JsSetMarkSheets, Manifest);

	if (Protocol.IsValid()) Protocol->HandlePageReady();
	CallPage(ShuttlegentEditorWeb::JsOnDrawerState, bDrawerOpen ? ShuttlegentEditorWeb::JsTrue : ShuttlegentEditorWeb::JsFalse);
}

FString SShuttlegentWebPanel::LoadMarkManifest() const
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(ShuttlegentPath::PluginName);
	const FString BaseDir = Plugin.IsValid() ? Plugin->GetBaseDir() : FString();
	const FString ManifestPath = FPaths::Combine(BaseDir, ShuttlegentEditorResource::ResourcesDir, ShuttlegentEditorResource::AnimDir, ShuttlegentEditorResource::AnimPalette, ShuttlegentEditorResource::AnimManifest);

	FString ManifestText;
	if (!FFileHelper::LoadFileToString(ManifestText, *ManifestPath))
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Mark animation manifest not found at %s; the mark stays static."), *ManifestPath);
		return FString();
	}

	// Passed to the page as a JS object literal, so it must be valid JSON
	TSharedPtr<FJsonObject> Manifest;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ManifestText);
	if (!FJsonSerializer::Deserialize(Reader, Manifest) || !Manifest.IsValid())
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Mark animation manifest %s is not valid JSON; the mark stays static."), *ManifestPath);
		return FString();
	}
	return ManifestText;
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

	// The panel never opens a second browser window
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
		// Not the primary browser: SyncBrowserTo and "Browse to asset" keep going to the editor's own Content Browser
		FContentBrowserConfig Config;
		Config.bCanSetAsPrimaryBrowser = false;

		FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(ShuttlegentEditorWeb::ContentBrowserModule);
		ContentBrowser = ContentBrowserModule.Get().CreateContentBrowser(ShuttlegentEditorWeb::ContentBrowserInstance, nullptr, &Config);
		DrawerSlot->SetContent(
			SNew(SShuttlegentDrawerFrame)
			.OnDragLeft(this, &SShuttlegentWebPanel::HandleDrawerDragLeft)
			[
				SNew(SVerticalBox)

				// Resize grip on the top edge, dragged like the editor drawer's edge
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

	// Keyboard focus stays where it was; the drawer only takes focus when the user clicks into it
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

	// Focus that was inside the drawer would be lost with it; give it back to the page's draft
	if (bFocusInDrawer && Browser.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(Browser, EFocusCause::SetDirectly);
		CallPage(ShuttlegentEditorWeb::JsFocusDraft, ShuttlegentEditorWeb::JsNoArgument);
	}
	UE_LOG(LogShuttlegent, Verbose, TEXT("Shuttlegent panel: content browser drawer closing: %s"), Reason);
}

void SShuttlegentWebPanel::HandleDrawerDragLeft()
{
	// Like the editor's content drawer: a drag that leaves the drawer (an asset on its way to the page or the viewport) dismisses it
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

	// The closing slide reached the start: hide the drawer
	if (DrawerSlide.IsAtStart()) bDrawerShown = false;
	FSlateThrottleManager::Get().LeaveResponsiveMode(DrawerSlideThrottle);
	DrawerSlideTimer.Reset();
	return EActiveTimerReturnType::Stop;
}

TOptional<FSlateRenderTransform> SShuttlegentWebPanel::GetDrawerTransform() const
{
	// Bottom-aligned drawer pushed down by the part of its height not yet slid in, as SDrawerOverlay does
	const float Height = GetDrawerHeight().Get();
	return FSlateRenderTransform(FVector2f(0.0f, Height * (1.0f - DrawerSlide.GetLerp())));
}

void SShuttlegentWebPanel::HandleDrawerFocusChanging(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldPath, const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewPath, const TSharedPtr<SWidget>& NewWidget)
{
	if (!bDrawerOpen || NewPath.Widgets.Num() == 0 || NewPath.ContainsWidget(this)) return;

	// Clicking something in the panel that takes no focus (the resize grip, a border) hands focus to the nearest
	// focusable ancestor, typically the hosting SDockingTabStack: that is still this panel, not leaving it
	FWidgetPath PanelPath;
	if (NewWidget.IsValid() && FSlateApplication::Get().GeneratePathToWidgetUnchecked(AsShared(), PanelPath, EVisibility::Visible) && PanelPath.ContainsWidget(NewWidget.Get())) return;

	// A menu opened from inside the drawer (e.g. a Content Browser context menu) keeps it open, as the editor drawer does
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

	// The drawer is bottom-aligned: its height is the distance from the cursor to the panel's bottom edge
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
