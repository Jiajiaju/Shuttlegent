#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;
struct FShuttlegentReference;

/** 面板宿主与协议共用的 JSON：页面所见形式的引用，以及紧凑序列化。均为纯函数。 */
struct FShuttlegentWebJson
{
	/** { kind, path, name, cls } */
	static TSharedPtr<FJsonValue> MakeReference(const FShuttlegentReference& Reference);
	static TArray<TSharedPtr<FJsonValue>> MakeReferences(const TArray<FShuttlegentReference>& References);

	/** 页面以 JSON 数组发送的引用；跳过没有路径的条目。 */
	static TArray<FShuttlegentReference> ParseReferences(const FString& Json);

	static FString ToJson(const TSharedRef<FJsonObject>& Object);
	static FString ToJson(const TArray<TSharedPtr<FJsonValue>>& Values);
};
