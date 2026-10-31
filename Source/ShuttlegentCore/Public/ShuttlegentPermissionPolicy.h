#pragma once

#include "CoreMinimal.h"

#include "ShuttlegentPermissionPolicy.generated.h"

/** 没有绑定界面处理器时，如何答复运行时发来的 session/request_permission。 */
UENUM()
enum class EShuttlegentPermissionPolicy : uint8
{
	// 拒绝所有请求
	Reject,

	// 每个请求都允许一次
	Allow
};
