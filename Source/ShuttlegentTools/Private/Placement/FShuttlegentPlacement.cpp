#include "Placement/FShuttlegentPlacement.h"

#include "CollisionQueryParams.h"
#include "Components/SplineComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Math/RandomStream.h"
#include "Misc/Crc.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "FShuttlegentToolObjects.h"
#include "Placement/FShuttlegentPlacementPattern.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"

#define LOCTEXT_NAMESPACE "ShuttlegentPlacement"

namespace FShuttlegentPlacementPrivate
{
	// 参数名称
	inline const TCHAR* ArgPattern = TEXT("pattern");
	inline const TCHAR* ArgOrigin = TEXT("origin");
	inline const TCHAR* ArgEnd = TEXT("end");
	inline const TCHAR* ArgDirection = TEXT("direction");
	inline const TCHAR* ArgSpacing = TEXT("spacing");
	inline const TCHAR* ArgSpacingY = TEXT("spacing_y");
	inline const TCHAR* ArgCount = TEXT("count");
	inline const TCHAR* ArgRows = TEXT("rows");
	inline const TCHAR* ArgColumns = TEXT("columns");
	inline const TCHAR* ArgRadius = TEXT("radius");
	inline const TCHAR* ArgSpline = TEXT("spline");
	inline const TCHAR* ArgSideOffset = TEXT("side_offset");
	inline const TCHAR* ArgArea = TEXT("area");
	inline const TCHAR* ArgExtent = TEXT("extent");
	inline const TCHAR* ArgMinDistance = TEXT("min_distance");
	inline const TCHAR* ArgRotation = TEXT("rotation");
	inline const TCHAR* ArgFaceCenter = TEXT("face_center");
	inline const TCHAR* ArgFollowPath = TEXT("follow_path");
	inline const TCHAR* ArgRandomYaw = TEXT("random_yaw");
	inline const TCHAR* ArgRandomScale = TEXT("random_scale");
	inline const TCHAR* ArgSnapToGround = TEXT("snap_to_ground");
	inline const TCHAR* ArgSeed = TEXT("seed");

	// pattern 的取值，顺序与 EShuttlegentPlacementShape 中 None 之后的各项一致
	inline const TCHAR* ShapeNames[] = { TEXT("grid"), TEXT("line"), TEXT("circle"), TEXT("spline"), TEXT("scatter") };

	// 单次调用最多摆放或排列的 Actor 数；确认卡逐个列出
	inline constexpr int32 MaxActors = 500;

	// 坐标、方向与旋转参数的分量数；散布范围与随机缩放范围的分量数
	inline constexpr int32 VectorLength = 3;
	inline constexpr int32 PairLength = 2;

	// 散布时为每个点尝试的随机次数，用尽仍不满足最小间距即认为放不下
	inline constexpr int32 ScatterAttempts = 30;

	// 贴地射线从轴心上方多高开始、向下打多深（cm）
	inline constexpr double TraceUp = 10000.0;
	inline constexpr double TraceDown = 100000.0;

	bool ReadVector(const TSharedRef<FJsonObject>& Arguments, const TCHAR* Name, FVector& OutValue)
	{
		TArray<double> Values;
		if (!FShuttlegentToolArguments::GetNumberTuple(Arguments, Name, VectorLength, Values)) return false;

		OutValue = FVector(Values[0], Values[1], Values[2]);
		return true;
	}

	double ReadNumber(const TSharedRef<FJsonObject>& Arguments, const TCHAR* Name, double Default)
	{
		double Value = Default;
		return Arguments->TryGetNumberField(Name, Value) ? Value : Default;
	}

