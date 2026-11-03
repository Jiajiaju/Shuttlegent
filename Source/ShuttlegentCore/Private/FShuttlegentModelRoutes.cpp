#include "FShuttlegentModelRoutes.h"

#include "Misc/Crc.h"

#include "ShuttlegentConstants.h"
#include "ShuttlegentSettings.h"

namespace FShuttlegentModelRoutesPrivate
{
	/** 用于比较的地址：去掉首尾空白与末尾斜杠，转小写。 */
	FString NormalizeUrl(const FString& BaseUrl)
	{
		FString Url = BaseUrl.TrimStartAndEnd().ToLower();
		while (Url.EndsWith(ShuttlegentProbeConstant::UrlSlash))
		{
			Url.LeftChopInline(FCString::Strlen(ShuttlegentProbeConstant::UrlSlash));
		}
		return Url;
	}

	/** 名称与模型 ID 都填写了的模型才进路由。 */
	bool IsUsable(const FShuttlegentModelEntry& Entry)
	{
		return !Entry.Name.TrimStartAndEnd().IsEmpty() && !Entry.ModelId.TrimStartAndEnd().IsEmpty();
	}

	/** 路由的密钥变量：SHUTTLEGENT_<ROUTE>_API_KEY，连字符换成下划线。 */
	FString MakeKeyVariable(const FString& Route)
	{
		const FString Upper = Route.ToUpper().Replace(ShuttlegentProviderConstant::RouteSeparator, ShuttlegentProviderConstant::VariableSeparator);
		return FString(ShuttlegentProviderConstant::KeyVariablePrefix) + Upper + ShuttlegentProviderConstant::KeyVariableSuffix;
	}

	/** 未迁移来的路由名：前缀 + “地址 + 协议”的 CRC；不含密钥，改密钥不改路由名。 */
	FString MakeGeneratedRoute(const FString& BaseUrl, EShuttlegentProviderApi Api)
	{
		const FString Key = NormalizeUrl(BaseUrl) + ShuttlegentProviderConstant::RouteKeySeparator + FShuttlegentModelRoutes::GetApiName(Api);
		return FString(ShuttlegentProviderConstant::GeneratedRoutePrefix) + FString::Printf(TEXT("%08x"), FCrc::StrCrc32(*Key));
	}
}

TArray<FShuttlegentModelRoute> FShuttlegentModelRoutes::Build(const UShuttlegentSettings& Settings)
{
	FShuttlegentModelRoute Builtin;
	Builtin.Route = ShuttlegentProviderConstant::BuiltinRoute;
	Builtin.Api = EShuttlegentProviderApi::AnthropicMessages;
	Builtin.KeyVariable = ShuttlegentEnvVar::ProviderKey;
	Builtin.bBuiltin = true;

	// 其余模型按“地址 + 协议 + 密钥 + 迁移来的路由名”分组，组的顺序即设置中首次出现的顺序
	TArray<FShuttlegentModelRoute> Groups;
	TArray<FString> GroupLegacyRoutes;
	TMap<FString, int32> GroupByKey;
	for (int32 ModelIndex = 0; ModelIndex < Settings.Models.Num(); ++ModelIndex)
	{
		const FShuttlegentModelEntry& Entry = Settings.Models[ModelIndex];
		if (!FShuttlegentModelRoutesPrivate::IsUsable(Entry)) continue;

		int32 SourceIndex = INDEX_NONE;
		const FString ApiKey = ResolveKey(Settings, ModelIndex, SourceIndex);
		if (Entry.bBuiltin)
		{
			if (Builtin.ModelIndices.IsEmpty())
			{
				const FString BaseUrl = Entry.BaseUrl.TrimStartAndEnd();
				Builtin.BaseUrl = BaseUrl.IsEmpty() ? FString(ShuttlegentProviderConstant::DeepSeekBaseUrl) : BaseUrl;
				Builtin.ApiKey = ApiKey;
			}
			Builtin.ModelIndices.Add(ModelIndex);
			continue;
		}

		const FString GroupKey = FString::Join(TArray<FString> { FShuttlegentModelRoutesPrivate::NormalizeUrl(Entry.BaseUrl), GetApiName(Entry.Api), ApiKey, Entry.LegacyRoute }, ShuttlegentProviderConstant::RouteKeySeparator);
		const int32* Found = GroupByKey.Find(GroupKey);
		if (Found != nullptr)
		{
			Groups[*Found].ModelIndices.Add(ModelIndex);
			continue;
		}

		FShuttlegentModelRoute& Group = Groups.AddDefaulted_GetRef();
		Group.Api = Entry.Api;
		Group.BaseUrl = Entry.BaseUrl.TrimStartAndEnd();
		Group.ApiKey = ApiKey;
		Group.ModelIndices.Add(ModelIndex);
		GroupLegacyRoutes.Add(Entry.LegacyRoute);
		GroupByKey.Add(GroupKey, Groups.Num() - 1);
	}

	// 路由名：迁移来的沿用旧名，其余按地址与协议生成；重名的依次加 -2、-3
	TSet<FString> Used { Builtin.Route };
	for (int32 GroupIndex = 0; GroupIndex < Groups.Num(); ++GroupIndex)
	{
		FShuttlegentModelRoute& Group = Groups[GroupIndex];
		const FString Base = GroupLegacyRoutes[GroupIndex].IsEmpty() ? FShuttlegentModelRoutesPrivate::MakeGeneratedRoute(Group.BaseUrl, Group.Api) : GroupLegacyRoutes[GroupIndex];
		FString Route = Base;
		for (int32 Suffix = 2; Used.Contains(Route); ++Suffix)
		{
			Route = Base + ShuttlegentProviderConstant::RouteSeparator + FString::FromInt(Suffix);
		}

		Used.Add(Route);
		Group.Route = Route;
		Group.KeyVariable = FShuttlegentModelRoutesPrivate::MakeKeyVariable(Route);
	}

	TArray<FShuttlegentModelRoute> Routes;
	if (!Builtin.ModelIndices.IsEmpty()) Routes.Add(MoveTemp(Builtin));
	Routes.Append(MoveTemp(Groups));
	return Routes;
}

