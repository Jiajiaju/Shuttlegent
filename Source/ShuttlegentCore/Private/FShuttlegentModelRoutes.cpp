#include "FShuttlegentModelRoutes.h"

#include "Misc/Crc.h"

#include "FShuttlegentConnection.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentSettings.h"

namespace FShuttlegentModelRoutesPrivate
{
	/** 路由的密钥变量：SHUTTLEGENT_<ROUTE>_API_KEY，连字符换成下划线。 */
	FString MakeKeyVariable(const FString& Route)
	{
		const FString Upper = Route.ToUpper().Replace(ShuttlegentProviderConstant::RouteSeparator, ShuttlegentProviderConstant::VariableSeparator);
		return FString(ShuttlegentProviderConstant::KeyVariablePrefix) + Upper + ShuttlegentProviderConstant::KeyVariableSuffix;
	}

	/** 路由名：前缀 + “地址 + 协议”的 CRC；不含密钥，改密钥不改路由名，会话记录中的模型因此仍然有效。 */
	FString MakeGeneratedRoute(const FString& BaseUrl, EShuttlegentProviderApi Api)
	{
		const FString Key = FShuttlegentModelRoutes::TrimBaseUrl(BaseUrl).ToLower() + ShuttlegentProviderConstant::RouteKeySeparator + FShuttlegentModelRoutes::GetApiName(Api);
		return FString(ShuttlegentProviderConstant::GeneratedRoutePrefix) + FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*Key));
	}

	/** 内置模型的显示名对应的模型 ID；不是内置模型的显示名时原样返回。 */
	FString ResolveBuiltinName(const FString& Model)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ShuttlegentBuiltinModel::Names); ++Index)
		{
			if (Model == ShuttlegentBuiltinModel::Names[Index]) return ShuttlegentBuiltinModel::Ids[Index];
		}
		return Model;
	}

	bool IsConnected(const UShuttlegentSettings& Settings, const FShuttlegentModelRoute& Route)
	{
		return Settings.Connections.IsValidIndex(Route.ConnectionIndex) && Settings.Connections[Route.ConnectionIndex].Status == EShuttlegentConnectionStatus::Connected && !Route.ModelIds.IsEmpty();
	}
}

TArray<FShuttlegentModelRoute> FShuttlegentModelRoutes::Build(const UShuttlegentSettings& Settings)
{
	TArray<FShuttlegentModelRoute> Routes;
	TSet<FString> Used { ShuttlegentProviderConstant::BuiltinRoute };
	for (int32 Index = 0; Index < Settings.Connections.Num(); ++Index)
	{
		const FShuttlegentConnection& Connection = Settings.Connections[Index];
		const FString ApiKey = Connection.ApiKey.TrimStartAndEnd();
		if (ApiKey.IsEmpty()) continue;

		FShuttlegentModelRoute Route;
		Route.ApiKey = ApiKey;
		Route.ConnectionIndex = Index;
		if (Connection.bBuiltin)
		{
			// 内置连接填了密钥即交给运行时；是否作为默认模型仍看检测结论
			Route.Route = ShuttlegentProviderConstant::BuiltinRoute;
			Route.Api = EShuttlegentProviderApi::AnthropicMessages;
			Route.BaseUrl = ShuttlegentProviderConstant::DeepSeekBaseUrl;
			Route.KeyVariable = ShuttlegentEnvVar::ProviderKey;
			Route.bBuiltin = true;
			for (const TCHAR* ModelId : ShuttlegentBuiltinModel::Ids)
			{
				Route.ModelIds.Add(ModelId);
			}
			Routes.Add(MoveTemp(Route));
			continue;
		}

		const FString BaseUrl = TrimBaseUrl(Connection.BaseUrl);
		if (BaseUrl.IsEmpty() || Connection.Status != EShuttlegentConnectionStatus::Connected || Connection.ModelIds.IsEmpty()) continue;

		// 重名（同一地址与协议配了两次）依次加 -2、-3
		const FString Base = FShuttlegentModelRoutesPrivate::MakeGeneratedRoute(BaseUrl, Connection.Api);
		FString Name = Base;
		for (int32 Suffix = 2; Used.Contains(Name); ++Suffix)
		{
			Name = Base + ShuttlegentProviderConstant::RouteSeparator + FString::FromInt(Suffix);
		}
		Used.Add(Name);

		Route.Route = Name;
		Route.Api = Connection.Api;
		Route.BaseUrl = BaseUrl;
		Route.KeyVariable = FShuttlegentModelRoutesPrivate::MakeKeyVariable(Name);
		Route.ModelIds = Connection.ModelIds;
		Routes.Add(MoveTemp(Route));
	}
	return Routes;
}