	/** 参数的固定哈希，作为未给 seed 时的随机种子。 */
	int32 HashArguments(const TSharedRef<FJsonObject>& Arguments)
	{
		FString Text;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text);
		FJsonSerializer::Serialize(Arguments, Writer);
		return static_cast<int32>(FCrc::StrCrc32(*Text));
	}

	/** 样条参数所指的样条组件：Actor 标签或路径（取其第一个样条组件），或组件路径。 */
	USplineComponent* ResolveSpline(const FString& Target)
	{
		UObject* Object = FShuttlegentToolObjects::ResolveObject(Target);
		USplineComponent* Component = Cast<USplineComponent>(Object);
		if (IsValid(Component)) return Component;

		const AActor* Actor = Cast<AActor>(Object);
		return IsValid(Actor) ? Actor->FindComponentByClass<USplineComponent>() : nullptr;
	}

	FRotator FacingYaw(const FVector& Direction)
	{
		return FRotator(0.0, Direction.Rotation().Yaw, 0.0);
	}

	/** 摆放方式给定的朝向叠加在基础朝向上。 */
	FRotator Orient(const FShuttlegentPlacementPattern& Pattern, const FRotator& PathRotation)
	{
		return (FQuat(PathRotation) * FQuat(Pattern.Rotation)).Rotator();
	}

	bool ComputeGrid(const FShuttlegentPlacementPattern& Pattern, int32 Count, TArray<FTransform>& Out, FString& OutError)
	{
		if (Pattern.Spacing <= 0.0)
		{
			OutError = TEXT("grid needs spacing (cm).");
			return false;
		}

		int32 Columns = Pattern.Columns;
		int32 Total = Count;
		if (Total == INDEX_NONE)
		{
			if (Pattern.Rows <= 0 || Pattern.Columns <= 0)
			{
				OutError = TEXT("grid needs rows and columns.");
				return false;
			}
			Total = Pattern.Rows * Pattern.Columns;
		}
		if (Columns <= 0) Columns = FMath::Max(1, FMath::CeilToInt32(FMath::Sqrt(static_cast<double>(Total))));

		const double SpacingY = Pattern.SpacingY > 0.0 ? Pattern.SpacingY : Pattern.Spacing;
		for (int32 Index = 0; Index < Total; ++Index)
		{
			const FVector Location = Pattern.Origin + FVector((Index % Columns) * Pattern.Spacing, (Index / Columns) * SpacingY, 0.0);
			Out.Add(FTransform(Pattern.Rotation, Location));
		}
		return true;
	}

	bool ComputeLine(const FShuttlegentPlacementPattern& Pattern, int32 Count, TArray<FTransform>& Out, FString& OutError)
	{
		const FVector Direction = (Pattern.bHasEnd ? Pattern.End - Pattern.Origin : Pattern.Direction).GetSafeNormal();
		const double Length = Pattern.bHasEnd ? FVector::Dist(Pattern.Origin, Pattern.End) : 0.0;
		if (Direction.IsNearlyZero())
		{
			OutError = TEXT("line needs an end point different from origin, or a direction.");
			return false;
		}

		int32 Total = Count != INDEX_NONE ? Count : Pattern.Count;
		if (Total <= 0 && Pattern.bHasEnd && Pattern.Spacing > 0.0) Total = FMath::FloorToInt32(Length / Pattern.Spacing) + 1;
		if (Total <= 0 || (!Pattern.bHasEnd && Pattern.Spacing <= 0.0))
		{
			OutError = TEXT("line needs end with count or spacing, or direction with count and spacing.");
			return false;
		}

		// 给了终点且没给间距时首尾对齐终点；否则按间距从起点排开
		const bool bEvenly = Pattern.bHasEnd && (Pattern.Spacing <= 0.0 || Count != INDEX_NONE || Pattern.Count > 0);
		const double Step = bEvenly ? (Total > 1 ? Length / (Total - 1) : 0.0) : Pattern.Spacing;
		const FRotator Rotation = Pattern.bFollowPath ? Orient(Pattern, FacingYaw(Direction)) : Pattern.Rotation;
		for (int32 Index = 0; Index < Total; ++Index)
		{
			Out.Add(FTransform(Rotation, Pattern.Origin + Direction * (Step * Index)));
		}
		return true;
	}

	bool ComputeCircle(const FShuttlegentPlacementPattern& Pattern, int32 Count, TArray<FTransform>& Out, FString& OutError)
	{
		const int32 Total = Count != INDEX_NONE ? Count : Pattern.Count;
		if (Pattern.Radius <= 0.0 || Total <= 0)
		{
			OutError = TEXT("circle needs radius (cm) and count.");
			return false;
		}

		for (int32 Index = 0; Index < Total; ++Index)
		{
			const double Angle = UE_TWO_PI * Index / Total;
			const FVector Offset(FMath::Cos(Angle) * Pattern.Radius, FMath::Sin(Angle) * Pattern.Radius, 0.0);
			FRotator Rotation = Pattern.Rotation;
			if (Pattern.bFaceCenter) Rotation = Orient(Pattern, FacingYaw(-Offset));
			else if (Pattern.bFollowPath) Rotation = Orient(Pattern, FacingYaw(FVector(-Offset.Y, Offset.X, 0.0)));
			Out.Add(FTransform(Rotation, Pattern.Origin + Offset));
		}
		return true;
	}

	bool ComputeSpline(const FShuttlegentPlacementPattern& Pattern, int32 Count, TArray<FTransform>& Out, FString& OutError)
	{
		const USplineComponent* Spline = Pattern.Spline.Get();
		if (!IsValid(Spline))
		{
			OutError = TEXT("spline needs an actor or component with a spline in the open level.");
			return false;
		}

		const double Length = Spline->GetSplineLength();
		int32 Total = Count != INDEX_NONE ? Count : Pattern.Count;
		if (Total <= 0 && Pattern.Spacing > 0.0) Total = FMath::FloorToInt32(Length / Pattern.Spacing) + 1;
		if (Total <= 0)
		{
			OutError = TEXT("spline needs count or spacing.");
			return false;
		}

		// 闭合样条首尾重合，均分时不在终点重复放一个
		const bool bEvenly = Count != INDEX_NONE || Pattern.Count > 0;
		const int32 Divisions = Spline->IsClosedLoop() ? Total : FMath::Max(Total - 1, 1);
		const double Step = bEvenly ? (Total > 1 ? Length / Divisions : 0.0) : Pattern.Spacing;
		for (int32 Index = 0; Index < Total; ++Index)
		{
			const float Distance = static_cast<float>(FMath::Min(Step * Index, Length));
			const FRotator PathRotation = Spline->GetRotationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
			const FVector Right = Spline->GetRightVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
			const FVector Location = Spline->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) + Right * Pattern.SideOffset;
			Out.Add(FTransform(Pattern.bFollowPath ? Orient(Pattern, PathRotation) : Pattern.Rotation, Location));
		}
		return true;
	}

	bool ComputeScatter(const FShuttlegentPlacementPattern& Pattern, int32 Count, TArray<FTransform>& Out, FString& OutError)
	{
		const int32 Total = Count != INDEX_NONE ? Count : Pattern.Count;
		if (Total <= 0 || !Pattern.Area.IsValid)
		{
			OutError = TEXT("scatter needs count and an area: an actor (area) or origin with extent [x, y] (cm, half sizes).");
			return false;
		}

		FRandomStream Stream(Pattern.Seed);
		TArray<FVector> Points;
		for (int32 Index = 0; Index < Total; ++Index)
		{
			bool bPlaced = false;
			for (int32 Attempt = 0; Attempt < ScatterAttempts && !bPlaced; ++Attempt)
			{
				const FVector Candidate(Stream.FRandRange(Pattern.Area.Min.X, Pattern.Area.Max.X), Stream.FRandRange(Pattern.Area.Min.Y, Pattern.Area.Max.Y), Pattern.Area.GetCenter().Z);
				const bool bTooClose = Points.ContainsByPredicate([&Candidate, &Pattern](const FVector& Point) { return FVector::Dist2D(Point, Candidate) < Pattern.MinDistance; });
				if (bTooClose) continue;

				Points.Add(Candidate);
				bPlaced = true;
			}
			if (bPlaced) continue;

			OutError = FString::Printf(TEXT("Only %d of %d fit in the area with min_distance %g; lower min_distance or count, or enlarge the area."), Points.Num(), Total, Pattern.MinDistance);
			return false;
		}

		for (const FVector& Point : Points)
		{
			Out.Add(FTransform(Pattern.Rotation, Point));
		}
		return true;
	}
}

