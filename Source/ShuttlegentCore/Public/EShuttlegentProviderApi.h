#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentProviderApi.generated.h"

/** API protocol of a model provider route served through llm-pi-ai. */
UENUM()
enum class EShuttlegentProviderApi : uint8
{
	// OpenAI Chat Completions and compatible endpoints: DashScope, Kimi, Zhipu, Doubao, Ollama, vLLM, team gateways
	OpenAiCompletions,

	// OpenAI Responses API
	OpenAiResponses,

	// Anthropic Messages API and compatible endpoints
	AnthropicMessages
};
