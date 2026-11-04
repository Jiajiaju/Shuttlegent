#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentConnectionStatus.h"
#include "EShuttlegentProviderApi.h"

#include "FShuttlegentConnection.generated.h"

/**
 * 个人设置中的一条模型连接：只填地址与密钥，协议与可用模型由检测得出（FShuttlegentConnectionTester）。
 * 一条连接即运行时的一个路由，检测出的每个模型都出现在会话的模型菜单中。
 */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentConnection
{
	GENERATED_BODY()

	/** 服务地址，例如 https://api.openai.com/v1。内置 DeepSeek 连接的地址固定。 */
	UPROPERTY(Config, EditAnywhere, Category = "Connection")
	FString BaseUrl;

	/** 密钥，只保存在本机设置中。 */
	UPROPERTY(Config, EditAnywhere, Category = "Connection", meta = (PasswordField = true))
	FString ApiKey;

	/** 内置 DeepSeek 连接：由运行时 llm-deepseek 提供，地址、协议与模型固定，只需填密钥，不可删除。 */
	UPROPERTY(Config)
	bool bBuiltin = false;

	/** 检测出的接口协议。 */
	UPROPERTY(Config)
	EShuttlegentProviderApi Api = EShuttlegentProviderApi::OpenAiCompletions;

	/** 检测出的可用于对话的模型 ID，最新的在前；第一个即这条连接的默认模型。 */
	UPROPERTY(Config)
	TArray<FString> ModelIds;

	/** 最近一次检测的结论。 */
	UPROPERTY(Config)
	EShuttlegentConnectionStatus Status = EShuttlegentConnectionStatus::Untested;

	/** 最近一次检测时地址与密钥的指纹；与当前值不一致说明改过，需要重新检测。不含密钥原文。 */
	UPROPERTY(Config)
	FString TestedFingerprint;
};
