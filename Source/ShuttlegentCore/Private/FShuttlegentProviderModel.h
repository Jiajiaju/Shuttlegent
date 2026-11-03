#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentProviderModel.generated.h"

/** 旧版供应商路由中的一个模型，只供 FShuttlegentSettingsMigration 解析旧配置；字段与旧版一致，不得改动。 */
USTRUCT()
struct FShuttlegentProviderModel
{
	GENERATED_BODY()

	/** 模型 ID。 */
	UPROPERTY()
	FString Id;

	/** 显示名称；为空时显示模型 ID。 */
	UPROPERTY()
	FString Name;
};
