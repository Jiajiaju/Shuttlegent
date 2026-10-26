#include "ShuttlegentToolsModule.h"

#include "Features/IModularFeatures.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Modules/ModuleManager.h"

#include "FShuttlegentBuiltinToolProvider.h"
#include "Tools/FShuttlegentLogCapture.h"

void FShuttlegentToolsModule::StartupModule()
{
	LogCapture = MakeShared<FShuttlegentLogCapture>();
	if (GLog != nullptr) GLog->AddOutputDevice(LogCapture.Get());

	Provider = MakeShared<FShuttlegentBuiltinToolProvider>(LogCapture.ToSharedRef());
	IModularFeatures::Get().RegisterModularFeature(IShuttlegentToolProvider::GetModularFeatureName(), Provider.Get());
}

void FShuttlegentToolsModule::ShutdownModule()
{
	if (Provider.IsValid()) IModularFeatures::Get().UnregisterModularFeature(IShuttlegentToolProvider::GetModularFeatureName(), Provider.Get());
	Provider.Reset();

	if (GLog != nullptr && LogCapture.IsValid()) GLog->RemoveOutputDevice(LogCapture.Get());
	LogCapture.Reset();
}

IMPLEMENT_MODULE(FShuttlegentToolsModule, ShuttlegentTools)
