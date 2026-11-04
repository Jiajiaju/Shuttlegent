#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentModelRoute.h"

class UShuttlegentSettings;

/**
 * 由个人设置中的模型推出运行时路由。路由对用户不可见；
 * 生成补丁、密钥解析、默认路由与启动检查都从这里取，规则只在一处。仅限游戏线程。
 */
struct SHUTTLEGENTCORE_API FShuttlegentModelRoutes
{
	/**
	 * 按“地址 + 协议 + 密钥 + 迁移来的路由名”分组；名称或模型 ID 为空的模型不进路由。
	 * 同一组设置多次调用得到同样的路由名。
	 */
	static TArray<FShuttlegentModelRoute> Build(const UShuttlegentSettings& Settings);

	/**
	 * 模型实际使用的密钥：自己的；留空时沿用地址与协议相同、排在最前且有密钥的模型的。
	 * OutSourceIndex 为提供密钥的模型下标，没有密钥时为 INDEX_NONE。
	 */
	static FString ResolveKey(const UShuttlegentSettings& Settings, int32 ModelIndex, int32& OutSourceIndex);

	/** 按名称找模型，大小写敏感；找不到时返回 INDEX_NONE。 */
	static int32 FindModel(const UShuttlegentSettings& Settings, const FString& Name);

	/** 新会话的默认模型：默认值中的模型名称；为空或找不到时为第一个内置模型。 */
	static int32 FindDefaultModel(const UShuttlegentSettings& Settings);

	/** 模型所在的路由；模型不在任何路由中时返回 false。 */
	static bool FindRoute(const TArray<FShuttlegentModelRoute>& Routes, int32 ModelIndex, FShuttlegentModelRoute& OutRoute);

	/** 地址与协议是否相同：去掉首尾空白与末尾斜杠后不区分大小写比较。 */
	static bool IsSameEndpoint(const FString& LeftUrl, EShuttlegentProviderApi LeftApi, const FString& RightUrl, EShuttlegentProviderApi RightApi);

	/** 运行时 llm-pi-ai 的 api 取值。 */
	static const TCHAR* GetApiName(EShuttlegentProviderApi Api);
};
