#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentProviderApi.h"

/** 运行时的一个模型路由：一条已连通的模型连接。 */
struct FShuttlegentModelRoute
{
	/** 路由名：内置为 deepseek-official；其余由地址与协议确定性生成。 */
	FString Route;

	/** 接口协议。 */
	EShuttlegentProviderApi Api = EShuttlegentProviderApi::OpenAiCompletions;

	/** 去掉首尾空白与末尾斜杠的地址。 */
	FString BaseUrl;

	/** 连接的密钥；只在内存中使用，经环境变量传给运行时。 */
	FString ApiKey;

	/** 运行时读取该路由密钥的环境变量。 */
	FString KeyVariable;

	/** 内置 DeepSeek 路由，由运行时 llm-deepseek 提供。 */
	bool bBuiltin = false;

	/** 可用的模型 ID，最新的在前。 */
	TArray<FString> ModelIds;

	/** 查得推理强度档位的模型 ID 到其档位（从低到高）；不在其中的模型不声明档位。内置路由由运行时自带。 */
	TMap<FString, TArray<FString>> ModelEfforts;

	/** 来源连接在 UShuttlegentSettings::Connections 中的下标。 */
	int32 ConnectionIndex = INDEX_NONE;
};
