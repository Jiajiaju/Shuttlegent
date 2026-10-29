#include "ShuttlegentToolsetBridgeModule.h"

#include "Features/IModularFeatures.h"
#include "Modules/ModuleManager.h"

#include "FShuttlegentToolsetToolProvider.h"

void FShuttlegentToolsetBridgeModule::StartupModule()
{
	Provider = MakeShared<FShuttlegentToolsetToolProvider>();
	IModularFeatures::Get().RegisterModularFeature(IShuttlegentToolProvider::GetModularFeatureName(), Provider.Get());
}

void FShuttlegentToolsetBridgeModule::ShutdownModule()
{
	if (Provider.IsValid()) IModularFeatures::Get().UnregisterModularFeature(IShuttlegentToolProvider::GetModularFeatureName(), Provider.Get());
	Provider.Reset();
}

IMPLEMENT_MODULE(FShuttlegentToolsetBridgeModule, ShuttlegentToolsetBridge)
