#pragma once

#include "CoreMinimal.h"

#include "Template/FShuttlegentTemplate.h"

/** 读取项目模板目录中的斜杠模板（Shuttlegent.md 第 11 节）。 */
struct FShuttlegentTemplateLibrary
{
	/** 各目录及其子文件夹中的全部 *.md 文件，按名称排序；同名的两个文件以先出现的目录为准。 */
	static TArray<FShuttlegentTemplate> Load();
};
