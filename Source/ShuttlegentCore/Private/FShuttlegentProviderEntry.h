#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentProviderApi.h"
#include "FShuttlegentProviderModel.h"

#include "FShuttlegentProviderEntry.generated.h"

/** 旧版个人设置中的一个供应商路由，只供 FShuttlegentSettingsMigration 解析旧配置；字段与旧版一致，不得改动。 */
USTRUCT()
struct FShuttlegentProviderEntry
{
	GENERATED_BODY()

	/** 路由 ID。 */
	UPROPERTY()
	FString Route;

	/** 显示名称。 */
	UPROPERTY()
	FString DisplayName;

	/** 接口协议。 */
	UPROPERTY()
	EShuttlegentProviderApi Api = EShuttlegentProviderApi::OpenAiCompletions;

	/** 端点的基础 URL。 */
	UPROPERTY()
	FString BaseUrl;

	/** 该路由的密钥。 */
	UPROPERTY()
	FString ApiKey;

	/** 该路由提供的模型。 */
	UPROPERTY()
	TArray<FShuttlegentProviderModel> Models;
};
