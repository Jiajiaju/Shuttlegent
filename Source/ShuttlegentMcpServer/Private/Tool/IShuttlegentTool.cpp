#include "Tool/IShuttlegentTool.h"

bool IShuttlegentTool::IsReadOnlyCall(const TSharedRef<FJsonObject>& Arguments) const
{
	return GetDefinition().bReadOnly;
}

void IShuttlegentTool::RunAsync(const TSharedRef<FJsonObject>& Arguments, TFunction<void(const FShuttlegentToolResult&)> OnDone)
{
	OnDone(Run(Arguments));
}

FShuttlegentToolResult IShuttlegentTool::Run(const TSharedRef<FJsonObject>& Arguments)
{
	return FShuttlegentToolResult::Error(TEXT("This tool writes; it runs through confirmation."));
}

bool IShuttlegentTool::Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError)
{
	OutError = TEXT("This tool does not write.");
	return false;
}

void IShuttlegentTool::Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects)
{
}
