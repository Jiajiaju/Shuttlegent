#include "Tool/FShuttlegentToolArguments.h"

#include "Dom/JsonValue.h"

#include "ShuttlegentMcpServerConstants.h"

FString FShuttlegentToolArguments::GetString(const TSharedRef<FJsonObject>& Arguments, const FString& Name, const FString& Default)
{
	FString Value;
	return Arguments->TryGetStringField(Name, Value) ? Value : Default;
}

int32 FShuttlegentToolArguments::GetInteger(const TSharedRef<FJsonObject>& Arguments, const FString& Name, int32 Default)
{
	double Value = 0.0;
	return Arguments->TryGetNumberField(Name, Value) ? FMath::FloorToInt32(Value) : Default;
}

bool FShuttlegentToolArguments::GetBoolean(const TSharedRef<FJsonObject>& Arguments, const FString& Name, bool bDefault)
{
	bool bValue = false;
	return Arguments->TryGetBoolField(Name, bValue) ? bValue : bDefault;
}

TArray<FString> FShuttlegentToolArguments::GetStringArray(const TSharedRef<FJsonObject>& Arguments, const FString& Name)
{
	TArray<FString> Values;
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Arguments->TryGetArrayField(Name, Items)) return Values;

	for (const TSharedPtr<FJsonValue>& Item : *Items)
	{
		FString Value;
		if (Item.IsValid() && Item->TryGetString(Value) && !Value.IsEmpty()) Values.Add(Value);
	}
	return Values;
}

bool FShuttlegentToolArguments::GetNumberTuple(const TSharedRef<FJsonObject>& Arguments, const FString& Name, int32 Length, TArray<double>& OutValues)
{
	OutValues.Reset();
	const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
	if (!Arguments->TryGetArrayField(Name, Items) || Items->Num() != Length) return false;

	for (const TSharedPtr<FJsonValue>& Item : *Items)
	{
		double Value = 0.0;
		if (!Item.IsValid() || !Item->TryGetNumber(Value)) return false;
		OutValues.Add(Value);
	}
	return true;
}

void FShuttlegentToolArguments::GetPage(const TSharedRef<FJsonObject>& Arguments, int32& OutOffset, int32& OutLimit)
{
	OutOffset = FMath::Max(0, GetInteger(Arguments, ShuttlegentMcpTool::ArgOffset, 0));
	OutLimit = FMath::Clamp(GetInteger(Arguments, ShuttlegentMcpTool::ArgLimit, ShuttlegentMcpTool::DefaultPageLimit), 1, ShuttlegentMcpTool::MaxPageLimit);
}

FString FShuttlegentToolArguments::MakePageHeader(int32 Offset, int32 Count, int32 Total)
{
	if (Count <= 0) return FString::Printf(TEXT("No items at offset %d (total %d)."), Offset, Total);

	const int32 Next = Offset + Count;
	const FString Range = FString::Printf(TEXT("Items %d-%d of %d."), Offset + 1, Next, Total);
	return Next < Total ? FString::Printf(TEXT("%s Next page: offset=%d."), *Range, Next) : Range;
}
