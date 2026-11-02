#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** 交互工具：ask_user，提出一个带备选答案的问题，由用户在面板中作答；ask_choices，在对话中放一张选择卡，用户的选择作为下一条消息到来。 */
struct FShuttlegentInteractionTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
