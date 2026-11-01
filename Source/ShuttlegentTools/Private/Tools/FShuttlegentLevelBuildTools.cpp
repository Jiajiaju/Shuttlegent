#include "Tools/FShuttlegentLevelBuildTools.h"

#include "AssetSelection.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Subsystems/EditorActorSubsystem.h"

#include "FShuttlegentToolBase.h"
#include "FShuttlegentToolObjects.h"
#include "Placement/FShuttlegentPlacement.h"
#include "Placement/FShuttlegentPlacementPattern.h"
#include "ShuttlegentToolsConstants.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"
#include "WriteGate/FShuttlegentWriteRequest.h"

#define LOCTEXT_NAMESPACE "ShuttlegentLevelBuildTools"

namespace FShuttlegentLevelBuildToolsPrivate
{
	// 工具名称
	inline const TCHAR* PlaceActorsName = TEXT("place_actors");
	inline const TCHAR* ArrangeActorsName = TEXT("arrange_actors");

	// 参数名称
	inline const TCHAR* ArgSource = TEXT("source");
	inline const TCHAR* ArgLabel = TEXT("label");
	inline const TCHAR* ArgFolder = TEXT("folder");
	inline const TCHAR* ArgTargets = TEXT("targets");
	inline const TCHAR* ArgOperation = TEXT("operation");
	inline const TCHAR* ArgAxis = TEXT("axis");
	inline const TCHAR* ArgAlignTo = TEXT("align_to");
	inline const TCHAR* ArgValue = TEXT("value");

	// arrange_actors 的操作
	inline const TCHAR* OperationAlign = TEXT("align");
	inline const TCHAR* OperationDistribute = TEXT("distribute");
	inline const TCHAR* OperationSnap = TEXT("snap_to_ground");
	inline const TCHAR* OperationPattern = TEXT("pattern");
	inline const TCHAR* OperationRandomize = TEXT("randomize");

	// 轴的名称，顺序即分量下标
	inline const TCHAR* AxisNames[] = { TEXT("x"), TEXT("y"), TEXT("z") };

	// 对齐的基准
	inline const TCHAR* AlignMin = TEXT("min");
	inline const TCHAR* AlignMax = TEXT("max");
	inline const TCHAR* AlignCenter = TEXT("center");
	inline const TCHAR* AlignValue = TEXT("value");

	// 新 Actor 标签与序号之间的分隔符，如 SM_Rock_3
	inline const TCHAR* LabelSeparator = TEXT("_");

	// 变换是否变化的容差：位置（cm）、旋转与缩放
	inline constexpr double TransformTolerance = 0.01;

	/** place_actors 复制或生成的来源，三者取其一。 */
	struct FPlaceSource
	{
		TWeakObjectPtr<AActor> Actor = nullptr;
		TWeakObjectPtr<UObject> Asset = nullptr;
		TWeakObjectPtr<UClass> Class = nullptr;
		FString Path;
		FString Prefix;

		/** 轴心高出自身底部的距离（未缩放），供贴地使用；无法得知时为 0。 */
		double BottomOffset = 0.0;
	};

	double BottomOffsetOf(const AActor* Actor)
	{
		FVector Origin = FVector::ZeroVector;
		FVector Extent = FVector::ZeroVector;
		Actor->GetActorBounds(false, Origin, Extent);
		const double ScaleZ = FMath::Max(Actor->GetActorScale3D().Z, UE_KINDA_SMALL_NUMBER);
		return (Actor->GetActorLocation().Z - (Origin.Z - Extent.Z)) / ScaleZ;
	}

