#pragma once

#include "CoreMinimal.h"

#include "Placement/EShuttlegentPlacementShape.h"

class USplineComponent;

/** 从工具参数解析出的摆放方式；坐标为世界坐标（cm），角度为度。 */
struct FShuttlegentPlacementPattern
{
	EShuttlegentPlacementShape Shape = EShuttlegentPlacementShape::None;

	/** 网格的起始角、直线的起点、圆周的中心、散布的中心。 */
	FVector Origin {};
	bool bHasOrigin = false;

	/** 直线的终点；没有时从起点沿 Direction 按 Spacing 排开。 */
	FVector End {};
	bool bHasEnd = false;
	FVector Direction = FVector::ForwardVector;

	/** 相邻两个之间的距离；网格沿 X 用 Spacing、沿 Y 用 SpacingY。 */
	double Spacing = 0.0;
	double SpacingY = 0.0;

	/** 个数；0 表示由其他参数推出。 */
	int32 Count = 0;
	int32 Rows = 0;
	int32 Columns = 0;

	double Radius = 0.0;

	TWeakObjectPtr<USplineComponent> Spline = nullptr;

	/** 沿样条右侧的横向偏移，负值在左侧。 */
	double SideOffset = 0.0;

	/** 散布区域；无效时用 Origin 与 Extent 构成。 */
	FBox Area = FBox(ForceInit);
	double MinDistance = 0.0;

	/** 统一的基础朝向；没有给出时排列已有 Actor 会保留各自的朝向。 */
	FRotator Rotation {};
	bool bHasRotation = false;

	bool bFaceCenter = false;
	bool bFollowPath = false;

	/** 随机偏航的范围（±度）与随机缩放倍数的范围。 */
	double RandomYaw = 0.0;
	FVector2D RandomScale = FVector2D(1.0, 1.0);

	bool bSnapToGround = false;

	int32 Seed = 0;
};
