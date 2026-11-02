#pragma once

#include "CoreMinimal.h"

/** 用户在选择卡上提交的内容。 */
struct FShuttlegentChoiceAnswer
{
	/** 选中项的 id，按卡片上的顺序。 */
	TArray<FString> Selected;

	/** 选项 id 到该项评语；没写评语的项不在其中。 */
	TMap<FString, FString> ItemNotes;

	FString OverallNote;
};
