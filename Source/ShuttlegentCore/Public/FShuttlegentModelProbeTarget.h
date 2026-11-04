#pragma once

#include "CoreMinimal.h"

/** 检测的对象：一条模型连接的地址与密钥。 */
struct FShuttlegentModelProbeTarget
{
	/** 服务地址；内置 DeepSeek 连接为固定地址。 */
	FString BaseUrl;

	/** 密钥；只在内存中使用，不写日志。 */
	FString ApiKey;

	/** 内置 DeepSeek 连接：协议与模型固定，只验证密钥。 */
	bool bBuiltin = false;
};
