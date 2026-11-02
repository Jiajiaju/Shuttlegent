#pragma once

#include "CoreMinimal.h"

/** 选择卡（ask_choices）中的一项。 */
struct FShuttlegentChoiceOption
{
	/** Agent 给出的稳定 id，用户提交后按它回报选中项与评语。 */
	FString Id;

	FString Title;

	/** 一行说明；可为空。 */
	FString Description;

	/** 图片来源，与回答中的 ![](src) 相同：ue-asset:<对象路径>、Saved/Shuttlegent/ 下的本机图片或 http(s)；可为空。 */
	FString Image;
};
