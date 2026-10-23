#pragma once

#include "Modules/ModuleInterface.h"

class FShuttlegentEditorModule : public IModuleInterface
{
public:
	//~ Begin IModuleInterface Interface.
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface.
};
