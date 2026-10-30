#pragma once

#include "CoreMinimal.h"
#include "Animation/CurveSequence.h"
#include "Application/ThrottleManager.h"
#include "UObject/StrongObjectPtr.h"

#include "Reference/FShuttlegentReference.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FActiveTimerHandle;
class FShuttlegentChatModel;
class FShuttlegentReferenceFinder;
class FShuttlegentWebProtocol;
class SBorder;
class SBox;
class SWebBrowser;
class UShuttlegentWebBridge;
struct FWebNavigationRequest;
enum class EWebBrowserConsoleLogSeverity;

/**
 * The Shuttlegent panel: hosts Resources/Web/index.html in CEF and connects it to FShuttlegentChatModel.
 *
 * The browser is created the first time the panel is painted, with a placeholder until the page reports
 * ready. Around the page the host takes what CEF cannot: editor drag and drop, Ctrl+Space and Esc, the
 * panel's own Content Browser drawer, and http(s) links (opened in the system browser). The module keeps
 * one instance and puts it into every spawned tab, so closing and reopening the tab keeps the page.
 */
class SShuttlegentWebPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShuttlegentWebPanel) {}
		SLATE_ARGUMENT(TSharedPtr<FShuttlegentChatModel>, Model)
	SLATE_END_ARGS()

private:
	TSharedPtr<FShuttlegentChatModel> Model;
	TSharedPtr<FShuttlegentWebProtocol> Protocol;
	TSharedPtr<SWebBrowser> Browser;
	TStrongObjectPtr<UShuttlegentWebBridge> Bridge;
	TSharedPtr<SBox> BrowserSlot;

	/** Draft references: candidates for the @ menu, and the chips whose existence the page is told about. */
	TSharedPtr<FShuttlegentReferenceFinder> ReferenceFinder;
	TArray<FShuttlegentReference> WatchedReferences;

	/** The page called ready at least once; until then the placeholder covers it. */
	bool bPageReady = false;

	/** A dock tab shows the panel; only then does the panel answer runtime permission requests. */
	bool bHostTabOpen = false;

	/** Content Browser drawer: a real SContentBrowser created on first use and kept for the panel's lifetime. */
	TSharedPtr<SWidget> ContentBrowser;
	TSharedPtr<SBox> DrawerSlot;
	TSharedPtr<SBorder> DrawerGrip;
	bool bDrawerOpen = false;

	/** Slide animation: bDrawerShown stays true until the closing slide ends. */
	FCurveSequence DrawerSlide;
	TSharedPtr<FActiveTimerHandle> DrawerSlideTimer;
	FThrottleRequest DrawerSlideThrottle;
	bool bDrawerShown = false;

	/** Closes the drawer when focus leaves the panel, while it is open. */
	FDelegateHandle DrawerFocusHandle;

	/** When a click on the page last closed the drawer (FPlatformTime seconds). */
	double LastClickCloseSeconds = 0.0;

	/** Drawer height as a share of the panel height; dragged with the grip and saved per user. */
	float DrawerHeightShare = 0.0f;
	bool bResizingDrawer = false;

public:
	// Defined in the .cpp, where UShuttlegentWebBridge is complete (TStrongObjectPtr needs it)
	SShuttlegentWebPanel();
	void Construct(const FArguments& InArgs);
	virtual ~SShuttlegentWebPanel() override;

	/** Called by the module when a tab starts or stops showing the panel; the instance outlives its tabs. */
	void SetHostTabOpen(bool bOpen);

	//~ Begin SWidget Interface.
	virtual FReply OnPreviewKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	//~ End SWidget Interface.

private:
	TSharedRef<SWidget> BuildPlaceholder() const;

	/** First paint of the panel: creates the bridge and the browser. */
	EActiveTimerReturnType HandleCreateBrowser(double InCurrentTime, float InDeltaTime);
	void BindBridge();

	/** Page name of the panel shortcut bound to this key, or nullptr. Matches the current (rebindable) chords. */
	const TCHAR* FindPanelCommand(const FKeyEvent& InKeyEvent) const;

	/** Whether the key matches the current chord of FShuttlegentPanelCommands::ReloadPage. */
	bool IsReloadPageChord(const FKeyEvent& InKeyEvent) const;

	/** Reloads the page from disk; the placeholder shows until the page reports ready again and gets a full snapshot. */
	void ReloadPage();

	FString GetPageUrl() const;

	/** The assets or actors an editor drag carries, as a JS array of references; empty for anything else. */
	FString GetDraggedItems(const FDragDropEvent& DragDropEvent) const;

	/** Sends the prompt with its references, or refuses when a reference no longer exists; the page learns which through onSubmitResult. */
	void HandleSubmit(const FString& Text, const FString& ReferencesJson);
	void HandleQueryReferences(const FString& Query);

	/** Takes a queued item back into the draft (the current draft swaps into its place) and hands it to the page. */
	void HandleTakeQueued(int32 ItemId, const FString& DraftText, const FString& DraftReferencesJson);
	void HandleWatchReferences(const FString& ReferencesJson);

	/** Tells the page which watched references still exist. */
	void PushReferenceValidity();

	void ExecuteJavascript(const FString& Script) const;

	/** Calls a page entry point with an argument that is already a JS literal (object, array, boolean). */
	void CallPage(const TCHAR* Entry, const FString& ArgumentLiteral) const;

	void HandlePageReady();
	bool HandleBeforeNavigation(const FString& Url, const FWebNavigationRequest& Request);
	bool HandleBeforePopup(FString Url, FString Frame);
	void HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line, EWebBrowserConsoleLogSeverity Severity);

	void ToggleDrawer();

	/** The page's add-reference button; ignored right after the same click closed the drawer. */
	void HandlePageToggleDrawer();
	void OpenDrawer();

	/** Starts the closing slide; focus returns to the page only when it was inside the drawer. */
	void CloseDrawer(const TCHAR* Reason);
	void PlayDrawerSlide();

	/** A drag left the drawer frame; the drawer closes while the drag goes on. */
	void HandleDrawerDragLeft();
	EActiveTimerReturnType HandleDrawerSlideTick(double InCurrentTime, float InDeltaTime);
	TOptional<FSlateRenderTransform> GetDrawerTransform() const;
	void HandleDrawerFocusChanging(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldPath, const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewPath, const TSharedPtr<SWidget>& NewWidget);
	FOptionalSize GetDrawerHeight() const;
	FReply HandleGripMouseDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
	FReply HandleGripMouseMove(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
	FReply HandleGripMouseUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
};
