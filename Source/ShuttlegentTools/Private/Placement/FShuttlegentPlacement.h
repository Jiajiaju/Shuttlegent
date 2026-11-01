#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
class FShuttlegentToolSchema;
class UWorld;
struct FShuttlegentPlacementPattern;

/** place_actors 与 arrange_actors 共用的摆放计算：只算变换，不生成也不移动任何东西。仅限游戏线程。 */
struct FShuttlegentPlacement
{
	/** 单次调用最多摆放或排列的 Actor 数。 */
	static int32 GetMaxActors();

	/** 向工具参数表加入摆放方式、随机化与贴地参数；bForPlace 时 count 参与决定个数。 */
	static void AddArguments(FShuttlegentToolSchema& Schema, bool bForPlace);

	/** 读取摆放方式与通用选项；未给 origin 时用 DefaultOrigin。未给 seed 时由全部参数算出固定种子，预览与执行因而一致。 */
	static bool Parse(const TSharedRef<FJsonObject>& Arguments, const FVector& DefaultOrigin, FShuttlegentPlacementPattern& OutPattern, FString& OutError);

	/** 只读取随机化、贴地与种子选项，供不需要摆放方式的操作使用。 */
	static void ParseOptions(const TSharedRef<FJsonObject>& Arguments, FShuttlegentPlacementPattern& OutPattern);

	/** 按摆放方式算出变换（缩放为 1）；Count 为 INDEX_NONE 时由参数推出个数。bOutOriented 表示朝向由摆放方式给定。 */
	static bool Compute(const FShuttlegentPlacementPattern& Pattern, int32 Count, TArray<FTransform>& OutTransforms, bool& bOutOriented, FString& OutError);

	/** 叠加随机偏航与随机缩放倍数。 */
	static void Randomize(const FShuttlegentPlacementPattern& Pattern, TArray<FTransform>& InOutTransforms);

	/** 向下做射线检测落到地面；BottomOffsets 为各自轴心高出自身底部的距离（未缩放）。返回没打到地面的个数。 */
	static int32 SnapToGround(UWorld* World, TArray<FTransform>& InOutTransforms, const TArray<double>& BottomOffsets, const TArray<const AActor*>& Ignored);

	/** 确认卡上的变换文字。 */
	static FString Describe(const FTransform& Transform);
};
