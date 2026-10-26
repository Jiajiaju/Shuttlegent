#include "Features/IModularFeatures.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "FShuttlegentSampleToolProvider.h"

/** Registers the sample's tool provider with Shuttlegent while the module is loaded. */
class FShuttlegentSampleModule : public IModuleInterface
{
private:
	TSharedPtr<FShuttlegentSampleToolProvider> Provider;

public:
	//~ Begin IModuleInterface Interface.
	virtual void StartupModule() override
	{
		Provider = MakeShared<FShuttlegentSampleToolProvider>();
		IModularFeatures::Get().RegisterModularFeature(IShuttlegentToolProvider::GetModularFeatureName(), Provider.Get());
	}

	virtual void ShutdownModule() override
	{
		if (Provider.IsValid()) IModularFeatures::Get().UnregisterModularFeature(IShuttlegentToolProvider::GetModularFeatureName(), Provider.Get());
		Provider.Reset();
	}
	//~ End IModuleInterface Interface.
};

IMPLEMENT_MODULE(FShuttlegentSampleModule, ShuttlegentSample)