	bool ResolveSource(const FString& Source, FPlaceSource& OutSource, FString& OutError)
	{
		UWorld* World = FShuttlegentToolObjects::GetEditorWorld();
		UObject* Object = FShuttlegentToolObjects::ResolveAsset(Source);
		AActor* Actor = Cast<AActor>(Object);
		UClass* Class = Cast<UClass>(Object);
		if (!IsValid(World))
		{
			OutError = TEXT("No level is open in the editor.");
			return false;
		}

		if (IsValid(Actor) && Actor->GetWorld() == World)
		{
			OutSource.Actor = Actor;
			OutSource.Path = Actor->GetPathName();
			OutSource.Prefix = Actor->GetActorLabel();
			OutSource.BottomOffset = BottomOffsetOf(Actor);
			return true;
		}

		if (IsValid(Object) && !IsValid(Class) && FActorFactoryAssetProxy::GetFactoryForAssetObject(Object) != nullptr)
		{
			const UStaticMesh* Mesh = Cast<UStaticMesh>(Object);
			OutSource.Asset = Object;
			OutSource.Path = Object->GetPathName();
			OutSource.Prefix = Object->GetName();
			if (IsValid(Mesh)) OutSource.BottomOffset = -Mesh->GetBoundingBox().Min.Z;
			return true;
		}

		if (!IsValid(Class)) Class = FShuttlegentToolObjects::ResolveClass(Source);
		if (IsValid(Class) && Class->IsChildOf(AActor::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract))
		{
			OutSource.Class = Class;
			OutSource.Path = Class->GetPathName();
			OutSource.Prefix = Class->GetName();
			OutSource.Prefix.RemoveFromEnd(ShuttlegentToolsConstant::GeneratedClassSuffix);
			return true;
		}

		OutError = FString::Printf(TEXT("%s is not a placeable asset (static mesh, Blueprint), actor class or actor of the open level."), *Source);
		return false;
	}

	/** place_actors 的来源与全部新 Actor 的变换；OutMissed 为贴地时下方没有地面的个数。 */
	bool PlanPlacement(const TSharedRef<FJsonObject>& Arguments, FPlaceSource& OutSource, TArray<FTransform>& OutTransforms, int32& OutMissed, FString& OutError)
	{
		if (!ResolveSource(FShuttlegentToolArguments::GetString(Arguments, ArgSource), OutSource, OutError)) return false;

		FShuttlegentPlacementPattern Pattern;
		bool bOriented = false;
		if (!FShuttlegentPlacement::Parse(Arguments, FVector::ZeroVector, Pattern, OutError)) return false;
		if (!FShuttlegentPlacement::Compute(Pattern, INDEX_NONE, OutTransforms, bOriented, OutError)) return false;
		if (OutTransforms.IsEmpty())
		{
			OutError = TEXT("The layout makes no actors.");
			return false;
		}

		FShuttlegentPlacement::Randomize(Pattern, OutTransforms);

		// 复制已有 Actor 时保留它自身的缩放
		const AActor* SourceActor = OutSource.Actor.Get();
		if (IsValid(SourceActor))
		{
			for (FTransform& Transform : OutTransforms)
			{
				Transform.SetScale3D(Transform.GetScale3D() * SourceActor->GetActorScale3D());
			}
		}

		OutMissed = 0;
		if (Pattern.bSnapToGround)
		{
			TArray<double> Offsets;
			Offsets.Init(OutSource.BottomOffset, OutTransforms.Num());
			OutMissed = FShuttlegentPlacement::SnapToGround(FShuttlegentToolObjects::GetEditorWorld(), OutTransforms, Offsets, { SourceActor });
		}
		return true;
	}

	FString MakeLabel(const TSharedRef<FJsonObject>& Arguments, const FPlaceSource& Source, int32 Index)
	{
		const FString Prefix = FShuttlegentToolArguments::GetString(Arguments, ArgLabel, Source.Prefix);
		return Prefix + LabelSeparator + FString::FromInt(Index + 1);
	}

