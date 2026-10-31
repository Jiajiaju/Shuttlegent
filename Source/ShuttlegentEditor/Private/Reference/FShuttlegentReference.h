#pragma once

#include "CoreMinimal.h"

#include "Reference/EShuttlegentReferenceKind.h"

/** 草稿中的一个引用标签（Shuttlegent UI 交互规范第 4.2 节）。Path 是稳定标识；Name 仅用于显示。 */
struct FShuttlegentReference
{
	EShuttlegentReferenceKind Kind = EShuttlegentReferenceKind::Asset;

	/** 资产对象路径，或 Actor 在编辑器世界中的对象路径。 */
	FString Path;

	/** 资产名称或 Actor 标签。 */
	FString Name;

	/** 资产类或 Actor 类的名称。 */
	FString ClassName;
};
