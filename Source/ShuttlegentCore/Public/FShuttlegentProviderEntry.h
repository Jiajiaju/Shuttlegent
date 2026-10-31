#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentProviderApi.h"
#include "FShuttlegentProviderModel.h"
#include "ShuttlegentConstants.h"

#include "FShuttlegentProviderEntry.generated.h"

/** 用户经 llm-pi-ai 添加的模型供应商路由（Shuttlegent.md 第 8.2 节）。DeepSeek 已内置，无需添加。 */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentProviderEntry
{
	GENERATED_BODY()

	/** 路由 ID，在各供应商中唯一，例如 team-gateway；只用小写字母、数字和连字符。 */
	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	FString Route;

	/** 在面板中显示的名称。 */
	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	FString DisplayName;

	/** 该路由的接口协议。 */
	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	EShuttlegentProviderApi Api = EShuttlegentProviderApi::OpenAiCompletions;

	/** 端点的基础 URL，例如 https://gateway.example/v1；新条目默认为 DeepSeek 的 OpenAI 兼容端点。 */
	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	FString BaseUrl = ShuttlegentProviderConstant::DeepSeekOpenAiBaseUrl;

	/** 该路由的密钥，只保存在本机设置中；运行时以 SHUTTLEGENT_<ROUTE>_API_KEY 接收。 */
	UPROPERTY(Config, EditAnywhere, Category = "Provider", meta = (PasswordField = true))
	FString ApiKey;

	/** 该路由提供的模型。 */
	UPROPERTY(Config, EditAnywhere, Category = "Provider")
	TArray<FShuttlegentProviderModel> Models;
};
