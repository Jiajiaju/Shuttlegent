#pragma once

#include "CoreMinimal.h"

/** 随提问发送给运行时的一张图片。 */
struct FShuttlegentPromptImage
{
	/** image/png、image/jpeg、image/webp 或 image/gif。 */
	FString MimeType;

	/** 图片数据的 base64（不带 data: 前缀）。 */
	FString Data;
};
