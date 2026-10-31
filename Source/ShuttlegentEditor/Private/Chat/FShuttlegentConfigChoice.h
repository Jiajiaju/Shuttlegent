#pragma once

#include "CoreMinimal.h"

/** ACP 会话配置选项（模型、推理强度）的一个可选值。 */
struct FShuttlegentConfigChoice
{
	/** 随 session/set_config_option 回传的值。 */
	FString Value;

	/** 显示名称。 */
	FString Name;
};
