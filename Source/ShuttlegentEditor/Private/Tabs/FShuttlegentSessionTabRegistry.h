#pragma once

#include "CoreMinimal.h"

#include "Chat/EShuttlegentAttention.h"

class FShuttlegentChatModel;
class FShuttlegentChatView;
class FTabManager;
class ILevelEditor;
class SDockTab;
class SShuttlegentWebPanel;

/** 一个会话页签：ViewId 决定页签的类型名，视图与面板随页签关闭一并释放。仅供 FShuttlegentSessionTabRegistry 使用。 */
struct FShuttlegentSessionTab
{
	FGuid ViewId {};

	/** 页签类型名 ShuttlegentSession.<ViewId>，在 TabManager 上注册了 Spawner。 */
	FName TabType;

	/** 注册 Spawner 的关卡编辑器页签管理器；关卡编辑器重建后旧管理器失效。 */
	TWeakPtr<FTabManager> TabManager;

	TSharedPtr<FShuttlegentChatView> View;
	TSharedPtr<SShuttlegentWebPanel> Panel;
	TWeakPtr<SDockTab> Tab;
};

/** 按用户配置中记下的一个会话页签：重启后据此注册 Spawner，等关卡编辑器恢复布局时接回。仅供 FShuttlegentSessionTabRegistry 使用。 */
struct FShuttlegentSessionBinding
{
	FGuid ViewId {};

	/** 页签显示的会话；新对话为空。 */
	FString SessionId;
};

/**
 * 会话窗口（工作区 Doc/ShuttlegentSessionTabs_Design.md §6、§9、§10）：从主页签会话列表拖出的会话各自成为一个页签，有自己的视图、面板与 CEF 页面。
 * 会话页签是关卡编辑器页签管理器里的 PanelTab，与 Details、Outliner、视口同级：可以单独浮出、互相组合，也能与这些面板叠放、分割，
 * 但进不了编辑器顶部归全局管理器的大页签栏。拖出时先在光标处开一个会话窗口，再借用 SDockTab::OnDragDetected 交给引擎原生的页签拖拽。
 * 一个会话只在一处显示：拖已在窗口中的会话即拖那个页签。
 * 重启恢复：各页签绑定的会话与主页签当前的会话记在按用户配置里；关卡编辑器恢复布局前为记下的页签注册 Spawner，布局按类型名把页签接回原位置，
 * 会话只回放记录，页签第一次到前台时才恢复。仅限游戏线程。
 */
class FShuttlegentSessionTabRegistry : public TSharedFromThis<FShuttlegentSessionTabRegistry>
{
private:
	TSharedPtr<FShuttlegentChatModel> Model;
	TWeakPtr<FShuttlegentChatView> HubView;
	TArray<FShuttlegentSessionTab> Tabs;

	/** 已注册 Spawner、等待关卡编辑器恢复布局时接回的页签。 */
	TArray<FShuttlegentSessionBinding> PendingRestore;
	TWeakPtr<FTabManager> PendingTabManager;

	/** 上次写入配置的绑定；只在变化时写盘。 */
	FString SavedSnapshot;

	/** 关卡编辑器完成过一次布局恢复；此前配置中的绑定尚未接回，不能被当前（空的）状态覆盖。 */
	bool bRestored = false;

	FDelegateHandle RegisterTabsHandle;
	FDelegateHandle LevelEditorCreatedHandle;

public:
	void Initialize(const TSharedRef<FShuttlegentChatModel>& InModel, const TSharedRef<FShuttlegentChatView>& InHubView);

	/** 编辑器退出：交还各页签的对话，不再接收页签事件。 */
	void Shutdown();

	/** 会话列表中一行被按住拖动：会话已在某个会话窗口里时拖那个页签，否则在光标处新开一个会话窗口再拖；左键已松开时不拖。 */
	void BeginDrag(const FString& SessionId);

	/** 有会话页签在显示该会话时把它切到前台（浮动时连同窗口）并返回 true。 */
	bool FocusSession(const FString& SessionId) const;

	/** 把各会话页签是否在前台告诉其视图，并在绑定变化时写入配置；由模块定时调用。 */
	void UpdateVisibility();

	/** 主页签显示上次的当前会话（只回放记录）；会话已删除、已归档或绑定在某个会话页签上时不显示。模块启动时调用一次。 */
	void RestoreHub();

	/** 提醒对应的页签色条颜色；没有提醒时透明，页签不画色条。 */
	static FLinearColor GetAttentionColor(EShuttlegentAttention Attention);

private:
	/** 关卡编辑器恢复布局之前（FLevelEditorModule::OnRegisterTabs）：为配置中记下的页签注册 Spawner。 */
	void RegisterRestoredTabs(TSharedPtr<FTabManager> TabManager);

	/** 关卡编辑器恢复布局之后（OnLevelEditorCreated）：布局里没有的页签注销 Spawner、删去绑定。 */
	void HandleLevelEditorCreated(TSharedPtr<ILevelEditor> LevelEditor);
	void FinishRestore();

	const FShuttlegentSessionTab* FindByView(const FGuid& ViewId) const;
	TSharedPtr<SDockTab> FindTab(const FString& SessionId) const;

	static FName MakeTabType(const FGuid& ViewId);
	static TArray<FShuttlegentSessionBinding> LoadBindings();

	/** 注销页签的 Spawner 与默认窗口尺寸。 */
	static void UnregisterTab(const FName& TabType, const TWeakPtr<FTabManager>& TabManager);

	/** 交还页签的对话并注销，不再接收其事件；页签项由调用方删除。 */
	static void ReleaseTab(const FShuttlegentSessionTab& Entry);

	/** 为会话新建视图、面板与页签并登记；会话正显示在主页签时主页签改为新对话。bDeferResume 时会话只回放记录。 */
	TSharedRef<SDockTab> BuildTab(const FGuid& ViewId, const TSharedRef<FTabManager>& TabManager, const FString& SessionId, bool bDeferResume);

	/** 拖出新会话：建页签、注册 Spawner，在光标处的新会话窗口中打开。 */
	TSharedPtr<SDockTab> CreateTab(const FString& SessionId);

	/** 布局恢复时引擎调用记下的页签的 Spawner。 */
	TSharedRef<SDockTab> SpawnRestoredTab(const FGuid& ViewId);

	/** 当前各页签的绑定与主页签的会话有变化时写入配置。 */
	void SaveBindings();

	FText GetTabLabel(const FGuid& ViewId) const;
	FLinearColor GetTabColor(const FGuid& ViewId) const;
	void HandleTabClosed(const FGuid& ViewId, const TSharedRef<SDockTab>& ClosedTab);
};
