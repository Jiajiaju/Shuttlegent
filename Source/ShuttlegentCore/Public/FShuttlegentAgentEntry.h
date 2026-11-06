#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentAgentEntry.generated.h"

/** Personal settings for one agent; the agents themselves come from the plugin and are not entered by hand. */
USTRUCT()
struct SHUTTLEGENTCORE_API FShuttlegentAgentEntry
{
	GENERATED_BODY()

	/** Agent identifier, filled in by the plugin. */
	UPROPERTY(Config, VisibleAnywhere, Category = "Agent")
	FString AgentId;

	/** Whether the agent appears in the session's agent picker; the default agent is always enabled. */
	UPROPERTY(Config, EditAnywhere, Category = "Agent")
	bool bEnabled = true;

	/** Model selected together with this agent, chosen by model name; leave empty to use the default model under Defaults. */
	UPROPERTY(Config, EditAnywhere, Category = "Agent")
	FString DefaultModel;
};
