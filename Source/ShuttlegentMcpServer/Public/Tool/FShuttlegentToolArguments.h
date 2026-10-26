#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** Reads tool call arguments with defaults, and pages list results (Shuttlegent.md section 10.2). */
struct SHUTTLEGENTMCPSERVER_API FShuttlegentToolArguments
{
	static FString GetString(const TSharedRef<FJsonObject>& Arguments, const FString& Name, const FString& Default = FString());
	static int32 GetInteger(const TSharedRef<FJsonObject>& Arguments, const FString& Name, int32 Default);
	static bool GetBoolean(const TSharedRef<FJsonObject>& Arguments, const FString& Name, bool bDefault);
	static TArray<FString> GetStringArray(const TSharedRef<FJsonObject>& Arguments, const FString& Name);

	/** A fixed-length number array such as [x, y, z]; false when absent or of another length. */
	static bool GetNumberTuple(const TSharedRef<FJsonObject>& Arguments, const FString& Name, int32 Length, TArray<double>& OutValues);

	/** The "offset" and "limit" arguments of a paged tool, clamped to the page size limits. */
	static void GetPage(const TSharedRef<FJsonObject>& Arguments, int32& OutOffset, int32& OutLimit);

	/** First line of a paged answer; states exactly the range sent, and how to ask for the next page. */
	static FString MakePageHeader(int32 Offset, int32 Count, int32 Total);
};
