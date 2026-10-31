#pragma once

#include "CoreMinimal.h"

struct FShuttlegentReference;

/**
 * 引用随提示传递的方式：以每个引用一行的块追加到文本之后，使运行时、
 * 对话记录与恢复的会话看到同一份固定快照。均为纯函数。
 */
struct FShuttlegentReferencePrompt
{
	/** 草稿文本后接引用块；没有引用时仅为文本本身。 */
	static FString Compose(const FString& Text, const TArray<FShuttlegentReference>& References);

	/** 将已发送的提示拆回其文本与引用块的各行（没有引用块时为空）。 */
	static void Split(const FString& Prompt, FString& OutText, TArray<FString>& OutReferenceLines);
};
