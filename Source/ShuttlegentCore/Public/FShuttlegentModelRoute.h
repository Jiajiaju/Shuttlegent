#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentProviderApi.h"

/** 运行时的一个模型路由：地址、协议、密钥相同的模型归为一组（ShuttlegentAgentModel_Design.md 第 7 节）。 */
struct FShuttlegentModelRoute
{
	/** 路由名：内置为 deepseek-official；迁移来的沿用旧路由名；其余由地址与协议确定性生成。 */
	FString Route;

	/** 接口协议。 */
	EShuttlegentProviderApi Api = EShuttlegentProviderApi::OpenAiCompletions;

	/** 去掉首尾空白的地址。 */
	FString BaseUrl;

	/** 该组实际使用的密钥；只在内存中使用，经环境变量传给运行时。 */
	FString ApiKey;

	/** 运行时读取该组密钥的环境变量。 */
	FString KeyVariable;

	/** 内置 DeepSeek 路由，由运行时 llm-deepseek 提供。 */
	bool bBuiltin = false;

	/** 组内模型在 UShuttlegentSettings::Models 中的下标，按设置中的顺序。 */
	TArray<int32> ModelIndices;
};
