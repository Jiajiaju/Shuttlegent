#include "FShuttlegentSecrets.h"

#include "FShuttlegentModelRoutes.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentSettings.h"

bool FShuttlegentSecrets::Resolve(const FString& Variable, FString& OutValue)
{
	OutValue.Reset();
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	if (Variable.IsEmpty() || !IsValid(Settings)) return false;

	// 模型路由的密钥：内置 DeepSeek 为 DEEPSEEK_API_KEY，其余为各路由的 SHUTTLEGENT_<ROUTE>_API_KEY
	const TArray<FShuttlegentModelRoute> Routes = FShuttlegentModelRoutes::Build(*Settings);
	const FShuttlegentModelRoute* Route = Routes.FindByPredicate([&Variable](const FShuttlegentModelRoute& Item) { return Item.KeyVariable == Variable; });
	if (Route != nullptr)
	{
		OutValue = Route->ApiKey;
		return !OutValue.IsEmpty();
	}
	if (Variable == ShuttlegentEnvVar::ProviderKey) return false;

	const FString* HeaderValue = Settings->McpHeaderValues.Find(Variable);
	if (HeaderValue != nullptr) OutValue = HeaderValue->TrimStartAndEnd();
	return !OutValue.IsEmpty();
}