int32 FShuttlegentPlacement::GetMaxActors()
{
	return FShuttlegentPlacementPrivate::MaxActors;
}

void FShuttlegentPlacement::AddArguments(FShuttlegentToolSchema& Schema, bool bForPlace)
{
	TArray<FString> Shapes;
	for (const TCHAR* Name : FShuttlegentPlacementPrivate::ShapeNames)
	{
		Shapes.Add(Name);
	}

	Schema.Enum(FShuttlegentPlacementPrivate::ArgPattern, bForPlace ? TEXT("How to lay the new actors out.") : TEXT("For operation pattern: the layout to move the targets into, in the order given."), Shapes, bForPlace)
		.NumberTuple(FShuttlegentPlacementPrivate::ArgOrigin, bForPlace ? TEXT("[x, y, z] cm: grid corner, line start, circle center or scatter center.") : TEXT("[x, y, z] cm: grid corner, line start, circle center or scatter center. Defaults to the first target's location."), FShuttlegentPlacementPrivate::VectorLength)
		.NumberTuple(FShuttlegentPlacementPrivate::ArgEnd, TEXT("line: [x, y, z] cm end point; without spacing the first and last actor sit on origin and end."), FShuttlegentPlacementPrivate::VectorLength)
		.NumberTuple(FShuttlegentPlacementPrivate::ArgDirection, TEXT("line without end: [x, y, z] direction from origin. Defaults to [1, 0, 0]."), FShuttlegentPlacementPrivate::VectorLength)
		.Number(FShuttlegentPlacementPrivate::ArgSpacing, TEXT("cm between neighbours: grid along X (and Y unless spacing_y), line, spline."))
		.Number(FShuttlegentPlacementPrivate::ArgSpacingY, TEXT("grid: cm between rows along Y. Defaults to spacing."))
		.Integer(FShuttlegentPlacementPrivate::ArgRows, bForPlace ? TEXT("grid: rows along Y.") : TEXT("Unused by arrange_actors; columns decides the rows."))
		.Integer(FShuttlegentPlacementPrivate::ArgColumns, bForPlace ? TEXT("grid: columns along X.") : TEXT("grid: columns along X. Defaults to a square-ish grid."))
		.Number(FShuttlegentPlacementPrivate::ArgRadius, TEXT("circle: radius in cm."))
		.String(FShuttlegentPlacementPrivate::ArgSpline, TEXT("spline: label or path of an actor with a spline component (or the component path)."))
		.Number(FShuttlegentPlacementPrivate::ArgSideOffset, TEXT("spline: cm to the right of the spline, negative to the left; call twice for both sides of a road."))
		.String(FShuttlegentPlacementPrivate::ArgArea, TEXT("scatter: label or path of an actor (e.g. a volume) whose bounds are the area."))
		.NumberTuple(FShuttlegentPlacementPrivate::ArgExtent, TEXT("scatter without area: [x, y] half sizes in cm around origin."), FShuttlegentPlacementPrivate::PairLength)
		.Number(FShuttlegentPlacementPrivate::ArgMinDistance, TEXT("scatter: least cm between any two actors. Defaults to 0."))
		.NumberTuple(FShuttlegentPlacementPrivate::ArgRotation, bForPlace ? TEXT("[pitch, yaw, roll] degrees for every actor. Defaults to [0, 0, 0].") : TEXT("[pitch, yaw, roll] degrees for every target; without it targets keep their own rotation unless face_center or follow_path."), FShuttlegentPlacementPrivate::VectorLength)
		.Boolean(FShuttlegentPlacementPrivate::ArgFaceCenter, TEXT("circle: turn each actor's front (+X) to the center."))
		.Boolean(FShuttlegentPlacementPrivate::ArgFollowPath, TEXT("line, circle, spline: turn each actor's front (+X) along the path."))
		.Number(FShuttlegentPlacementPrivate::ArgRandomYaw, TEXT("Random extra yaw within ± this many degrees."))
		.NumberTuple(FShuttlegentPlacementPrivate::ArgRandomScale, TEXT("[min, max] random uniform scale factor, e.g. [0.8, 1.2]."), FShuttlegentPlacementPrivate::PairLength)
		.Boolean(FShuttlegentPlacementPrivate::ArgSnapToGround, TEXT("Drop each actor straight down until its bottom rests on what is below. Defaults to false."))
		.Integer(FShuttlegentPlacementPrivate::ArgSeed, TEXT("Random seed for scatter and random_*; the same arguments give the same result."));
	if (bForPlace) Schema.Integer(FShuttlegentPlacementPrivate::ArgCount, TEXT("How many: line, circle, spline, scatter. Line and spline may use spacing instead."));
}

