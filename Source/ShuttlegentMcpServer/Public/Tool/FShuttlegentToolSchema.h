#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * Builds the JSON Schema of a tool's arguments object:
 * FShuttlegentToolSchema().String(TEXT("path"), TEXT("Object path"), true).Integer(TEXT("limit"), TEXT("Page size")).Build()
 */
class SHUTTLEGENTMCPSERVER_API FShuttlegentToolSchema
{
private:
	TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
	TArray<TSharedPtr<FJsonValue>> Required;

public:
	FShuttlegentToolSchema& String(const FString& Name, const FString& Description, bool bRequired = false);
	FShuttlegentToolSchema& Integer(const FString& Name, const FString& Description, bool bRequired = false);
	FShuttlegentToolSchema& Number(const FString& Name, const FString& Description, bool bRequired = false);
	FShuttlegentToolSchema& Boolean(const FString& Name, const FString& Description, bool bRequired = false);
	FShuttlegentToolSchema& StringArray(const FString& Name, const FString& Description, bool bRequired = false);

	/** A string that must be one of Values. */
	FShuttlegentToolSchema& Enum(const FString& Name, const FString& Description, const TArray<FString>& Values, bool bRequired = false);

	/** An array of numbers of a fixed length, such as a location [x, y, z]. */
	FShuttlegentToolSchema& NumberTuple(const FString& Name, const FString& Description, int32 Length, bool bRequired = false);

	TSharedPtr<FJsonObject> Build() const;

private:
	FShuttlegentToolSchema& Add(const FString& Name, const TSharedRef<FJsonObject>& Property, bool bRequired);
};
