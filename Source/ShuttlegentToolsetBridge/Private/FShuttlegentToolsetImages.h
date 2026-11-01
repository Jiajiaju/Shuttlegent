#pragma once

#include "CoreMinimal.h"

struct FShuttlegentToolImage;

/** 从引擎工具结果中取出图片（FToolsetImage：MimeType 与 base64 Data），缩小过大的图片，并在原位置留下说明。 */
struct FShuttlegentToolsetImages
{
	/**
	 * InOutText 为引擎工具结果的 JSON 文本；其中每个图片对象被替换为一行说明（类型、尺寸、在本机保存的路径），图片本身加入 OutImages。
	 * 文本不是 JSON 或其中没有图片时原样保留。
	 */
	static void Extract(FString& InOutText, TArray<FShuttlegentToolImage>& OutImages);
};
