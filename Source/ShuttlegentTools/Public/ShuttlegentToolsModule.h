#pragma once

#include "Modules/ModuleInterface.h"

class FShuttlegentBuiltinToolProvider;
class FShuttlegentLogCapture;

class FShuttlegentToolsModule : public IModuleInterface
{
private:
	/** Output Log lines since startup, for read_log. */
	TSharedPtr<FShuttlegentLogCapture> LogCapture;

	/** The general editor tools, registered with the MCP endpoint as an IShuttlegentToolProvider. */
	TSharedPtr<FShuttlegentBuiltinToolProvider> Provider;

public:
	//~ Begin IModuleInterface Interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface.
};
