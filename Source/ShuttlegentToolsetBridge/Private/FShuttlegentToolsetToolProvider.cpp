#include "FShuttlegentToolsetToolProvider.h"

#include "FShuttlegentToolsetTools.h"

FShuttlegentToolsetToolProvider::FShuttlegentToolsetToolProvider()
{
	FShuttlegentToolsetTools::Append(Tools);
}

void FShuttlegentToolsetToolProvider::GetTools(TArray<TSharedRef<IShuttlegentTool>>& OutTools) const
{
	OutTools.Append(Tools);
}
