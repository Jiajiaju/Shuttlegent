#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;
struct FShuttlegentReference;

/** JSON shared by the panel host and the protocol: references as the page sees them, and condensed serialization. Pure functions. */
struct FShuttlegentWebJson
{
	/** { kind, path, name, cls } */
	static TSharedPtr<FJsonValue> MakeReference(const FShuttlegentReference& Reference);
	static TArray<TSharedPtr<FJsonValue>> MakeReferences(const TArray<FShuttlegentReference>& References);

	/** References the page sent as a JSON array; entries without a path are skipped. */
	static TArray<FShuttlegentReference> ParseReferences(const FString& Json);

	static FString ToJson(const TSharedRef<FJsonObject>& Object);
	static FString ToJson(const TArray<TSharedPtr<FJsonValue>>& Values);
};
