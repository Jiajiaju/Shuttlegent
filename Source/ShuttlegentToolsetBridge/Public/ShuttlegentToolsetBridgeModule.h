#pragma once

#include "Modules/ModuleInterface.h"

class FShuttlegentToolsetToolProvider;

class FShuttlegentToolsetBridgeModule : public IModuleInterface
{
private:
	/** The engine toolset tools, registered with the MCP endpoint as an IShuttlegentToolProvider. */
	TSharedPtr<FShuttlegentToolsetToolProvider> Provider;

public:
	//~ Begin IModuleInterface Interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface.
};
