#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentProviderApi.h"
#include "FShuttlegentModelProbeResult.h"
#include "FShuttlegentModelProbeTarget.h"

/**
 * 检测一条模型连接：编辑器直接向服务发请求，识别协议并列出可用的模型，不依赖运行时。
 * 先按 OpenAI 的方式取 {地址}/models，再依次试 /responses 与 /chat/completions；不是 OpenAI 的方式时按 Anthropic 取 {地址}/v1/models。
 * 内置 DeepSeek 连接只验证密钥。每个请求与结论写进 Output Log；日志绝不记录密钥。仅限游戏线程。
 */
struct SHUTTLEGENTCORE_API FShuttlegentModelProbe
{
	DECLARE_DELEGATE_OneParam(FOnProbeDone, const FShuttlegentModelProbeResult& /*Result*/);

	/** 发出检测；缺少地址或密钥时不发请求、直接判为未连通。OnDone 在游戏线程上恰好调用一次。 */
	static void Detect(const FShuttlegentModelProbeTarget& Target, FOnProbeDone OnDone);

	/** 运行时向该地址、该协议发对话请求的完整地址；地址为空时返回空。 */
	static FString GetChatUrl(const FString& BaseUrl, EShuttlegentProviderApi Api, bool bBuiltin);
};