FString FShuttlegentModelRoutes::ResolveKey(const UShuttlegentSettings& Settings, int32 ModelIndex, int32& OutSourceIndex)
{
	OutSourceIndex = INDEX_NONE;
	if (!Settings.Models.IsValidIndex(ModelIndex)) return FString();

	const FShuttlegentModelEntry& Entry = Settings.Models[ModelIndex];
	const FString Own = Entry.ApiKey.TrimStartAndEnd();
	if (!Own.IsEmpty())
	{
		OutSourceIndex = ModelIndex;
		return Own;
	}

	for (int32 OtherIndex = 0; OtherIndex < Settings.Models.Num(); ++OtherIndex)
	{
		const FShuttlegentModelEntry& Other = Settings.Models[OtherIndex];
		const FString OtherKey = Other.ApiKey.TrimStartAndEnd();
		if (OtherIndex == ModelIndex || OtherKey.IsEmpty() || Other.bBuiltin != Entry.bBuiltin) continue;
		if (!IsSameEndpoint(Entry.BaseUrl, Entry.Api, Other.BaseUrl, Other.Api)) continue;

		OutSourceIndex = OtherIndex;
		return OtherKey;
	}
	return FString();
}

int32 FShuttlegentModelRoutes::FindModel(const UShuttlegentSettings& Settings, const FString& Name)
{
	if (Name.IsEmpty()) return INDEX_NONE;

	return Settings.Models.IndexOfByPredicate([&Name](const FShuttlegentModelEntry& Entry) { return Entry.Name == Name && FShuttlegentModelRoutesPrivate::IsUsable(Entry); });
}

int32 FShuttlegentModelRoutes::FindDefaultModel(const UShuttlegentSettings& Settings)
{
	const int32 Named = FindModel(Settings, Settings.DefaultModelName);
	if (Named != INDEX_NONE) return Named;

	const FString FirstBuiltinId = ShuttlegentBuiltinModel::Ids[0];
	return Settings.Models.IndexOfByPredicate([&FirstBuiltinId](const FShuttlegentModelEntry& Entry) { return Entry.bBuiltin && Entry.ModelId == FirstBuiltinId; });
}

bool FShuttlegentModelRoutes::FindRoute(const TArray<FShuttlegentModelRoute>& Routes, int32 ModelIndex, FShuttlegentModelRoute& OutRoute)
{
	const FShuttlegentModelRoute* Found = Routes.FindByPredicate([ModelIndex](const FShuttlegentModelRoute& Route) { return Route.ModelIndices.Contains(ModelIndex); });
	if (Found == nullptr) return false;

	OutRoute = *Found;
	return true;
}

bool FShuttlegentModelRoutes::IsSameEndpoint(const FString& LeftUrl, EShuttlegentProviderApi LeftApi, const FString& RightUrl, EShuttlegentProviderApi RightApi)
{
	return LeftApi == RightApi && FShuttlegentModelRoutesPrivate::NormalizeUrl(LeftUrl) == FShuttlegentModelRoutesPrivate::NormalizeUrl(RightUrl);
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
