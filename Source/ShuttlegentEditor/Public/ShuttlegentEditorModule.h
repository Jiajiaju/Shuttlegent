#pragma once

#include "Containers/Ticker.h"
#include "Modules/ModuleInterface.h"

class FShuttlegentChatModel;
class FShuttlegentChatView;
class FSpawnTabArgs;
class SDockTab;
class SShuttlegentWebPanel;
struct FLinearColor;

class FShuttlegentEditorModule : public IModuleInterface
{
private:
	TSharedPtr<FShuttlegentChatModel> ChatModel;

	/** 主页签的对话视图：主页签显示的当前对话。 */
	TSharedPtr<FShuttlegentChatView> HubView;

	/** 聊天面板，在标签页关闭与重新打开之间保留，使页面保持其状态。 */
	TSharedPtr<SShuttlegentWebPanel> Panel;

	/** 编辑器启动后对运行时执行一次性预热（UShuttlegentSettings::bPrewarmRuntime）。 */
	FTSTicker::FDelegateHandle PrewarmHandle;

	/** 当前打开的聊天页签；不在前台时据此显示提醒色条。 */
	TWeakPtr<SDockTab> ChatTab;

	/** 定时把页签是否在前台告诉主页签的视图（Slate 没有页签转入后台的通知）。 */
	FTSTicker::FDelegateHandle TabForegroundHandle;

public:
	//~ Begin IModuleInterface Interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface.

private:
	TSharedRef<SDockTab> SpawnChatTab(const FSpawnTabArgs& Args);
	void RegisterMenus();
	void HandleEngineInitComplete();

	/** 页签色条：不在前台时按对话模型的提醒为蓝（完成未看）或黄（等待输入），否则透明不显示。 */
	FLinearColor GetChatTabColor() const;
	bool HandleTabForegroundCheck(float DeltaTime);
};
