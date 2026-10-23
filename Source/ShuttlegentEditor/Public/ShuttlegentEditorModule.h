#pragma once

#include "Modules/ModuleInterface.h"

class FShuttlegentChatModel;
class FSpawnTabArgs;
class SDockTab;

class FShuttlegentEditorModule : public IModuleInterface
{
private:
	TSharedPtr<FShuttlegentChatModel> ChatModel;

public:
	//~ Begin IModuleInterface Interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface.

private:
	TSharedRef<SDockTab> SpawnChatTab(const FSpawnTabArgs& Args);
	void RegisterMenus();
};
