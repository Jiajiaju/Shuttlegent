#pragma once

#include "CoreMinimal.h"

struct FShuttlegentReference;

/**
 * 回答中对象链接的点击（功能规划 C6）：在编辑器中定位或打开所指的资产或 Actor。
 * 只改选中状态与焦点，不产生事务，因此不影响“回到这里”对撤销栈的判断。均为纯函数。
 */
struct FShuttlegentReferenceNavigator
{
	/**
	 * 资产：在内容浏览器中定位；bOpen 时打开其编辑器。
	 * Actor：在关卡中单独选中并把关卡编辑器切到前台；bOpen 时视口镜头移到它。
	 * 对象已不存在时什么也不做并返回 false。
	 */
	static bool Reveal(const FShuttlegentReference& Reference, bool bOpen);
};
