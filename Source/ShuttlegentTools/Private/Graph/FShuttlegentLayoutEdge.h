#pragma once

#include "CoreMinimal.h"

/** 两个节点之间的一条连线：从 Source 的输出引脚连到 Target 的输入引脚。 */
struct FShuttlegentLayoutEdge
{
	/** 节点在 FShuttlegentLayoutGraph::Nodes 中的下标。 */
	int32 Source = INDEX_NONE;
	int32 Target = INDEX_NONE;

	/** 引脚在所属节点同向引脚中的相对位置：0 为最上，1 为最下。 */
	float SourcePin = 0.0f;
	float TargetPin = 0.0f;
};