bool FShuttlegentPlacement::Parse(const TSharedRef<FJsonObject>& Arguments, const FVector& DefaultOrigin, FShuttlegentPlacementPattern& OutPattern, FString& OutError)
{
	ParseOptions(Arguments, OutPattern);

	const FString ShapeName = FShuttlegentToolArguments::GetString(Arguments, FShuttlegentPlacementPrivate::ArgPattern);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FShuttlegentPlacementPrivate::ShapeNames); ++Index)
	{
		if (ShapeName.Equals(FShuttlegentPlacementPrivate::ShapeNames[Index], ESearchCase::IgnoreCase)) OutPattern.Shape = static_cast<EShuttlegentPlacementShape>(Index + 1);
	}
	if (OutPattern.Shape == EShuttlegentPlacementShape::None)
	{
		OutError = TEXT("Give pattern: grid, line, circle, spline or scatter.");
		return false;
	}

	OutPattern.bHasOrigin = FShuttlegentPlacementPrivate::ReadVector(Arguments, FShuttlegentPlacementPrivate::ArgOrigin, OutPattern.Origin);
	if (!OutPattern.bHasOrigin) OutPattern.Origin = DefaultOrigin;
	OutPattern.bHasEnd = FShuttlegentPlacementPrivate::ReadVector(Arguments, FShuttlegentPlacementPrivate::ArgEnd, OutPattern.End);
	FShuttlegentPlacementPrivate::ReadVector(Arguments, FShuttlegentPlacementPrivate::ArgDirection, OutPattern.Direction);
	OutPattern.Spacing = FShuttlegentPlacementPrivate::ReadNumber(Arguments, FShuttlegentPlacementPrivate::ArgSpacing, 0.0);
	OutPattern.SpacingY = FShuttlegentPlacementPrivate::ReadNumber(Arguments, FShuttlegentPlacementPrivate::ArgSpacingY, 0.0);
	OutPattern.Count = FShuttlegentToolArguments::GetInteger(Arguments, FShuttlegentPlacementPrivate::ArgCount, 0);
	OutPattern.Rows = FShuttlegentToolArguments::GetInteger(Arguments, FShuttlegentPlacementPrivate::ArgRows, 0);
	OutPattern.Columns = FShuttlegentToolArguments::GetInteger(Arguments, FShuttlegentPlacementPrivate::ArgColumns, 0);
	OutPattern.Radius = FShuttlegentPlacementPrivate::ReadNumber(Arguments, FShuttlegentPlacementPrivate::ArgRadius, 0.0);
	OutPattern.SideOffset = FShuttlegentPlacementPrivate::ReadNumber(Arguments, FShuttlegentPlacementPrivate::ArgSideOffset, 0.0);
	OutPattern.MinDistance = FShuttlegentPlacementPrivate::ReadNumber(Arguments, FShuttlegentPlacementPrivate::ArgMinDistance, 0.0);
	OutPattern.bFaceCenter = FShuttlegentToolArguments::GetBoolean(Arguments, FShuttlegentPlacementPrivate::ArgFaceCenter, false);
	OutPattern.bFollowPath = FShuttlegentToolArguments::GetBoolean(Arguments, FShuttlegentPlacementPrivate::ArgFollowPath, false);

	FVector Rotation = FVector::ZeroVector;
	OutPattern.bHasRotation = FShuttlegentPlacementPrivate::ReadVector(Arguments, FShuttlegentPlacementPrivate::ArgRotation, Rotation);
	OutPattern.Rotation = FRotator(Rotation.X, Rotation.Y, Rotation.Z);

	const FString SplineTarget = FShuttlegentToolArguments::GetString(Arguments, FShuttlegentPlacementPrivate::ArgSpline);
	if (!SplineTarget.IsEmpty()) OutPattern.Spline = FShuttlegentPlacementPrivate::ResolveSpline(SplineTarget);
	if (OutPattern.Shape == EShuttlegentPlacementShape::Spline && !OutPattern.Spline.IsValid())
	{
		OutError = FString::Printf(TEXT("%s is not an actor or component with a spline in the open level."), *SplineTarget);
		return false;
	}

	// 散布区域：优先取 area 所指 Actor 的包围盒，否则用 origin 与 extent
	const FString AreaTarget = FShuttlegentToolArguments::GetString(Arguments, FShuttlegentPlacementPrivate::ArgArea);
	TArray<double> Extent;
	const AActor* AreaActor = AreaTarget.IsEmpty() ? nullptr : Cast<AActor>(FShuttlegentToolObjects::ResolveObject(AreaTarget));
	if (!AreaTarget.IsEmpty() && !IsValid(AreaActor))
	{
		OutError = FString::Printf(TEXT("%s is not an actor of the open level."), *AreaTarget);
		return false;
	}
	if (IsValid(AreaActor))
	{
		FVector Center = FVector::ZeroVector;
		FVector HalfSize = FVector::ZeroVector;
		AreaActor->GetActorBounds(false, Center, HalfSize);
		OutPattern.Area = FBox(Center - HalfSize, Center + HalfSize);
	}
	else if (FShuttlegentToolArguments::GetNumberTuple(Arguments, FShuttlegentPlacementPrivate::ArgExtent, FShuttlegentPlacementPrivate::PairLength, Extent))
	{
		const FVector HalfSize(FMath::Abs(Extent[0]), FMath::Abs(Extent[1]), 0.0);
		OutPattern.Area = FBox(OutPattern.Origin - HalfSize, OutPattern.Origin + HalfSize);
	}
	return true;
}

