#pragma once

#include "CoreMinimal.h"

class FJsonValue;

/** 本机图片文件（拖入面板或从文件对话框选择）转为页面的草稿图片 { mimeType, data, name }。仅限游戏线程。 */
struct FShuttlegentImageFiles
{
	/** 扩展名为 PNG、JPEG、WebP 或 GIF 的文件。 */
	static bool IsImageFile(const FString& Path);

	/** 读出各图片文件；跳过不是图片、读不出或超过大小上限的文件。 */
	static TArray<TSharedPtr<FJsonValue>> Load(const TArray<FString>& Paths);

	/** 文件对话框的图片筛选串。 */
	static const TCHAR* GetDialogFilter();
};
