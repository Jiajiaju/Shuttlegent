#include "Tools/FShuttlegentEditTools.h"

#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "Misc/StringOutputDevice.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#include "FShuttlegentToolBase.h"
#include "FShuttlegentToolObjects.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"
#include "WriteGate/FShuttlegentWriteRequest.h"

#define LOCTEXT_NAMESPACE "ShuttlegentEditTools"

namespace FShuttlegentEditToolsPrivate
{
	// Tool names
	inline const TCHAR* SetPropertyName = TEXT("set_property");
	inline const TCHAR* SpawnActorName = TEXT("spawn_actor");
	inline const TCHAR* DeleteActorsName = TEXT("delete_actors");
	inline const TCHAR* SaveAssetsName = TEXT("save_assets");

	// Argument names
	inline const TCHAR* ArgTargets = TEXT("targets");
	inline const TCHAR* ArgProperty = TEXT("property");
	inline const TCHAR* ArgValue = TEXT("value");
	inline const TCHAR* ArgClass = TEXT("class");
	inline const TCHAR* ArgLocation = TEXT("location");
	inline const TCHAR* ArgRotation = TEXT("rotation");
	inline const TCHAR* ArgLabel = TEXT("label");
	inline const TCHAR* ArgAllDirty = TEXT("all_dirty");

	// Components of a location or rotation argument
	inline constexpr int32 VectorLength = 3;

	/** Checks Value against the property without touching the object: imports it into a scratch value and exports it back. */
	bool NormalizeValue(const FProperty* Property, UObject* Owner, const FString& Value, FString& OutNormalized, FString& OutError)
	{
		void* Scratch = FMemory::Malloc(Property->GetSize(), Property->GetMinAlignment());
		Property->InitializeValue(Scratch);

		FStringOutputDevice Errors;
		const TCHAR* End = Property->ImportText_Direct(*Value, Scratch, Owner, PPF_None, &Errors);
		const bool bParsed = End != nullptr;
		if (bParsed) Property->ExportTextItem_Direct(OutNormalized, Scratch, nullptr, Owner, PPF_None);
		else OutError = Errors.IsEmpty() ? FString::Printf(TEXT("'%s' is not a valid value for %s."), *Value, *FShuttlegentToolObjects::DescribeProperty(Property)) : FString(Errors);

		Property->DestroyValue(Scratch);
		FMemory::Free(Scratch);
		return bParsed;
	}

	class FSetProperty : public FShuttlegentToolBase
	{
	public:
		FSetProperty()
		{
			Definition.Name = SetPropertyName;
			Definition.Description = TEXT("Set one property to the same value on one or more objects (actors, components, assets), as the Details panel would. The value uses editor text format, the same get_properties prints: 1.5, true, (X=0,Y=0,Z=100), /Game/Mat/M_Red.M_Red. When the user names a setting in words, it is the Details panel name of the object they selected (usually the actor itself, not its components); check it with get_properties first. The user confirms first; it runs as one undoable step and is rolled back as a whole if any object fails.");
			Definition.bReadOnly = false;
			Definition.InputSchema = FShuttlegentToolSchema()
				.StringArray(ArgTargets, TEXT("Full object paths, or labels of actors in the open level."), true)
				.String(ArgProperty, TEXT("Property name or Details panel name, as get_properties lists it."), true)
				.String(ArgValue, TEXT("New value in editor text format."), true)
				.Build();
		}

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			const FString PropertyName = FShuttlegentToolArguments::GetString(Arguments, ArgProperty);
			const FString Value = FShuttlegentToolArguments::GetString(Arguments, ArgValue);
			const TArray<FString> Targets = FShuttlegentToolArguments::GetStringArray(Arguments, ArgTargets);
			if (Targets.IsEmpty() || PropertyName.IsEmpty())
			{
				OutError = TEXT("Give targets, property and value.");
				return false;
			}

