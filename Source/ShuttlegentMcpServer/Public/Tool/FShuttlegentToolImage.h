#pragma once

#include "CoreMinimal.h"

/** 工具结果中的一张图片，以 MCP image 内容块返回给运行时。 */
struct FShuttlegentToolImage
{
	/** image/png、image/jpeg、image/webp 或 image/gif。 */
	FString MimeType;

	/** 图片数据的 base64。 */
	FString Data;
};
