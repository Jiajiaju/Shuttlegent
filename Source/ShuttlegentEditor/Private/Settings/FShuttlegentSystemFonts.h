#pragma once

#include "CoreMinimal.h"

#include "Settings/FShuttlegentSystemFont.h"

/**
 * 本机已安装的字族，供字体设置的下拉列表与“未安装”提示使用。
 *
 * Windows 上经 DirectWrite 枚举系统字体集合：每个字族取英文名（面板用的 Chromium 同样经 DirectWrite 匹配），
 * 并标出是否等宽。首次调用时枚举一次并缓存；其他平台返回空列表。
 */
class FShuttlegentSystemFonts
{
public:
	/** 按名称排序、去重的全部字族。 */
	static const TArray<FShuttlegentSystemFont>& Get();

	/** 字族是否已安装；按名称比较，不区分大小写，也认本地化名（如“微软雅黑”）。 */
	static bool IsInstalled(const FString& Family);
};
