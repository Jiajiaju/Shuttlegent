#include "WriteGate/FShuttlegentWriteProbe.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Containers/Ticker.h"
#include "ScopedTransaction.h"
#include "Selection.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "ShuttlegentLog.h"
#include "ShuttlegentMcpServerConstants.h"
#include "WriteGate/FShuttlegentWriteGate.h"

#define LOCTEXT_NAMESPACE "ShuttlegentWriteProbe"

namespace FShuttlegentWriteProbePrivate
{
	FString FormatZ(double Value)
	{
		FNumberFormattingOptions Options;
		Options.MinimumFractionalDigits = ShuttlegentWriteProbe::ValueFractionalDigits;
		Options.MaximumFractionalDigits = ShuttlegentWriteProbe::ValueFractionalDigits;
		Options.UseGrouping = false;
		return FText::AsNumber(Value, &Options).ToString();
	}

	FString MakeRawArguments(float ZOffset, int32 FailFrom, float Delay, const TArray<TWeakObjectPtr<AActor>>& Actors)
	{
		TArray<TSharedPtr<FJsonValue>> Paths;
		for (const TWeakObjectPtr<AActor>& Actor : Actors)
		{
			if (Actor.IsValid()) Paths.Add(MakeShared<FJsonValueString>(Actor->GetPathName()));
		}

		const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetStringField(ShuttlegentWriteProbe::FieldTool, ShuttlegentWriteProbe::ToolName);
		Root->SetNumberField(ShuttlegentWriteProbe::FieldZOffset, ZOffset);
		Root->SetNumberField(ShuttlegentWriteProbe::FieldFailFrom, FailFrom);
		Root->SetNumberField(ShuttlegentWriteProbe::FieldDelay, Delay);
		Root->SetArrayField(ShuttlegentWriteProbe::FieldActors, Paths);

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		return Json;
	}
}

FShuttlegentWriteProbe::FShuttlegentWriteProbe(const TSharedRef<FShuttlegentWriteGate>& InGate)
	: Gate(InGate)
{
	Command = MakeUnique<FAutoConsoleCommand>(ShuttlegentWriteProbe::CommandName, ShuttlegentWriteProbe::CommandHelp, FConsoleCommandWithArgsDelegate::CreateRaw(this, &FShuttlegentWriteProbe::HandleCommand));
}