			OutRequest.Action = FText::Format(LOCTEXT("SetPropertyAction", "设置属性 {0}"), FText::FromString(PropertyName)).ToString();
			for (const FString& Target : Targets)
			{
				UObject* Object = FShuttlegentToolObjects::ResolveObject(Target);
				if (!IsValid(Object))
				{
					OutError = FString::Printf(TEXT("No object found for %s; nothing was changed."), *Target);
					return false;
				}

				const FProperty* Property = FShuttlegentToolObjects::FindProperty(Object->GetClass(), PropertyName);
				if (Property == nullptr)
				{
					OutError = FString::Printf(TEXT("%s has no property %s; nothing was changed."), *FShuttlegentToolObjects::GetDisplayName(Object), *PropertyName);
					return false;
				}

				FString Normalized;
				if (!NormalizeValue(Property, Object, Value, Normalized, OutError)) return false;

				FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
				Item.Name = FShuttlegentToolObjects::GetDisplayName(Object);
				Item.Path = Object->GetPathName();
				Item.Field = Property->GetName();
				Item.CurrentValue = FShuttlegentToolObjects::ExportValue(Property, Object);
				Item.PlannedValue = Normalized;
				if (OutRequest.Level.IsEmpty()) OutRequest.Level = FShuttlegentToolObjects::GetLevelName(Object);
			}
			return true;
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			const FString Value = FShuttlegentToolArguments::GetString(Arguments, ArgValue);
			for (FShuttlegentWriteObject& Item : InOutObjects)
			{
				UObject* Object = FShuttlegentToolObjects::ResolveObject(Item.Path);
				FProperty* Property = IsValid(Object) ? FShuttlegentToolObjects::FindProperty(Object->GetClass(), Item.Field) : nullptr;
				if (Property == nullptr)
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = LOCTEXT("ObjectGone", "对象或属性已不存在").ToString();
					continue;
				}

				Object->Modify();
				Object->PreEditChange(Property);
				FStringOutputDevice Errors;
				const TCHAR* End = Property->ImportText_InContainer(*Value, Object, Object, PPF_None, &Errors);
				FPropertyChangedEvent ChangedEvent(Property, EPropertyChangeType::ValueSet);
				Object->PostEditChangeProperty(ChangedEvent);

