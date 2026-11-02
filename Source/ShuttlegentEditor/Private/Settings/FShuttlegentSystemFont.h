#pragma once

#include "CoreMinimal.h"

/** 本机安装的一个字族。 */
struct FShuttlegentSystemFont
{
	/** 字族名，优先英文名；面板的 font-family 按它匹配。 */
	FString Family;

	/** 字族中有等宽字体；代码字体只列这类。 */
	bool bMonospace = false;
};
