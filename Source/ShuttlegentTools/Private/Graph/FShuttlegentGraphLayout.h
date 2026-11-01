#pragma once

#include "CoreMinimal.h"

struct FShuttlegentLayoutGraph;

/** 通用节点图排版：数据从左往右分列，列内按引脚顺序排行，互不相连的部分上下叠放，孤立节点归到一侧。 */
struct FShuttlegentGraphLayout
{
	/** 就地修改 bArrange 为 true 的节点位置，以及框住这些节点的注释框。 */
	static void Arrange(FShuttlegentLayoutGraph& Graph);

	/** 节点包围盒，用于描述排版前后的范围；没有节点时为无效盒。 */
	static FBox2D GetBounds(const FShuttlegentLayoutGraph& Graph);
};
