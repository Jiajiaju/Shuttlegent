#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentModelEfforts.generated.h"

/**
 * 一个模型可选的推理强度档位，测试连接时从 models.opencode.ai 的模型目录查得（FShuttlegentEffortCatalog）。
 * 只记录查到了具体档位的模型；没有记录的模型不声明档位，请求不带推理参数，沿用服务方的默认行为。
 */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentModelEfforts
{
	GENERATED_BODY()

	/** 模型 ID。 */
	UPROPERTY(Config)
	FString ModelId;

	/** 可选档位，取运行时的档位名（minimal、low、medium、high、xhigh、max），从低到高。 */
	UPROPERTY(Config)
	TArray<FString> Efforts;
};
