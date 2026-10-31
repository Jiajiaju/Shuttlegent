#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentUiGuideKind.generated.h"

/** 界面指路条目为用户打开的位置类型。 */
UENUM()
enum class EShuttlegentUiGuideKind : uint8
{
	// 编辑器页签，按页签 ID 打开
	Tab,

	// 设置页面，按 容器 / 分类 / 分节 打开
	Settings
};
