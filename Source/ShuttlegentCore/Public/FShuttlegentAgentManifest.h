#pragma once

#include "CoreMinimal.h"

/** 一个可用的 Agent：默认循环，或插件目录下由说明文件声明的循环插件（ShuttlegentAgentModel_Design.md 第 9.1 节）。 */
struct FShuttlegentAgentManifest
{
	/** Agent 的标识，各 Agent 之间唯一。 */
	FString Id;

	/** 设置页与面板中显示的名称；默认 Agent 为空，由界面给出本地化名称。 */
	FString DisplayName;

	/** 一句话说明。 */
	FString Description;

	/** 声明的能力项（rewind、slashCommands、planMode、todo、contextUsage、memory、modelSwitch、imageInput）。 */
	TArray<FString> Capabilities;

	/** 建议的默认模型名称；Agent 第一次出现在设置中时作为它的默认模型。 */
	FString SuggestedModel;

	/** 循环插件入口模块的绝对路径；默认 Agent 由运行时自带，为空。 */
	FString EntryPath;

	/** 运行时自带的默认循环。 */
	bool bBuiltin = false;
};
