#pragma once

#include "Modules/ModuleInterface.h"

class FShuttlegentToolsetToolProvider;

class FShuttlegentToolsetBridgeModule : public IModuleInterface
{
private:
	/** 引擎 Toolset 工具，以 IShuttlegentToolProvider 的形式注册到 MCP 端点。 */
	TSharedPtr<FShuttlegentToolsetToolProvider> Provider;

public:
	//~ Begin IModuleInterface Interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface.
};
