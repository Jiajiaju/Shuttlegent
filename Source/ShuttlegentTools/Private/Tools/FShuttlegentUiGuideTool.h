#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/**
 * show_editor_ui：打开项目登记为 UI 指引的编辑器位置（编辑器标签页或设置页面），并返回该指引的说明语句。
 * 每次 tools/list 时重新构建，使指引列表跟随项目设置；项目未登记任何指引时不提供该工具。
 * 仅打开视图，因此无需确认。
 */
struct FShuttlegentUiGuideTool
{
	/** 该工具；项目未登记任何 UI 指引时为 nullptr。 */
	static TSharedPtr<IShuttlegentTool> Make();
};