void FShuttlegentPlacement::ParseOptions(const TSharedRef<FJsonObject>& Arguments, FShuttlegentPlacementPattern& OutPattern)
{
	TArray<double> Scale;
	OutPattern.RandomYaw = FMath::Abs(FShuttlegentPlacementPrivate::ReadNumber(Arguments, FShuttlegentPlacementPrivate::ArgRandomYaw, 0.0));
	if (FShuttlegentToolArguments::GetNumberTuple(Arguments, FShuttlegentPlacementPrivate::ArgRandomScale, FShuttlegentPlacementPrivate::PairLength, Scale)) OutPattern.RandomScale = FVector2D(FMath::Min(Scale[0], Scale[1]), FMath::Max(Scale[0], Scale[1]));
	OutPattern.bSnapToGround = FShuttlegentToolArguments::GetBoolean(Arguments, FShuttlegentPlacementPrivate::ArgSnapToGround, false);
	OutPattern.Seed = FShuttlegentToolArguments::GetInteger(Arguments, FShuttlegentPlacementPrivate::ArgSeed, FShuttlegentPlacementPrivate::HashArguments(Arguments));
}

bool FShuttlegentPlacement::Compute(const FShuttlegentPlacementPattern& Pattern, int32 Count, TArray<FTransform>& OutTransforms, bool& bOutOriented, FString& OutError)
{
	OutTransforms.Reset();
	bOutOriented = Pattern.bHasRotation || Pattern.bFollowPath || (Pattern.bFaceCenter && Pattern.Shape == EShuttlegentPlacementShape::Circle);

	bool bComputed = false;
	switch (Pattern.Shape)
	{
	case EShuttlegentPlacementShape::Grid:
	{
		bComputed = FShuttlegentPlacementPrivate::ComputeGrid(Pattern, Count, OutTransforms, OutError);
		break;
	}
	case EShuttlegentPlacementShape::Line:
	{
		bComputed = FShuttlegentPlacementPrivate::ComputeLine(Pattern, Count, OutTransforms, OutError);
		break;
	}
	case EShuttlegentPlacementShape::Circle:
	{
		bComputed = FShuttlegentPlacementPrivate::ComputeCircle(Pattern, Count, OutTransforms, OutError);
		break;
	}
	case EShuttlegentPlacementShape::Spline:
	{
		bComputed = FShuttlegentPlacementPrivate::ComputeSpline(Pattern, Count, OutTransforms, OutError);
		break;
	}
	case EShuttlegentPlacementShape::Scatter:
	{
		bComputed = FShuttlegentPlacementPrivate::ComputeScatter(Pattern, Count, OutTransforms, OutError);
		break;
	}
	default:
	{
		OutError = TEXT("Give pattern: grid, line, circle, spline or scatter.");
		break;
	}
	}
	if (!bComputed) return false;

	if (OutTransforms.Num() > FShuttlegentPlacementPrivate::MaxActors)
	{
		OutError = FString::Printf(TEXT("That makes %d actors; at most %d per call."), OutTransforms.Num(), FShuttlegentPlacementPrivate::MaxActors);
		return false;
	}
	return true;
}

