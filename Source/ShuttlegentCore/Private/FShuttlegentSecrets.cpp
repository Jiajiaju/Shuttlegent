#include "FShuttlegentSecrets.h"

#include "FShuttlegentProjectConfig.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentSettings.h"

bool FShuttlegentSecrets::Resolve(const FString& Variable, FString& OutValue)
{
	OutValue.Reset();
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	if (Variable.IsEmpty() || !IsValid(Settings)) return false;

	if (Variable == ShuttlegentEnvVar::ProviderKey)
	{
		OutValue = Settings->DeepSeekApiKey.TrimStartAndEnd();
		return !OutValue.IsEmpty();
	}

	const FShuttlegentProviderEntry* Entry = Settings->Providers.FindByPredicate([&Variable](const FShuttlegentProviderEntry& Provider) { return FShuttlegentProjectConfig::GetKeyVariable(Provider) == Variable; });
	if (Entry != nullptr)
	{
		OutValue = Entry->ApiKey.TrimStartAndEnd();
		return !OutValue.IsEmpty();
	}

	const FString* HeaderValue = Settings->McpHeaderValues.Find(Variable);
	if (HeaderValue != nullptr) OutValue = HeaderValue->TrimStartAndEnd();
	return !OutValue.IsEmpty();
}
