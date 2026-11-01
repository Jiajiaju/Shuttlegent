#pragma once

#include "CoreMinimal.h"

/** 批量摆放或重新排列 Actor 的方式。 */
enum class EShuttlegentPlacementShape : uint8
{
	// 未指定
	None,

	// 网格：沿 X 为列、沿 Y 为行
	Grid,

	// 直线：从起点到终点，或从起点沿方向按间距
	Line,

	// 圆周：绕中心等角度分布
	Circle,

	// 样条：沿关卡中某个样条组件
	Spline,

	// 散布：在区域内随机分布，保持最小间距
	Scatter
};