void FShuttlegentPlacement::Randomize(const FShuttlegentPlacementPattern& Pattern, TArray<FTransform>& InOutTransforms)
{
	const bool bScale = !FMath::IsNearlyEqual(Pattern.RandomScale.X, 1.0) || !FMath::IsNearlyEqual(Pattern.RandomScale.Y, 1.0);
	if (Pattern.RandomYaw <= 0.0 && !bScale) return;

	// 与散布用不同的流：散布点数变化时，各个 Actor 的随机朝向与缩放不跟着全部改变
	FRandomStream Stream(static_cast<int32>(HashCombine(static_cast<uint32>(Pattern.Seed), static_cast<uint32>(InOutTransforms.Num()))));
	for (FTransform& Transform : InOutTransforms)
	{
		const double Yaw = Pattern.RandomYaw > 0.0 ? Stream.FRandRange(-Pattern.RandomYaw, Pattern.RandomYaw) : 0.0;
		const double Factor = bScale ? Stream.FRandRange(Pattern.RandomScale.X, Pattern.RandomScale.Y) : 1.0;
		Transform.SetRotation(FQuat(FRotator(0.0, Yaw, 0.0)) * Transform.GetRotation());
		Transform.SetScale3D(Transform.GetScale3D() * Factor);
	}
}

