#include "Tool/IShuttlegentToolProvider.h"

#include "ShuttlegentMcpServerConstants.h"

FName IShuttlegentToolProvider::GetModularFeatureName()
{
	return ShuttlegentMcpTool::ProviderFeatureName;
}
