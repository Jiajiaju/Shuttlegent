#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentModelRoute.h"

class UShuttlegentSettings;
struct FShuttlegentConnection;

/**
 * 由个人设置中的模型连接推出运行时路由。路由对用户不可见；
 * 生成补丁、密钥解析、默认模型与启动检查都从这里取，规则只在一处。仅限游戏线程。
 */
struct SHUTTLEGENTCORE_API FShuttlegentModelRoutes
{
	/**
	 * 每条可用的连接一个路由，按设置中的顺序：内置 DeepSeek 连接填了密钥即可用；
	 * 其余连接须已检测连通、有模型。同一组设置多次调用得到同样的路由名。
	 */
	static TArray<FShuttlegentModelRoute> Build(const UShuttlegentSettings& Settings);

	/** 新会话的默认模型：第一条已连通的连接中的第一个模型。没有已连通的连接时返回 false。 */
	static bool FindDefaultModel(const UShuttlegentSettings& Settings, FShuttlegentModelRoute& OutRoute, FString& OutModelId);

	/** 按模型 ID（内置模型也可用其显示名）找所在的第一个已连通路由；找不到时返回 false。 */
	static bool FindModel(const UShuttlegentSettings& Settings, const FString& Model, FShuttlegentModelRoute& OutRoute, FString& OutModelId);

	/** 路由名与模型 ID 指向的模型现在可用：所在连接已连通且列出了该模型。 */
	static bool IsUsable(const UShuttlegentSettings& Settings, const FString& Route, const FString& ModelId);

	/** 已连通的连接提供的全部模型 ID，按连接顺序，重复的只列一次。 */
	static TArray<FString> GetModelIds(const UShuttlegentSettings& Settings);

	/** 连接的地址与密钥指纹（CRC），用于判断检测后是否改过；不含密钥原文。 */
	static FString MakeFingerprint(const FShuttlegentConnection& Connection);

	/** 去掉首尾空白与末尾斜杠的地址，大小写不变。 */
	static FString TrimBaseUrl(const FString& BaseUrl);

	/** 运行时 llm-pi-ai 的 api 取值。 */
	static const TCHAR* GetApiName(EShuttlegentProviderApi Api);
};