	int32 FindAxis(const FString& Name)
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(AxisNames); ++Index)
		{
			if (Name.Equals(AxisNames[Index], ESearchCase::IgnoreCase)) return Index;
		}
		return INDEX_NONE;
	}

	/** arrange_actors 的目标与各自的计划变换（顺序与 targets 一致）。 */
	bool PlanArrangement(const TSharedRef<FJsonObject>& Arguments, TArray<AActor*>& OutActors, TArray<FTransform>& OutPlanned, FString& OutError)
	{
		UWorld* World = FShuttlegentToolObjects::GetEditorWorld();
		for (const FString& Target : FShuttlegentToolArguments::GetStringArray(Arguments, ArgTargets))
		{
			AActor* Actor = Cast<AActor>(FShuttlegentToolObjects::ResolveObject(Target));
			if (!IsValid(Actor) || Actor->GetWorld() != World)
			{
				OutError = FString::Printf(TEXT("%s is not an actor of the open level; nothing was moved."), *Target);
				return false;
			}
			OutActors.AddUnique(Actor);
		}
		if (OutActors.IsEmpty())
		{
			OutError = TEXT("Give targets.");
			return false;
		}
		if (OutActors.Num() > FShuttlegentPlacement::GetMaxActors())
		{
			OutError = FString::Printf(TEXT("At most %d actors per call."), FShuttlegentPlacement::GetMaxActors());
			return false;
		}

		for (const AActor* Actor : OutActors)
		{
			OutPlanned.Add(Actor->GetActorTransform());
		}

		const FString Operation = FShuttlegentToolArguments::GetString(Arguments, ArgOperation);
		const int32 Axis = FindAxis(FShuttlegentToolArguments::GetString(Arguments, ArgAxis));
		FShuttlegentPlacementPattern Pattern;
		FShuttlegentPlacement::ParseOptions(Arguments, Pattern);

		if (Operation.Equals(OperationAlign, ESearchCase::IgnoreCase) || Operation.Equals(OperationDistribute, ESearchCase::IgnoreCase))
		{
			if (Axis == INDEX_NONE)
			{
				OutError = TEXT("align and distribute need axis: x, y or z.");
				return false;
			}

			TArray<int32> Order;
			double Min = TNumericLimits<double>::Max();
			double Max = -TNumericLimits<double>::Max();
			for (int32 Index = 0; Index < OutPlanned.Num(); ++Index)
			{
				const double Coordinate = OutPlanned[Index].GetLocation()[Axis];
				Min = FMath::Min(Min, Coordinate);
				Max = FMath::Max(Max, Coordinate);
				Order.Add(Index);
			}

			if (Operation.Equals(OperationAlign, ESearchCase::IgnoreCase))
			{
				const FString AlignTo = FShuttlegentToolArguments::GetString(Arguments, ArgAlignTo, AlignCenter);
				double Value = (Min + Max) * 0.5;
				if (AlignTo.Equals(AlignMin, ESearchCase::IgnoreCase)) Value = Min;
				else if (AlignTo.Equals(AlignMax, ESearchCase::IgnoreCase)) Value = Max;
				else if (AlignTo.Equals(AlignValue, ESearchCase::IgnoreCase) && !Arguments->TryGetNumberField(ArgValue, Value))
				{
					OutError = TEXT("align_to value needs value (cm).");
					return false;
				}

				for (FTransform& Transform : OutPlanned)
				{
					FVector Location = Transform.GetLocation();
					Location[Axis] = Value;
					Transform.SetLocation(Location);
				}
			}
			else
			{
				// 两端不动，中间按原先在该轴上的先后次序等距排开
				Order.StableSort([&OutPlanned, Axis](int32 Left, int32 Right) { return OutPlanned[Left].GetLocation()[Axis] < OutPlanned[Right].GetLocation()[Axis]; });
				const double Step = Order.Num() > 1 ? (Max - Min) / (Order.Num() - 1) : 0.0;
				for (int32 Rank = 0; Rank < Order.Num(); ++Rank)
				{
					FVector Location = OutPlanned[Order[Rank]].GetLocation();
					Location[Axis] = Min + Step * Rank;
					OutPlanned[Order[Rank]].SetLocation(Location);
				}
			}
		}
		else if (Operation.Equals(OperationPattern, ESearchCase::IgnoreCase))
		{
			TArray<FTransform> Layout;
			bool bOriented = false;
			if (!FShuttlegentPlacement::Parse(Arguments, OutPlanned[0].GetLocation(), Pattern, OutError)) return false;
			if (!FShuttlegentPlacement::Compute(Pattern, OutPlanned.Num(), Layout, bOriented, OutError)) return false;

			for (int32 Index = 0; Index < OutPlanned.Num() && Index < Layout.Num(); ++Index)
			{
				OutPlanned[Index].SetLocation(Layout[Index].GetLocation());
				if (bOriented) OutPlanned[Index].SetRotation(Layout[Index].GetRotation());
			}
			FShuttlegentPlacement::Randomize(Pattern, OutPlanned);
		}
		else if (Operation.Equals(OperationRandomize, ESearchCase::IgnoreCase))
		{
			FShuttlegentPlacement::Randomize(Pattern, OutPlanned);
		}
		else if (!Operation.Equals(OperationSnap, ESearchCase::IgnoreCase))
		{
			OutError = TEXT("Give operation: align, distribute, snap_to_ground, pattern or randomize.");
			return false;
		}

		if (Operation.Equals(OperationSnap, ESearchCase::IgnoreCase) || Pattern.bSnapToGround)
		{
			TArray<double> Offsets;
			TArray<const AActor*> Ignored;
			for (const AActor* Actor : OutActors)
			{
				Offsets.Add(BottomOffsetOf(Actor));
				Ignored.Add(Actor);
			}
			FShuttlegentPlacement::SnapToGround(World, OutPlanned, Offsets, Ignored);
		}
		return true;
	}

	class FPlaceActors : public FShuttlegentToolBase
	{
	public:
		FPlaceActors()
		{
			Definition.Name = PlaceActorsName;
			Definition.Description = TEXT("Place many new actors in the open level in one step: copies of a static mesh, Blueprint or actor class, or duplicates of an actor already in the level (keeping its settings). Layouts: grid (rows x columns), line (origin to end, or along a direction), circle (around a center), spline (along a spline actor, optionally to one side), scatter (random within an area, keeping min_distance). Optional random yaw and scale, dropping onto the ground, an Outliner folder and a label. Up to 500 per call; the user confirms one card listing every new actor's transform; it runs as one undoable step. Use this instead of calling spawn_actor many times.");
			Definition.bReadOnly = false;

			FShuttlegentToolSchema Schema;
			Schema.String(ArgSource, TEXT("What to place: asset path (/Game/Props/SM_Rock, a Blueprint), actor class name (PointLight), or the label of an actor in the open level to duplicate."), true)
				.String(ArgLabel, TEXT("Label for the new actors, numbered _1, _2 ... Defaults to the source name."))
				.String(ArgFolder, TEXT("Outliner folder for the new actors, e.g. Props/Rocks."));
			FShuttlegentPlacement::AddArguments(Schema, true);
			Definition.InputSchema = Schema.Build();
		}

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			FPlaceSource Source;
			TArray<FTransform> Transforms;
			int32 Missed = 0;
			if (!PlanPlacement(Arguments, Source, Transforms, Missed, OutError)) return false;

			const FText Action = FText::Format(LOCTEXT("PlaceAction", "批量摆放 {0} 个 {1}"), FText::AsNumber(Transforms.Num()), FText::FromString(Source.Prefix));
			OutRequest.Action = Missed > 0 ? FText::Format(LOCTEXT("PlaceActionMissed", "{0}（{1} 个下方没有地面，保持原高度）"), Action, FText::AsNumber(Missed)).ToString() : Action.ToString();
			OutRequest.Level = FShuttlegentToolObjects::GetEditorWorld()->GetMapName();
			for (int32 Index = 0; Index < Transforms.Num(); ++Index)
			{
				FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
				Item.Name = MakeLabel(Arguments, Source, Index);
				Item.Path = Source.Path;
				Item.Field = LOCTEXT("TransformField", "位置 / 旋转 / 缩放").ToString();
				Item.CurrentValue = LOCTEXT("NotPlaced", "（新建）").ToString();
				Item.PlannedValue = FShuttlegentPlacement::Describe(Transforms[Index]);
			}
			return true;
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			FPlaceSource Source;
			TArray<FTransform> Transforms;
			int32 Missed = 0;
			FString Error;
			const bool bPlanned = PlanPlacement(Arguments, Source, Transforms, Missed, Error);
			UWorld* World = FShuttlegentToolObjects::GetEditorWorld();
			UEditorActorSubsystem* Subsystem = GEditor != nullptr ? GEditor->GetEditorSubsystem<UEditorActorSubsystem>() : nullptr;
			const FString Folder = FShuttlegentToolArguments::GetString(Arguments, ArgFolder);

			for (int32 Index = 0; Index < InOutObjects.Num(); ++Index)
			{
				FShuttlegentWriteObject& Item = InOutObjects[Index];
				if (!bPlanned || !Transforms.IsValidIndex(Index) || !IsValid(Subsystem) || !IsValid(World))
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = Error.IsEmpty() ? LOCTEXT("PlanChanged", "来源或关卡在确认期间已变化").ToString() : Error;
					continue;
				}

				const FTransform& Transform = Transforms[Index];
				AActor* NewActor = nullptr;
				if (Source.Actor.IsValid()) NewActor = Subsystem->DuplicateActor(Source.Actor.Get(), World, FVector::ZeroVector);
				else if (Source.Asset.IsValid()) NewActor = Subsystem->SpawnActorFromObject(Source.Asset.Get(), Transform.GetLocation(), Transform.Rotator());
				else if (Source.Class.IsValid()) NewActor = Subsystem->SpawnActorFromClass(Source.Class.Get(), Transform.GetLocation(), Transform.Rotator());
				if (!IsValid(NewActor))
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = LOCTEXT("PlaceFailed", "无法在当前关卡生成").ToString();
					continue;
				}

				NewActor->SetActorTransform(Transform);
				FActorLabelUtilities::SetActorLabelUnique(NewActor, Item.Name);
				if (!Folder.IsEmpty()) NewActor->SetFolderPath(FName(*Folder));
				Item.Name = NewActor->GetActorLabel();
				Item.Path = NewActor->GetPathName();
				Item.Status = EShuttlegentWriteObjectStatus::Completed;
			}
		}
	};

	class FArrangeActors : public FShuttlegentToolBase
	{
	public:
		FArrangeActors()
		{
			Definition.Name = ArrangeActorsName;
			Definition.Description = TEXT("Move existing actors of the open level in one step. operation: align (same coordinate on one axis: min, max, center or a value), distribute (spread evenly along one axis between the two outermost, keeping their order), snap_to_ground (drop each until its bottom rests on what is below), pattern (move them into a grid, line, circle, spline or scatter layout, in the order given; origin defaults to the first target), randomize (random yaw and scale). The user confirms one card listing each actor's current and planned transform; it runs as one undoable step. Use this instead of setting each actor's location with set_property.");
			Definition.bReadOnly = false;

			FShuttlegentToolSchema Schema;
			Schema.StringArray(ArgTargets, TEXT("Actor labels or paths in the open level; for pattern, their order is the layout order."), true)
				.Enum(ArgOperation, TEXT("What to do with the targets."), { OperationAlign, OperationDistribute, OperationSnap, OperationPattern, OperationRandomize }, true)
				.Enum(ArgAxis, TEXT("align, distribute: the world axis."), { AxisNames[0], AxisNames[1], AxisNames[2] })
				.Enum(ArgAlignTo, TEXT("align: line up on the smallest, largest or middle coordinate, or on value. Defaults to center."), { AlignMin, AlignMax, AlignCenter, AlignValue })
				.Number(ArgValue, TEXT("align_to value: the coordinate in cm."));
			FShuttlegentPlacement::AddArguments(Schema, false);
			Definition.InputSchema = Schema.Build();
		}

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			TArray<AActor*> Actors;
			TArray<FTransform> Planned;
			if (!PlanArrangement(Arguments, Actors, Planned, OutError)) return false;

			OutRequest.Action = FText::Format(LOCTEXT("ArrangeAction", "整理 Actor 位置（{0}）"), FText::FromString(FShuttlegentToolArguments::GetString(Arguments, ArgOperation))).ToString();
			for (int32 Index = 0; Index < Actors.Num(); ++Index)
			{
				const FTransform Current = Actors[Index]->GetActorTransform();
				if (Current.Equals(Planned[Index], TransformTolerance)) continue;

				FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
				Item.Name = Actors[Index]->GetActorLabel();
				Item.Path = Actors[Index]->GetPathName();
				Item.Field = LOCTEXT("ArrangeField", "位置 / 旋转 / 缩放").ToString();
				Item.CurrentValue = FShuttlegentPlacement::Describe(Current);
				Item.PlannedValue = FShuttlegentPlacement::Describe(Planned[Index]);
				if (OutRequest.Level.IsEmpty()) OutRequest.Level = FShuttlegentToolObjects::GetLevelName(Actors[Index]);
			}

			if (OutRequest.Objects.IsEmpty()) OutError = TEXT("Nothing would move: every target already has the planned transform.");
			return !OutRequest.Objects.IsEmpty();
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			TArray<AActor*> Actors;
			TArray<FTransform> Planned;
			FString Error;
			PlanArrangement(Arguments, Actors, Planned, Error);
			for (FShuttlegentWriteObject& Item : InOutObjects)
			{
				const int32 Index = Actors.IndexOfByPredicate([&Item](const AActor* Actor) { return IsValid(Actor) && Actor->GetPathName() == Item.Path; });
				if (!Planned.IsValidIndex(Index))
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = Error.IsEmpty() ? LOCTEXT("ActorGone", "Actor 已不存在").ToString() : Error;
					continue;
				}

				AActor* Actor = Actors[Index];
				Actor->Modify();
				Actor->SetActorTransform(Planned[Index]);
				Actor->PostEditMove(true);
				Item.Status = EShuttlegentWriteObjectStatus::Completed;
			}
		}
	};
}

void FShuttlegentLevelBuildTools::Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentLevelBuildToolsPrivate::FPlaceActors>());
	OutTools.Add(MakeShared<FShuttlegentLevelBuildToolsPrivate::FArrangeActors>());
}

#undef LOCTEXT_NAMESPACE