				Item.Status = End != nullptr ? EShuttlegentWriteObjectStatus::Completed : EShuttlegentWriteObjectStatus::Failed;
				if (End == nullptr) Item.Message = Errors.IsEmpty() ? LOCTEXT("ValueRejected", "值无法写入该属性").ToString() : FString(Errors);
			}
		}
	};

	class FSpawnActor : public FShuttlegentToolBase
	{
	public:
		FSpawnActor()
		{
			Definition.Name = SpawnActorName;
			Definition.Description = TEXT("Place a new actor in the open level, from a native class or a Blueprint. The user confirms first; it runs as one undoable step.");
			Definition.bReadOnly = false;
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgClass, TEXT("Actor class name (PointLight), script path, or Blueprint asset path (/Game/BP/BP_Door)."), true)
				.NumberTuple(ArgLocation, TEXT("Location [x, y, z] in cm. Defaults to [0, 0, 0]."), VectorLength)
				.NumberTuple(ArgRotation, TEXT("Rotation [pitch, yaw, roll] in degrees. Defaults to [0, 0, 0]."), VectorLength)
				.String(ArgLabel, TEXT("Actor label shown in the Outliner; defaults to the class name."))
				.Build();
		}

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			UWorld* World = FShuttlegentToolObjects::GetEditorWorld();
			const FString ClassName = FShuttlegentToolArguments::GetString(Arguments, ArgClass);
			UClass* Class = FShuttlegentToolObjects::ResolveClass(ClassName);
			if (!IsValid(World))
			{
				OutError = TEXT("No level is open in the editor.");
				return false;
			}
			if (!IsValid(Class) || !Class->IsChildOf(AActor::StaticClass()) || Class->HasAnyClassFlags(CLASS_Abstract))
			{
				OutError = FString::Printf(TEXT("%s is not a placeable actor class."), *ClassName);
				return false;
			}

			const FTransform Transform = MakeTransform(Arguments);
			const FString Label = FShuttlegentToolArguments::GetString(Arguments, ArgLabel, Class->GetName());
			OutRequest.Action = FText::Format(LOCTEXT("SpawnAction", "生成 Actor {0}"), FText::FromString(Class->GetName())).ToString();
			OutRequest.Level = World->GetMapName();

			FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
			Item.Name = Label;
			Item.Path = Class->GetPathName();
			Item.Field = LOCTEXT("SpawnField", "位置 / 旋转").ToString();
			Item.CurrentValue = LOCTEXT("NotPlaced", "（新建）").ToString();
			Item.PlannedValue = Transform.GetLocation().ToCompactString() + TEXT(" ") + Transform.Rotator().ToCompactString();
			return true;
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			for (FShuttlegentWriteObject& Item : InOutObjects)
			{
				UWorld* World = FShuttlegentToolObjects::GetEditorWorld();
				UClass* Class = FShuttlegentToolObjects::ResolveClass(Item.Path);
				AActor* Actor = IsValid(World) && IsValid(Class) && GEditor != nullptr ? GEditor->AddActor(World->GetCurrentLevel(), Class, MakeTransform(Arguments), true, RF_Transactional, false) : nullptr;
				if (!IsValid(Actor))
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = LOCTEXT("SpawnFailed", "无法在当前关卡生成该类").ToString();
					continue;
				}

				Actor->SetActorLabel(Item.Name);
				Item.Path = Actor->GetPathName();
				Item.Status = EShuttlegentWriteObjectStatus::Completed;
			}
		}

	private:
		static FTransform MakeTransform(const TSharedRef<FJsonObject>& Arguments)
		{
			TArray<double> Location;
			TArray<double> Rotation;
			const bool bLocation = FShuttlegentToolArguments::GetNumberTuple(Arguments, ArgLocation, VectorLength, Location);
			const bool bRotation = FShuttlegentToolArguments::GetNumberTuple(Arguments, ArgRotation, VectorLength, Rotation);
			const FVector Position = bLocation ? FVector(Location[0], Location[1], Location[2]) : FVector::ZeroVector;
			const FRotator Rotator = bRotation ? FRotator(Rotation[0], Rotation[1], Rotation[2]) : FRotator::ZeroRotator;
			return FTransform(Rotator, Position);
		}
	};

	class FDeleteActors : public FShuttlegentToolBase
	{
	public:
		FDeleteActors()
		{
			Definition.Name = DeleteActorsName;
			Definition.Description = TEXT("Delete actors from the open level. The user confirms first; it runs as one undoable step and is rolled back as a whole if any actor fails.");
			Definition.bReadOnly = false;
			Definition.InputSchema = FShuttlegentToolSchema()
				.StringArray(ArgTargets, TEXT("Full actor paths, or labels of actors in the open level."), true)
				.Build();
		}

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			const TArray<FString> Targets = FShuttlegentToolArguments::GetStringArray(Arguments, ArgTargets);
			OutRequest.Action = LOCTEXT("DeleteAction", "删除 Actor").ToString();
			OutRequest.bDestructive = true;
			for (const FString& Target : Targets)
			{
				const AActor* Actor = Cast<AActor>(FShuttlegentToolObjects::ResolveObject(Target));
				if (!IsValid(Actor) || Actor->GetWorld() != FShuttlegentToolObjects::GetEditorWorld())
				{
					OutError = FString::Printf(TEXT("%s is not an actor of the open level; nothing was deleted."), *Target);
					return false;
				}

				FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
				Item.Name = Actor->GetActorLabel();
				Item.Path = Actor->GetPathName();
				Item.Field = LOCTEXT("DeleteField", "Actor").ToString();
				Item.CurrentValue = Actor->GetClass()->GetName();
				Item.PlannedValue = LOCTEXT("Deleted", "删除").ToString();
				if (OutRequest.Level.IsEmpty()) OutRequest.Level = FShuttlegentToolObjects::GetLevelName(Actor);
			}
			return true;
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			UWorld* World = FShuttlegentToolObjects::GetEditorWorld();
			for (FShuttlegentWriteObject& Item : InOutObjects)
			{
				AActor* Actor = Cast<AActor>(FShuttlegentToolObjects::ResolveObject(Item.Path));
				const bool bDeleted = IsValid(World) && IsValid(Actor) && World->EditorDestroyActor(Actor, true);
				Item.Status = bDeleted ? EShuttlegentWriteObjectStatus::Completed : EShuttlegentWriteObjectStatus::Failed;
				if (!bDeleted) Item.Message = LOCTEXT("DeleteFailed", "Actor 已不存在或无法删除").ToString();
			}
		}
	};

	class FSaveAssets : public FShuttlegentToolBase
	{
	public:
		FSaveAssets()
		{
			Definition.Name = SaveAssetsName;
			Definition.Description = TEXT("Save assets or levels to disk. Saving cannot be undone from the editor; the user confirms first. Source control may ask the user to check files out.");
			Definition.bReadOnly = false;
			Definition.bTransactional = false;
			Definition.InputSchema = FShuttlegentToolSchema()
				.StringArray(ArgTargets, TEXT("Object paths or package names of the assets or levels to save."))
				.Boolean(ArgAllDirty, TEXT("Save every asset and level with unsaved changes instead. Defaults to false."))
				.Build();
		}

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			TArray<UPackage*> Packages;
			if (!CollectPackages(Arguments, Packages, OutError)) return false;

			OutRequest.Action = LOCTEXT("SaveAction", "保存到磁盘").ToString();
			for (const UPackage* Package : Packages)
			{
				FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
				Item.Name = FPackageName::GetShortName(Package->GetName());
				Item.Path = Package->GetName();
				Item.Field = LOCTEXT("SaveField", "文件").ToString();
				Item.CurrentValue = Package->IsDirty() ? LOCTEXT("Dirty", "有未保存的修改").ToString() : LOCTEXT("Clean", "无修改").ToString();
				Item.PlannedValue = LOCTEXT("Saved", "保存").ToString();
			}
			return true;
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			TArray<UPackage*> Packages;
			for (const FShuttlegentWriteObject& Item : InOutObjects)
			{
				UPackage* Package = FindPackage(nullptr, *Item.Path);
				if (IsValid(Package)) Packages.Add(Package);
			}
			UEditorLoadingAndSavingUtils::SavePackages(Packages, false);

			// A package is saved when it no longer has unsaved changes
			for (FShuttlegentWriteObject& Item : InOutObjects)
			{
				const UPackage* Package = FindPackage(nullptr, *Item.Path);
				const bool bSaved = IsValid(Package) && !Package->IsDirty();
				Item.Status = bSaved ? EShuttlegentWriteObjectStatus::Completed : EShuttlegentWriteObjectStatus::Failed;
				if (!bSaved) Item.Message = LOCTEXT("SaveFailed", "未保存：文件可能只读或未签出").ToString();
			}
		}

	private:
		static bool CollectPackages(const TSharedRef<FJsonObject>& Arguments, TArray<UPackage*>& OutPackages, FString& OutError)
		{
			if (FShuttlegentToolArguments::GetBoolean(Arguments, ArgAllDirty, false))
			{
				FEditorFileUtils::GetDirtyContentPackages(OutPackages);
				FEditorFileUtils::GetDirtyWorldPackages(OutPackages);
				if (OutPackages.IsEmpty()) OutError = TEXT("Nothing has unsaved changes.");
				return !OutPackages.IsEmpty();
			}

			for (const FString& Target : FShuttlegentToolArguments::GetStringArray(Arguments, ArgTargets))
			{
				const UObject* Object = FShuttlegentToolObjects::ResolveObject(Target);
				UPackage* Package = IsValid(Object) ? Object->GetPackage() : FindPackage(nullptr, *Target);
				if (!IsValid(Package))
				{
					OutError = FString::Printf(TEXT("No loaded asset or level found for %s; nothing was saved."), *Target);
					return false;
				}
				OutPackages.AddUnique(Package);
			}
			if (OutPackages.IsEmpty()) OutError = TEXT("Give targets or all_dirty.");
			return !OutPackages.IsEmpty();
		}
	};
}

void FShuttlegentEditTools::Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentEditToolsPrivate::FSetProperty>());
	OutTools.Add(MakeShared<FShuttlegentEditToolsPrivate::FSpawnActor>());
	OutTools.Add(MakeShared<FShuttlegentEditToolsPrivate::FDeleteActors>());
	OutTools.Add(MakeShared<FShuttlegentEditToolsPrivate::FSaveAssets>());
}

#undef LOCTEXT_NAMESPACE
