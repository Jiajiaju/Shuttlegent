#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentProviderApi.h"
#include "ShuttlegentConstants.h"

#include "FShuttlegentModelEntry.generated.h"

/**
 * 个人设置中的一个模型（ShuttlegentAgentModel_Design.md 第 6.1 节）：自带地址、协议与密钥。
 * 生成运行时补丁时，地址、协议、密钥相同的模型归为一个路由，路由对用户不可见。
 */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentModelEntry
{
	GENERATED_BODY()

	/** 会话中显示的名字，各模型之间不可重名；默认模型与 Agent 的默认模型按名字选择。 */
	UPROPERTY(Config, EditAnywhere, Category = "Model")
	FString Name;

	/** 发给服务方的模型 ID，例如 deepseek-chat。 */
	UPROPERTY(Config, EditAnywhere, Category = "Model")
	FString ModelId;

	/** 服务的 Base Url。 */
	UPROPERTY(Config, EditAnywhere, Category = "Model")
	FString BaseUrl = ShuttlegentProviderConstant::DeepSeekOpenAiBaseUrl;

	/** 接口协议，必须与地址实际提供的接口一致；不确定时点“检测连接”。 */
	UPROPERTY(Config, EditAnywhere, Category = "Model")
	EShuttlegentProviderApi Api = EShuttlegentProviderApi::OpenAiCompletions;

	/** 密钥，只保存在本机设置中；留空时沿用地址与协议相同的其他模型的密钥。 */
	UPROPERTY(Config, EditAnywhere, Category = "Model", meta = (PasswordField = true))
	FString ApiKey;

	/** 内置 DeepSeek 模型：由运行时 llm-deepseek 提供，协议与模型 ID 锁定，不可删除。 */
	UPROPERTY(Config)
	bool bBuiltin = false;

	/** 从旧版供应商设置迁移来的路由名；非空时沿用为路由名，旧会话记录的模型因此仍然有效。 */
	UPROPERTY(Config)
	FString LegacyRoute;
};
