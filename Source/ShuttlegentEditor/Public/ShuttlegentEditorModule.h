#pragma once

#include "Containers/Ticker.h"
#include "Modules/ModuleInterface.h"

class FShuttlegentChatModel;
class FSpawnTabArgs;
class SDockTab;
class SShuttlegentWebPanel;

class FShuttlegentEditorModule : public IModuleInterface
{
private:
	TSharedPtr<FShuttlegentChatModel> ChatModel;

	/** 聊天面板，在标签页关闭与重新打开之间保留，使页面保持其状态。 */
	TSharedPtr<SShuttlegentWebPanel> Panel;

	/** 编辑器启动后对运行时执行一次性预热（UShuttlegentSettings::bPrewarmRuntime）。 */
	FTSTicker::FDelegateHandle PrewarmHandle;

public:
	//~ Begin IModuleInterface Interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface.

private:
	TSharedRef<SDockTab> SpawnChatTab(const FSpawnTabArgs& Args);
	void RegisterMenus();
	void HandleEngineInitComplete();
};