int32 FShuttlegentPlacement::SnapToGround(UWorld* World, TArray<FTransform>& InOutTransforms, const TArray<double>& BottomOffsets, const TArray<const AActor*>& Ignored)
{
	if (!IsValid(World)) return InOutTransforms.Num();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(ShuttlegentSnapToGround), true);
	for (const AActor* Actor : Ignored)
	{
		if (IsValid(Actor)) Params.AddIgnoredActor(Actor);
	}

	int32 Missed = 0;
	for (int32 Index = 0; Index < InOutTransforms.Num(); ++Index)
	{
		FTransform& Transform = InOutTransforms[Index];
		const FVector Location = Transform.GetLocation();
		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Location + FVector(0.0, 0.0, FShuttlegentPlacementPrivate::TraceUp), Location - FVector(0.0, 0.0, FShuttlegentPlacementPrivate::TraceDown), ECC_Visibility, Params))
		{
			++Missed;
			continue;
		}

		const double Offset = BottomOffsets.IsValidIndex(Index) ? BottomOffsets[Index] * Transform.GetScale3D().Z : 0.0;
		Transform.SetLocation(FVector(Location.X, Location.Y, Hit.ImpactPoint.Z + Offset));
	}
	return Missed;
}

FString FShuttlegentPlacement::Describe(const FTransform& Transform)
{
	return FText::Format(LOCTEXT("Transform", "Location {0} Rotation {1} Scale {2}"), FText::FromString(Transform.GetLocation().ToCompactString()), FText::FromString(Transform.Rotator().ToCompactString()), FText::FromString(Transform.GetScale3D().ToCompactString())).ToString();
}

#undef LOCTEXT_NAMESPACE
