#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentProviderModel.generated.h"

/** 供应商路由提供的一个模型。 */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentProviderModel
{
	GENERATED_BODY()

	/** 端点要求的模型 ID，例如 gpt-4.1 或 qwen-max。 */
	UPROPERTY(Config, EditAnywhere, Category = "Model")
	FString Id;

	/** 在面板模型选择器中显示的名称；为空时显示模型 ID。 */
	UPROPERTY(Config, EditAnywhere, Category = "Model")
	FString Name;
};
