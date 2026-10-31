#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * 构建工具参数对象的 JSON Schema：
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

	/** 任意结构的 JSON 对象，例如转交给另一个工具的参数。 */
	FShuttlegentToolSchema& Object(const FString& Name, const FString& Description, bool bRequired = false);

	/** 取值必须为 Values 之一的字符串。 */
	FShuttlegentToolSchema& Enum(const FString& Name, const FString& Description, const TArray<FString>& Values, bool bRequired = false);

	/** 定长数值数组，例如位置 [x, y, z]。 */
	FShuttlegentToolSchema& NumberTuple(const FString& Name, const FString& Description, int32 Length, bool bRequired = false);

	TSharedPtr<FJsonObject> Build() const;

private:
	FShuttlegentToolSchema& Add(const FString& Name, const TSharedRef<FJsonObject>& Property, bool bRequired);
};
