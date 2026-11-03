#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentProviderApi.h"

/** 检测连接的对象：一个模型及其所在路由的地址、协议与密钥，取值与运行时实际使用的一致。 */
struct FShuttlegentModelProbeTarget
{
	/** 结果中显示的名字：模型名称，没有时为模型 ID。 */
	FString Label;

	/** 发给服务方的模型 ID。 */
	FString ModelId;

	/** 路由的基础 URL。 */
	FString BaseUrl;

	/** 路由的接口协议；内置 DeepSeek 固定为 AnthropicMessages。 */
	EShuttlegentProviderApi Api = EShuttlegentProviderApi::OpenAiCompletions;

	/** 内置 DeepSeek 路由：由 llm-deepseek 发请求，地址已以 /v1 结尾时不再补 /v1。 */
	bool bBuiltinDeepSeek = false;

	/** 运行时将收到的密钥；只在内存中使用，不写日志。 */
	FString ApiKey;
};
