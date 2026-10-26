#pragma once

#include "CoreMinimal.h"

#include "Tool/IShuttlegentTool.h"

/** Every tool of every registered IShuttlegentToolProvider, by name. Names that are invalid or taken are skipped with a warning. */
class FShuttlegentToolRegistry
{
public:
	/** Collects the tools anew, in provider registration order. */
	static TArray<TSharedRef<IShuttlegentTool>> Collect();

	/** The tool of that name, or nullptr. */
	static TSharedPtr<IShuttlegentTool> Find(const FString& Name);
};
