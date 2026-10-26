#include "FShuttlegentSampleToolProvider.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"
#include "WriteGate/FShuttlegentWriteRequest.h"

#define LOCTEXT_NAMESPACE "ShuttlegentSample"

namespace FShuttlegentSampleToolProviderPrivate
{
	// Tool and argument names
	inline const TCHAR* CountActorsName = TEXT("sample_count_actors");
	inline const TCHAR* SetLabelName = TEXT("sample_set_actor_label");
	inline const TCHAR* ArgClass = TEXT("class");
	inline const TCHAR* ArgTarget = TEXT("target");
	inline const TCHAR* ArgLabel = TEXT("label");

	UWorld* GetEditorWorld()
	{
		UWorld* World = GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
		return IsValid(World) ? World : nullptr;
	}

	/** An actor of the open level by label or full path. */
	AActor* FindActor(const FString& LabelOrPath)
	{
		UWorld* World = GetEditorWorld();
		if (!IsValid(World)) return nullptr;

		for (TActorIterator<AActor> It(World); It; ++It)
		{
			AActor* Actor = *It;
			if (IsValid(Actor) && (Actor->GetActorLabel() == LabelOrPath || Actor->GetPathName() == LabelOrPath)) return Actor;
		}
		return nullptr;
	}

	class FCountActors : public IShuttlegentTool
	{
	private:
		FShuttlegentToolDefinition Definition {};

	public:
		FCountActors()
		{
			Definition.Name = CountActorsName;
			Definition.Description = TEXT("Sample project tool: how many actors of the open level have each class, most first.");
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgClass, TEXT("Only classes whose name contains this; empty counts all."))
				.Build();
		}

		virtual const FShuttlegentToolDefinition& GetDefinition() const override { return Definition; }

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			UWorld* World = GetEditorWorld();
			if (!IsValid(World)) return FShuttlegentToolResult::Error(TEXT("No level is open."));

			const FString Filter = FShuttlegentToolArguments::GetString(Arguments, ArgClass);
			TMap<FString, int32> Counts;
			for (TActorIterator<AActor> It(World); It; ++It)
			{
				const FString ClassName = It->GetClass()->GetName();
				if (Filter.IsEmpty() || ClassName.Contains(Filter)) ++Counts.FindOrAdd(ClassName);
			}
			Counts.ValueSort([](int32 Left, int32 Right) { return Left > Right; });

			TArray<FString> Lines;
			for (const TPair<FString, int32>& Pair : Counts)
			{
				Lines.Add(FString::Printf(TEXT("%s: %d"), *Pair.Key, Pair.Value));
			}
			return FShuttlegentToolResult::Ok(Lines.IsEmpty() ? FString(TEXT("No matching actors.")) : FString::Join(Lines, TEXT("\n")));
		}
	};

	class FSetActorLabel : public IShuttlegentTool
	{
	private:
		FShuttlegentToolDefinition Definition {};

	public:
		FSetActorLabel()
		{
			Definition.Name = SetLabelName;
			Definition.Description = TEXT("Sample project tool: rename one actor's label in the Outliner. The user confirms first; it is one undoable step.");
			Definition.bReadOnly = false;
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgTarget, TEXT("Current label or full path of the actor."), true)
				.String(ArgLabel, TEXT("New label."), true)
				.Build();
		}

		virtual const FShuttlegentToolDefinition& GetDefinition() const override { return Definition; }

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			const AActor* Actor = FindActor(FShuttlegentToolArguments::GetString(Arguments, ArgTarget));
			const FString Label = FShuttlegentToolArguments::GetString(Arguments, ArgLabel);
			if (!IsValid(Actor) || Label.IsEmpty())
			{
				OutError = TEXT("Give the label or path of an actor in the open level, and a new label.");
				return false;
			}

			OutRequest.Action = LOCTEXT("RenameAction", "示例工具：重命名 Actor").ToString();
			OutRequest.Level = Actor->GetWorld()->GetMapName();
			FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
			Item.Name = Actor->GetActorLabel();
			Item.Path = Actor->GetPathName();
			Item.Field = LOCTEXT("LabelField", "标签").ToString();
			Item.CurrentValue = Actor->GetActorLabel();
			Item.PlannedValue = Label;
			return true;
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			for (FShuttlegentWriteObject& Item : InOutObjects)
			{
				AActor* Actor = FindActor(Item.Path);
				if (!IsValid(Actor))
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = LOCTEXT("ActorGone", "Actor 已不存在").ToString();
					continue;
				}

				Actor->Modify();
				Actor->SetActorLabel(Item.PlannedValue);
				Item.Status = EShuttlegentWriteObjectStatus::Completed;
			}
		}
	};
}

FShuttlegentSampleToolProvider::FShuttlegentSampleToolProvider()
{
	Tools.Add(MakeShared<FShuttlegentSampleToolProviderPrivate::FCountActors>());
	Tools.Add(MakeShared<FShuttlegentSampleToolProviderPrivate::FSetActorLabel>());
}

void FShuttlegentSampleToolProvider::GetTools(TArray<TSharedRef<IShuttlegentTool>>& OutTools) const
{
	OutTools.Append(Tools);
}

#undef LOCTEXT_NAMESPACE
