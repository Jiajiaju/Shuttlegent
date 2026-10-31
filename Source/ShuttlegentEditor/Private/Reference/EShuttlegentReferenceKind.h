#pragma once

#include "CoreMinimal.h"

/** 草稿引用所指向的对象类型。 */
enum class EShuttlegentReferenceKind : uint8
{
	// 资产，以其对象路径标识；通过 AssetRegistry 查找，无需加载
	Asset,

	// 编辑器世界中的 Actor，以其对象路径标识
	Actor
};
