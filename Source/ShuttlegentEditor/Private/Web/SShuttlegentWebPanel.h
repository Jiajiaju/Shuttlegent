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
 * Shuttlegent 面板：在 CEF 中承载 Resources/Web/index.html，并将其连接到 FShuttlegentChatModel。
 *
 * 浏览器在面板首次绘制时创建，页面报告就绪前显示占位内容。宿主在页面之外承担 CEF
 * 无法处理的部分：编辑器拖放、Ctrl+Space 与 Esc、面板自有的 Content Browser 抽屉，
 * 以及 http(s) 链接（在系统浏览器中打开）。模块只保留一个实例并将其放入每个生成的标签页，
 * 因此关闭并重新打开标签页后页面得以保留。
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

	/** 草稿引用：@ 菜单的候选项，以及需要向页面告知其存在状态的引用标签。 */
	TSharedPtr<FShuttlegentReferenceFinder> ReferenceFinder;
	TArray<FShuttlegentReference> WatchedReferences;

	/** 页面至少调用过一次 ready；在此之前由占位内容覆盖。 */
	bool bPageReady = false;

	/** 有停靠标签页正在显示面板；仅在此时面板才答复运行时权限请求。 */
	bool bHostTabOpen = false;

	/** Content Browser 抽屉：首次使用时创建的真实 SContentBrowser，在面板生命周期内保留。 */
	TSharedPtr<SWidget> ContentBrowser;
	TSharedPtr<SBox> DrawerSlot;
	TSharedPtr<SBorder> DrawerGrip;
	bool bDrawerOpen = false;

	/** 滑动动画：bDrawerShown 保持为 true，直到关闭滑动结束。 */
	FCurveSequence DrawerSlide;
	TSharedPtr<FActiveTimerHandle> DrawerSlideTimer;
	FThrottleRequest DrawerSlideThrottle;
	bool bDrawerShown = false;

	/** 抽屉打开期间，焦点离开面板时关闭抽屉。 */
	FDelegateHandle DrawerFocusHandle;

	/** 页面上的点击最近一次关闭抽屉的时间（FPlatformTime 秒）。 */
	double LastClickCloseSeconds = 0.0;

	/** 抽屉高度占面板高度的比例；通过拖动手柄调整并按用户保存。 */
	float DrawerHeightShare = 0.0f;
	bool bResizingDrawer = false;

public:
	// 定义在 .cpp 中，那里 UShuttlegentWebBridge 是完整类型（TStrongObjectPtr 需要）
	SShuttlegentWebPanel();
	void Construct(const FArguments& InArgs);
	virtual ~SShuttlegentWebPanel() override;

	/** 当某个标签页开始或停止显示面板时由模块调用；实例的生命周期长于其标签页。 */
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

	/** 面板首次绘制：创建桥接对象与浏览器。 */
	EActiveTimerReturnType HandleCreateBrowser(double InCurrentTime, float InDeltaTime);
	void BindBridge();

	/** 绑定到该按键的面板快捷键在页面中的名称，或 nullptr。按当前（可重新绑定的）组合键匹配。 */
	const TCHAR* FindPanelCommand(const FKeyEvent& InKeyEvent) const;

	/** 该按键是否匹配 FShuttlegentPanelCommands::ReloadPage 的当前组合键。 */
	bool IsReloadPageChord(const FKeyEvent& InKeyEvent) const;

	/** 从磁盘重新加载页面；占位内容一直显示，直到页面再次报告就绪并获得完整快照。 */
	void ReloadPage();

	FString GetPageUrl() const;

	/** 编辑器拖拽所携带的资产或 Actor，以 JS 引用数组表示；其他内容时为空。 */
	FString GetDraggedItems(const FDragDropEvent& DragDropEvent) const;

	/** 发送提示及其引用，或在某个引用已不存在时拒绝；页面通过 onSubmitResult 获知结果。 */
	void HandleSubmit(const FString& Text, const FString& ReferencesJson);
	void HandleQueryReferences(const FString& Query);

	/** 将排队的条目取回草稿（当前草稿换入其位置），并交给页面。 */
	void HandleTakeQueued(int32 ItemId, const FString& DraftText, const FString& DraftReferencesJson);
	void HandleWatchReferences(const FString& ReferencesJson);

	/** 告知页面哪些被监视的引用仍然存在。 */
	void PushReferenceValidity();

	void ExecuteJavascript(const FString& Script) const;

	/** 以一个已是 JS 字面量（对象、数组、布尔值）的参数调用页面入口。 */
	void CallPage(const TCHAR* Entry, const FString& ArgumentLiteral) const;

	void HandlePageReady();
	bool HandleBeforeNavigation(const FString& Url, const FWebNavigationRequest& Request);
	bool HandleBeforePopup(FString Url, FString Frame);
	void HandleConsoleMessage(const FString& Message, const FString& Source, int32 Line, EWebBrowserConsoleLogSeverity Severity);

	void ToggleDrawer();

	/** 页面的添加引用按钮；若同一次点击刚刚关闭了抽屉则忽略。 */
	void HandlePageToggleDrawer();
	void OpenDrawer();

	/** 开始关闭滑动；仅当焦点原本位于抽屉内时才将焦点交还页面。 */
	void CloseDrawer(const TCHAR* Reason);
	void PlayDrawerSlide();

	/** 拖拽离开了抽屉外框；抽屉在拖拽继续进行时关闭。 */
	void HandleDrawerDragLeft();
	EActiveTimerReturnType HandleDrawerSlideTick(double InCurrentTime, float InDeltaTime);
	TOptional<FSlateRenderTransform> GetDrawerTransform() const;
	void HandleDrawerFocusChanging(const FFocusEvent& FocusEvent, const FWeakWidgetPath& OldPath, const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewPath, const TSharedPtr<SWidget>& NewWidget);
	FOptionalSize GetDrawerHeight() const;
	FReply HandleGripMouseDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
	FReply HandleGripMouseMove(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
	FReply HandleGripMouseUp(const FGeometry& Geometry, const FPointerEvent& MouseEvent);
};
