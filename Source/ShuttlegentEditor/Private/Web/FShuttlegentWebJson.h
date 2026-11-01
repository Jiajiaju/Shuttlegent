#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;
struct FShuttlegentPromptImage;
struct FShuttlegentReference;

/** 面板宿主与协议共用的 JSON：页面所见形式的引用，以及紧凑序列化。均为纯函数。 */
struct FShuttlegentWebJson
{
	/** { kind, path, name, cls } */
	static TSharedPtr<FJsonValue> MakeReference(const FShuttlegentReference& Reference);
	static TArray<TSharedPtr<FJsonValue>> MakeReferences(const TArray<FShuttlegentReference>& References);

	/** 页面以 JSON 数组发送的引用；跳过没有路径的条目。 */
	static TArray<FShuttlegentReference> ParseReferences(const FString& Json);

	/** 页面以 JSON 数组发送的草稿图片 { mimeType, data }；跳过缺少任一字段的条目。 */
	static TArray<FShuttlegentPromptImage> ParseImages(const FString& Json);

	/** 本机文件的 file:/// 地址，供页面显示。 */
	static FString MakeFileUrl(const FString& Path);

	static FString ToJson(const TSharedRef<FJsonObject>& Object);
	static FString ToJson(const TArray<TSharedPtr<FJsonValue>>& Values);
};
