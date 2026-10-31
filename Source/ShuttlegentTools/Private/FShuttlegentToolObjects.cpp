#include "FShuttlegentToolObjects.h"

#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"

#include "ShuttlegentToolsConstants.h"

UWorld* FShuttlegentToolObjects::GetEditorWorld()
{
	if (GEditor == nullptr) return nullptr;

	UWorld* World = GEditor->GetEditorWorldContext().World();
	return IsValid(World) ? World : nullptr;
}

UObject* FShuttlegentToolObjects::ResolveObject(const FString& PathOrLabel)
{
	if (PathOrLabel.IsEmpty()) return nullptr;
	if (!PathOrLabel.StartsWith(ShuttlegentToolsConstant::PathPrefix)) return FindActorByLabel(PathOrLabel);

	UObject* Found = StaticFindObject(UObject::StaticClass(), nullptr, *PathOrLabel);
	if (IsValid(Found)) return Found;

	// 尚未加载的资源；指向未打开关卡的路径保持无法解析
	UObject* Loaded = LoadObject<UObject>(nullptr, *PathOrLabel, nullptr, LOAD_NoWarn | LOAD_Quiet);
	return IsValid(Loaded) ? Loaded : nullptr;
}

AActor* FShuttlegentToolObjects::FindActorByLabel(const FString& Label)
{
	UWorld* World = GetEditorWorld();
	if (!IsValid(World)) return nullptr;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (IsValid(Actor) && Actor->GetActorLabel() == Label) return Actor;
	}
	return nullptr;
}

UClass* FShuttlegentToolObjects::ResolveClass(const FString& NameOrPath)
{
	if (NameOrPath.IsEmpty()) return nullptr;

	if (NameOrPath.StartsWith(ShuttlegentToolsConstant::PathPrefix))
	{
		UObject* Object = ResolveObject(NameOrPath);
		const UBlueprint* Blueprint = Cast<UBlueprint>(Object);
		if (IsValid(Blueprint)) return Blueprint->GeneratedClass;

		UClass* Class = Cast<UClass>(Object);
		if (IsValid(Class)) return Class;

		// 不带生成类后缀的 Blueprint 资源路径
		UClass* Generated = LoadObject<UClass>(nullptr, *(NameOrPath + ShuttlegentToolsConstant::GeneratedClassSuffix), nullptr, LOAD_NoWarn | LOAD_Quiet);
		return IsValid(Generated) ? Generated : nullptr;
	}

	UClass* Class = FindFirstObject<UClass>(*NameOrPath, EFindFirstObjectOptions::NativeFirst);
	if (IsValid(Class)) return Class;

	for (const TCHAR* Prefix : ShuttlegentToolsConstant::ClassPrefixes)
	{
		if (!NameOrPath.StartsWith(Prefix, ESearchCase::CaseSensitive)) continue;

		UClass* Unprefixed = FindFirstObject<UClass>(*NameOrPath.RightChop(FCString::Strlen(Prefix)), EFindFirstObjectOptions::NativeFirst);
		if (IsValid(Unprefixed)) return Unprefixed;
	}
	return nullptr;
}

FString FShuttlegentToolObjects::GetDisplayName(const UObject* Object)
{
	if (!IsValid(Object)) return FString();

	const AActor* Actor = Cast<AActor>(Object);
	return IsValid(Actor) ? Actor->GetActorLabel() : Object->GetName();
}

FString FShuttlegentToolObjects::Describe(const UObject* Object)
{
	if (!IsValid(Object)) return FString();

	return FString::Printf(TEXT("%s | %s | %s"), *GetDisplayName(Object), *Object->GetClass()->GetName(), *Object->GetPathName());
}

FString FShuttlegentToolObjects::ExportValue(const FProperty* Property, const UObject* Object)
{
	if (Property == nullptr || !IsValid(Object)) return FString();

	// ExportText_InContainer 会跳过与 delta 相等的值，且无 delta 时按零值处理：False 和 0 会导出为空
	FString Value;
	Property->ExportTextItem_Direct(Value, Property->ContainerPtrToValuePtr<void>(Object), nullptr, const_cast<UObject*>(Object), PPF_None);
	if (Value.Len() <= ShuttlegentToolsConstant::MaxValueChars) return Value;

	return Value.Left(ShuttlegentToolsConstant::MaxValueChars) + ShuttlegentToolsConstant::Ellipsis;
}

FString FShuttlegentToolObjects::DescribeProperty(const FProperty* Property)
{
	if (Property == nullptr) return FString();

	const FString Type = Property->IsA<FBoolProperty>() ? FString(ShuttlegentToolsConstant::BoolTypeName) : Property->GetCPPType();
	const FString Category = Property->GetMetaData(ShuttlegentToolsConstant::MetaCategory);
	const FString CategoryText = Category.IsEmpty() ? FString() : FString::Printf(TEXT(" [%s]"), *Category);
	return FString::Printf(TEXT("%s \"%s\"%s (%s)"), *Property->GetName(), *Property->GetDisplayNameText().ToString(), *CategoryText, *Type);
}

FProperty* FShuttlegentToolObjects::FindProperty(const UStruct* Struct, const FString& NameOrDisplayName)
{
	if (!IsValid(Struct) || NameOrDisplayName.IsEmpty()) return nullptr;

	// C++ 名称优先于恰好同名的 Details 面板名称
	FProperty* ByName = FindFProperty<FProperty>(Struct, FName(*NameOrDisplayName));
	if (ByName != nullptr) return ByName;

	for (TFieldIterator<FProperty> It(Struct); It; ++It)
	{
		if (MatchesProperty(*It, NameOrDisplayName)) return *It;
	}
	return nullptr;
}

bool FShuttlegentToolObjects::MatchesProperty(const FProperty* Property, const FString& NameOrDisplayName)
{
	if (Property == nullptr) return false;

	return Property->GetName().Equals(NameOrDisplayName, ESearchCase::IgnoreCase) || Property->GetDisplayNameText().ToString().Equals(NameOrDisplayName, ESearchCase::IgnoreCase);
}

FString FShuttlegentToolObjects::GetLevelName(const UObject* Object)
{
	if (!IsValid(Object)) return FString();

	const UWorld* World = Object->GetWorld();
	return IsValid(World) ? World->GetMapName() : FString();
}
