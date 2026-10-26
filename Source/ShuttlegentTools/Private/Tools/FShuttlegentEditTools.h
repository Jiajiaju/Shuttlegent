#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/** Write tools on objects: set_property, spawn_actor, delete_actors (one undoable transaction each) and save_assets. */
struct FShuttlegentEditTools
{
	static void Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
