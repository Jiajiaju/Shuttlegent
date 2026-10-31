#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentProviderApi.generated.h"

/** 经 llm-pi-ai 接入的模型供应商路由所用的接口协议（Shuttlegent.md 第 9.1 节）。 */
UENUM()
enum class EShuttlegentProviderApi : uint8
{
	// OpenAI Chat Completions 及兼容端点：DashScope、Kimi、智谱、豆包、Ollama、vLLM、团队网关
	OpenAiCompletions,

	// OpenAI Responses API
	OpenAiResponses,

	// Anthropic Messages API 及兼容端点
	AnthropicMessages
};
