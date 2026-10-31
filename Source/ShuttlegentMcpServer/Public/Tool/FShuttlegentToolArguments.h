#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/** 按默认值读取工具调用参数，并对列表结果分页（Shuttlegent.md 第 10.2 节）。 */
struct SHUTTLEGENTMCPSERVER_API FShuttlegentToolArguments
{
	static FString GetString(const TSharedRef<FJsonObject>& Arguments, const FString& Name, const FString& Default = FString());
	static int32 GetInteger(const TSharedRef<FJsonObject>& Arguments, const FString& Name, int32 Default);
	static bool GetBoolean(const TSharedRef<FJsonObject>& Arguments, const FString& Name, bool bDefault);
	static TArray<FString> GetStringArray(const TSharedRef<FJsonObject>& Arguments, const FString& Name);

	/** 定长数值数组，例如 [x, y, z]；缺失或长度不符时返回 false。 */
	static bool GetNumberTuple(const TSharedRef<FJsonObject>& Arguments, const FString& Name, int32 Length, TArray<double>& OutValues);

	/** 分页工具的 "offset" 与 "limit" 参数，限制在分页大小的上下限之内。 */
	static void GetPage(const TSharedRef<FJsonObject>& Arguments, int32& OutOffset, int32& OutLimit);

	/** 分页答复的首行；准确说明本次发送的范围，以及如何请求下一页。 */
	static FString MakePageHeader(int32 Offset, int32 Count, int32 Total);
};