bool FShuttlegentModelRoutes::FindDefaultModel(const UShuttlegentSettings& Settings, FShuttlegentModelRoute& OutRoute, FString& OutModelId)
{
	for (const FShuttlegentModelRoute& Route : Build(Settings))
	{
		if (!FShuttlegentModelRoutesPrivate::IsConnected(Settings, Route)) continue;

		OutRoute = Route;
		OutModelId = Route.ModelIds[0];
		return true;
	}
	return false;
}

bool FShuttlegentModelRoutes::FindModel(const UShuttlegentSettings& Settings, const FString& Model, FShuttlegentModelRoute& OutRoute, FString& OutModelId)
{
	const FString ModelId = FShuttlegentModelRoutesPrivate::ResolveBuiltinName(Model.TrimStartAndEnd());
	if (ModelId.IsEmpty()) return false;

	for (const FShuttlegentModelRoute& Route : Build(Settings))
	{
		if (!FShuttlegentModelRoutesPrivate::IsConnected(Settings, Route) || !Route.ModelIds.Contains(ModelId)) continue;

		OutRoute = Route;
		OutModelId = ModelId;
		return true;
	}
	return false;
}

bool FShuttlegentModelRoutes::IsUsable(const UShuttlegentSettings& Settings, const FString& Route, const FString& ModelId)
{
	for (const FShuttlegentModelRoute& Item : Build(Settings))
	{
		if (Item.Route == Route) return FShuttlegentModelRoutesPrivate::IsConnected(Settings, Item) && Item.ModelIds.Contains(ModelId);
	}
	return false;
}

TArray<FString> FShuttlegentModelRoutes::GetModelIds(const UShuttlegentSettings& Settings)
{
	TArray<FString> ModelIds;
	for (const FShuttlegentModelRoute& Route : Build(Settings))
	{
		if (!FShuttlegentModelRoutesPrivate::IsConnected(Settings, Route)) continue;

		for (const FString& ModelId : Route.ModelIds)
		{
			ModelIds.AddUnique(ModelId);
		}
	}
	return ModelIds;
}

FString FShuttlegentModelRoutes::MakeFingerprint(const FShuttlegentConnection& Connection)
{
	const FString Key = TrimBaseUrl(Connection.BaseUrl).ToLower() + ShuttlegentProviderConstant::RouteKeySeparator + Connection.ApiKey.TrimStartAndEnd();
	return FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*Key));
}

FString FShuttlegentModelRoutes::TrimBaseUrl(const FString& BaseUrl)
{
	FString Url = BaseUrl.TrimStartAndEnd();
	while (Url.EndsWith(ShuttlegentProbeConstant::UrlSlash))
	{
		Url.LeftChopInline(FCString::Strlen(ShuttlegentProbeConstant::UrlSlash));
	}
	return Url;
}

const TCHAR* FShuttlegentModelRoutes::GetApiName(EShuttlegentProviderApi Api)
{
	switch (Api)
	{
	case EShuttlegentProviderApi::OpenAiResponses:
		{
			return ShuttlegentProviderConstant::ApiOpenAiResponses;
		}
	case EShuttlegentProviderApi::AnthropicMessages:
		{
			return ShuttlegentProviderConstant::ApiAnthropicMessages;
		}
	case EShuttlegentProviderApi::OpenAiCompletions:
	default:
		{
			return ShuttlegentProviderConstant::ApiOpenAiCompletions;
		}
	}
}
