#pragma once

#include "CoreMinimal.h"

class IShuttlegentGraphAdapter;
class UObject;

/** 为资源找出可排版的节点图：RigVM 资源（Control Rig）、材质与材质函数、蓝图，以及其他持有 UEdGraph 的资源。 */
struct FShuttlegentGraphAdapters
{
	static TArray<TSharedRef<IShuttlegentGraphAdapter>> Collect(UObject* Asset);
};
