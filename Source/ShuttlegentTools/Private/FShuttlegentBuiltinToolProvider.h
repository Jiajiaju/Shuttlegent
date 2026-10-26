#pragma once

#include "CoreMinimal.h"

#include "Tool/IShuttlegentToolProvider.h"

class FShuttlegentLogCapture;

/** The general editor tools that hold for any UE project (Shuttlegent.md section 10.2). */
class FShuttlegentBuiltinToolProvider : public IShuttlegentToolProvider
{
private:
	TArray<TSharedRef<IShuttlegentTool>> Tools;

public:
	explicit FShuttlegentBuiltinToolProvider(const TSharedRef<FShuttlegentLogCapture>& LogCapture);

	//~ Begin IShuttlegentToolProvider Interface.
	virtual void GetTools(TArray<TSharedRef<IShuttlegentTool>>& OutTools) const override;
	//~ End IShuttlegentToolProvider Interface.
};
