#pragma once

#include "CoreMinimal.h"

class AActor;
class FProperty;
class UClass;
class UObject;
class UStruct;
class UWorld;

/** Finds the objects tool arguments name, and describes them for the model. Game thread only. */
struct FShuttlegentToolObjects
{
	/** The level being edited; nullptr while there is none. */
	static UWorld* GetEditorWorld();

	/**
	 * The object an argument names: a full object path (loaded when it is an unloaded asset), or the label of an actor
	 * in the edited level. nullptr when nothing matches.
	 */
	static UObject* ResolveObject(const FString& PathOrLabel);

	/** The actor of the edited level with that label, or nullptr. */
	static AActor* FindActorByLabel(const FString& Label);

	/** A class by name (StaticMeshActor, AStaticMeshActor) or path (/Script/Engine.StaticMeshActor, a Blueprint asset). */
	static UClass* ResolveClass(const FString& NameOrPath);

	/** Actor label, or object name. */
	static FString GetDisplayName(const UObject* Object);

	/** One line for lists: name, class and path. */
	static FString Describe(const UObject* Object);

	/** A property's value as editor text, cut to the per-value budget. Default values (False, 0, empty) are written out too. */
	static FString ExportValue(const FProperty* Property, const UObject* Object);

	/** How tools name a property: Name "Details panel name" [Category] (type); bool for any bool property. */
	static FString DescribeProperty(const FProperty* Property);

	/** A property of the class by name or by its Details panel name, case-insensitive; nullptr when none matches. */
	static FProperty* FindProperty(const UStruct* Struct, const FString& NameOrDisplayName);

	/** Whether the name is the property's name or its Details panel name, case-insensitive. */
	static bool MatchesProperty(const FProperty* Property, const FString& NameOrDisplayName);

	/** Level name of an object in a world, or empty. */
	static FString GetLevelName(const UObject* Object);
};
