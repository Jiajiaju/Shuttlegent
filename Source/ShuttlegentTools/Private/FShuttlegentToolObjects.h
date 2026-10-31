#pragma once

#include "CoreMinimal.h"

class AActor;
class FProperty;
class UClass;
class UObject;
class UStruct;
class UWorld;

/** 查找工具参数所指的对象，并为模型描述这些对象。仅限游戏线程。 */
struct FShuttlegentToolObjects
{
	/** 正在编辑的关卡；没有时为 nullptr。 */
	static UWorld* GetEditorWorld();

	/**
	 * 参数所指的对象：完整对象路径（若为未加载的资源则加载），或正在编辑关卡中某个 Actor 的标签。
	 * 无匹配时为 nullptr。
	 */
	static UObject* ResolveObject(const FString& PathOrLabel);

	/** 正在编辑关卡中具有该标签的 Actor，或 nullptr。 */
	static AActor* FindActorByLabel(const FString& Label);

	/** 按名称（StaticMeshActor、AStaticMeshActor）或路径（/Script/Engine.StaticMeshActor、Blueprint 资源）查找的类。 */
	static UClass* ResolveClass(const FString& NameOrPath);

	/** Actor 标签，或对象名称。 */
	static FString GetDisplayName(const UObject* Object);

	/** 用于列表的单行描述：名称、类与路径。 */
	static FString Describe(const UObject* Object);

	/** 属性值的编辑器文本，按单值长度上限截断。默认值（False、0、空）同样会输出。 */
	static FString ExportValue(const FProperty* Property, const UObject* Object);

	/** 工具对属性的表述方式：Name "Details panel name" [Category] (type)；任何 bool 属性的类型均写作 bool。 */
	static FString DescribeProperty(const FProperty* Property);

	/** 按名称或 Details 面板名称（不区分大小写）查找类的属性；无匹配时为 nullptr。 */
	static FProperty* FindProperty(const UStruct* Struct, const FString& NameOrDisplayName);

	/** 该名称是否为属性名称或其 Details 面板名称（不区分大小写）。 */
	static bool MatchesProperty(const FProperty* Property, const FString& NameOrDisplayName);

	/** 对象在 World 中所属关卡的名称，或为空。 */
	static FString GetLevelName(const UObject* Object);
};
