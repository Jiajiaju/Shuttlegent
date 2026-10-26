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

	/** The chat panel, kept across tab close and reopen so the page keeps its state. */
	TSharedPtr<SShuttlegentWebPanel> Panel;

	/** One-shot prewarm of the runtime once the editor has started (UShuttlegentSettings::bPrewarmRuntime). */
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
