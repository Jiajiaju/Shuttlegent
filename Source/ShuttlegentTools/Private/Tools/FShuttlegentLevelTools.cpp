#include "Tools/FShuttlegentLevelTools.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"

#include "FShuttlegentToolBase.h"
#include "FShuttlegentToolObjects.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"

namespace FShuttlegentLevelToolsPrivate
{
	// Tool names
	inline const TCHAR* ListActorsName = TEXT("list_actors");
	inline const TCHAR* GetSelectionName = TEXT("get_selection");

	// Argument names
	inline const TCHAR* ArgClass = TEXT("class");
	inline const TCHAR* ArgQuery = TEXT("query");
	inline const TCHAR* ArgOffset = TEXT("offset");
	inline const TCHAR* ArgLimit = TEXT("limit");

	/** One list line: label | class | path | location. */
	FString DescribeActor(const AActor* Actor)
	{
		return FShuttlegentToolObjects::Describe(Actor) + TEXT(" | ") + Actor->GetActorLocation().ToCompactString();
	}

	class FListActors : public FShuttlegentToolBase
	{
	public:
		FListActors()
		{
			Definition.Name = ListActorsName;
			Definition.Description = TEXT("List actors of the level open in the editor (loaded World Partition cells only). Lists label | class | path | location, sorted by label.");
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgClass, TEXT("Only actors of this class or a subclass, e.g. StaticMeshActor, PointLight; empty matches all."))
				.String(ArgQuery, TEXT("Part of the actor label, case-insensitive; empty matches all."))
				.Integer(ArgOffset, TEXT("Index of the first result, for paging. Defaults to 0."))
				.Integer(ArgLimit, TEXT("Results per page, 1-200. Defaults to 50."))
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			UWorld* World = FShuttlegentToolObjects::GetEditorWorld();
			if (!IsValid(World)) return FShuttlegentToolResult::Error(TEXT("No level is open in the editor."));

			const FString ClassName = FShuttlegentToolArguments::GetString(Arguments, ArgClass);
			UClass* Class = ClassName.IsEmpty() ? AActor::StaticClass() : FShuttlegentToolObjects::ResolveClass(ClassName);
			if (!IsValid(Class)) return FShuttlegentToolResult::Error(FString::Printf(TEXT("Unknown class %s."), *ClassName));

			const FString Query = FShuttlegentToolArguments::GetString(Arguments, ArgQuery);
			int32 Offset = 0;
			int32 Limit = 0;
			FShuttlegentToolArguments::GetPage(Arguments, Offset, Limit);

			TArray<AActor*> Actors;
			for (TActorIterator<AActor> It(World, Class); It; ++It)
			{
				AActor* Actor = *It;
				if (IsValid(Actor) && (Query.IsEmpty() || Actor->GetActorLabel().Contains(Query))) Actors.Add(Actor);
			}
			Actors.Sort([](const AActor& Left, const AActor& Right) { return Left.GetActorLabel() < Right.GetActorLabel(); });

			TArray<FString> Lines;
			Lines.Add(FString::Printf(TEXT("Level: %s"), *World->GetMapName()));
			const int32 End = FMath::Min(Offset + Limit, Actors.Num());
			for (int32 Index = Offset; Index < End; ++Index)
			{
				Lines.Add(DescribeActor(Actors[Index]));
			}
			return FShuttlegentToolResult::Ok(FShuttlegentToolArguments::MakePageHeader(Offset, End - FMath::Min(Offset, End), Actors.Num()) + TEXT("\n") + FString::Join(Lines, TEXT("\n")));
		}
	};

	class FGetSelection : public FShuttlegentToolBase
	{
	public:
		FGetSelection()
		{
			Definition.Name = GetSelectionName;
			Definition.Description = TEXT("Actors currently selected in the level editor: label | class | path | location.");
			Definition.InputSchema = FShuttlegentToolSchema().Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			USelection* Selection = GEditor != nullptr ? GEditor->GetSelectedActors() : nullptr;
			if (!IsValid(Selection)) return FShuttlegentToolResult::Error(TEXT("The editor selection is not available."));

			TArray<AActor*> Actors;
			Selection->GetSelectedObjects<AActor>(Actors);
			if (Actors.IsEmpty()) return FShuttlegentToolResult::Ok(TEXT("No actor is selected."));

			TArray<FString> Lines;
			Lines.Add(FString::Printf(TEXT("%d actor(s) selected:"), Actors.Num()));
			for (const AActor* Actor : Actors)
			{
				if (IsValid(Actor)) Lines.Add(DescribeActor(Actor));
			}
			return FShuttlegentToolResult::Ok(FString::Join(Lines, TEXT("\n")));
		}
	};
}

void FShuttlegentLevelTools::Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentLevelToolsPrivate::FListActors>());
	OutTools.Add(MakeShared<FShuttlegentLevelToolsPrivate::FGetSelection>());
}
