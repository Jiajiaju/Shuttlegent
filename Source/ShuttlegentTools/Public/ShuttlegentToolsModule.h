#pragma once

#include "Modules/ModuleInterface.h"

class FShuttlegentBuiltinToolProvider;
class FShuttlegentLogCapture;

class FShuttlegentToolsModule : public IModuleInterface
{
private:
	/** 自启动以来的 Output Log 日志行，供 read_log 使用。 */
	TSharedPtr<FShuttlegentLogCapture> LogCapture;

	/** 通用编辑器工具，作为 IShuttlegentToolProvider 注册到 MCP 端点。 */
	TSharedPtr<FShuttlegentBuiltinToolProvider> Provider;

public:
	//~ Begin IModuleInterface Interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface.
};
