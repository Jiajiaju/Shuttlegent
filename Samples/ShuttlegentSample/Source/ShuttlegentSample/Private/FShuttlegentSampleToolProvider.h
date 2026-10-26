#pragma once

#include "CoreMinimal.h"

#include "Tool/IShuttlegentToolProvider.h"

/**
 * A project's own editor tools, offered to Shuttlegent through IShuttlegentToolProvider (Shuttlegent.md section 11):
 * sample_count_actors reads, sample_set_actor_label writes through the confirmation card in one undoable step.
 */
class FShuttlegentSampleToolProvider : public IShuttlegentToolProvider
{
private:
	TArray<TSharedRef<IShuttlegentTool>> Tools;

public:
	FShuttlegentSampleToolProvider();

	//~ Begin IShuttlegentToolProvider Interface.
	virtual void GetTools(TArray<TSharedRef<IShuttlegentTool>>& OutTools) const override;
	//~ End IShuttlegentToolProvider Interface.
};
