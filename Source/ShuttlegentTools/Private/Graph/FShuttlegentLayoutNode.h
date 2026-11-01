#pragma once

#include "CoreMinimal.h"

/** 参与排版的一个节点，坐标与尺寸均为节点图坐标。 */
struct FShuttlegentLayoutNode
{
	/** 节点对象名，供 tidy_graph 的 nodes 参数匹配。 */
	FString Name;

	/** 节点标题，同样可用于匹配。 */
	FString Title;

	/** 左上角位置。 */
	FVector2D Position {};

	FVector2D Size {};

	/** 由本次排版移动；为 false 时原地不动，作为其他节点的参照。 */
	bool bArrange = true;
};
