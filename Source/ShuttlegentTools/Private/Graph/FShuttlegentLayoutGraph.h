#pragma once

#include "CoreMinimal.h"

#include "Graph/FShuttlegentLayoutComment.h"
#include "Graph/FShuttlegentLayoutEdge.h"
#include "Graph/FShuttlegentLayoutNode.h"

/** 与图表类型无关的节点图：由适配器从具体节点图读出，排版后再由适配器写回。 */
struct FShuttlegentLayoutGraph
{
	TArray<FShuttlegentLayoutNode> Nodes;
	TArray<FShuttlegentLayoutEdge> Edges;
	TArray<FShuttlegentLayoutComment> Comments;
};