void FShuttlegentWriteProbe::HandleCommand(const TArray<FString>& Args)
{
	const TSharedPtr<FShuttlegentWriteGate> PinnedGate = Gate.Pin();
	USelection* Selection = GEditor != nullptr ? GEditor->GetSelectedActors() : nullptr;
	if (!PinnedGate.IsValid() || Selection == nullptr) return;

	const float ZOffset = Args.IsValidIndex(ShuttlegentWriteProbe::ArgZOffset) ? FCString::Atof(*Args[ShuttlegentWriteProbe::ArgZOffset]) : ShuttlegentWriteProbe::DefaultZOffset;
	const int32 FailFrom = Args.IsValidIndex(ShuttlegentWriteProbe::ArgFailFrom) ? FCString::Atoi(*Args[ShuttlegentWriteProbe::ArgFailFrom]) : ShuttlegentWriteProbe::NoFailure;
	const float Delay = Args.IsValidIndex(ShuttlegentWriteProbe::ArgDelay) ? FMath::Max(FCString::Atof(*Args[ShuttlegentWriteProbe::ArgDelay]), ShuttlegentWriteProbe::NoDelay) : ShuttlegentWriteProbe::NoDelay;

	TArray<AActor*> SelectedActors;
	Selection->GetSelectedObjects<AActor>(SelectedActors);
	if (SelectedActors.IsEmpty())
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("%s: select one or more actors in the level first."), ShuttlegentWriteProbe::CommandName);
		return;
	}

	// The preview: every actor with its Z now and the Z it will get
	FShuttlegentWriteRequest Request;
	Request.ToolName = ShuttlegentWriteProbe::ToolName;
	Request.Action = LOCTEXT("Action", "设置 Actor 位置").ToString();
	Request.bUndoable = true;

	TArray<TWeakObjectPtr<AActor>> Actors;
	TArray<double> PlannedZ;
	for (AActor* Actor : SelectedActors)
	{
		if (!IsValid(Actor)) continue;

		const double CurrentZ = Actor->GetActorLocation().Z;
		FShuttlegentWriteObject& Object = Request.Objects.AddDefaulted_GetRef();
		Object.Name = Actor->GetActorLabel();
		Object.Path = Actor->GetPathName();
		Object.Field = LOCTEXT("FieldZ", "位置 Z（cm）").ToString();
		Object.CurrentValue = FShuttlegentWriteProbePrivate::FormatZ(CurrentZ);
		Object.PlannedValue = FShuttlegentWriteProbePrivate::FormatZ(CurrentZ + ZOffset);

		Actors.Add(Actor);
		PlannedZ.Add(CurrentZ + ZOffset);
		if (Request.Level.IsEmpty() && IsValid(Actor->GetWorld())) Request.Level = Actor->GetWorld()->GetMapName();
	}
	Request.RawArguments = FShuttlegentWriteProbePrivate::MakeRawArguments(ZOffset, FailFrom, Delay, Actors);

	const TWeakPtr<FShuttlegentWriteGate> WeakGate = PinnedGate;
	const TSharedRef<int32> RequestIdHolder = MakeShared<int32>(0);
	FShuttlegentWriteResult Result;
	Result.Action = Request.Action;
	Result.bUndoable = Request.bUndoable;
	Result.Objects = Request.Objects;

	// Writes the confirmed batch and reports it; runs right after confirmation, or DelaySeconds later
	auto Execute = [WeakGate, RequestIdHolder, Actors, PlannedZ, FailFrom, Result]() mutable
	{
		const TSharedPtr<FShuttlegentWriteGate> Owner = WeakGate.Pin();
		if (!Owner.IsValid()) return;

		// One synchronous transaction for the whole batch; the planned values are what was confirmed
		Result.RequestId = *RequestIdHolder;
		int32 Completed = 0;
		{
			FScopedTransaction Transaction(LOCTEXT("Transaction", "Shuttlegent · 设置 Actor 位置"));
			for (int32 Index = 0; Index < Actors.Num(); ++Index)
			{
				FShuttlegentWriteObject& Object = Result.Objects[Index];
				AActor* Actor = Actors[Index].Get();
				if (FailFrom != ShuttlegentWriteProbe::NoFailure && Index > FailFrom)
				{
					Object.Status = EShuttlegentWriteObjectStatus::NotRun;
					Object.Message = LOCTEXT("NotRun", "前一个对象失败，未执行").ToString();
					continue;
				}
				if (Index == FailFrom)
				{
					Object.Status = EShuttlegentWriteObjectStatus::Failed;
					Object.Message = LOCTEXT("ProbeFailure", "探针参数指定此对象失败").ToString();
					continue;
				}
				if (!IsValid(Actor))
				{
					Object.Status = EShuttlegentWriteObjectStatus::Failed;
					Object.Message = LOCTEXT("ActorGone", "对象已不存在").ToString();
					continue;
				}

				Actor->Modify();
				FVector Location = Actor->GetActorLocation();
				Location.Z = PlannedZ[Index];
				Actor->SetActorLocation(Location);
				Object.Status = EShuttlegentWriteObjectStatus::Completed;
				++Completed;
			}
			if (Completed == 0) Transaction.Cancel();
		}

		// The transaction just closed is the one the next undo would revert
		if (Completed > 0 && GEditor != nullptr && IsValid(GEditor->Trans)) Result.TransactionId = GEditor->Trans->GetUndoContext(true).TransactionId;
		Owner->ReportResult(Result);
	};

	const int32 RequestId = PinnedGate->Request(MoveTemp(Request), [Execute, Delay](EShuttlegentWriteDecision Decision) mutable
	{
		if (Decision != EShuttlegentWriteDecision::Approved) return;
		if (Delay <= ShuttlegentWriteProbe::NoDelay)
		{
			Execute();
			return;
		}

		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Execute](float DeltaTime) mutable
		{
			Execute();
			return false;
		}), Delay);
	});
	*RequestIdHolder = RequestId;
	UE_LOG(LogShuttlegent, Log, TEXT("%s: request %d for %d actor(s), Z offset %.1f, fail from %d, delay %.1f s."), ShuttlegentWriteProbe::CommandName, RequestId, Actors.Num(), ZOffset, FailFrom, Delay);
}

#undef LOCTEXT_NAMESPACE
