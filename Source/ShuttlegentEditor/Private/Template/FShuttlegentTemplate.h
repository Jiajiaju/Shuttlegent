#pragma once

#include "CoreMinimal.h"

/** 一个斜杠模板：模板目录中的一个 Markdown 文件，通过 / 菜单插入草稿。 */
struct FShuttlegentTemplate
{
	/** 不含扩展名的文件名；即用户在 / 之后输入的内容。 */
	FString Name;

	/** 文件第一行去掉 Markdown 标题标记后的内容，显示在菜单中。 */
	FString Description;

	/** 整个文件内容，插入草稿。 */
	FString Body;
};
