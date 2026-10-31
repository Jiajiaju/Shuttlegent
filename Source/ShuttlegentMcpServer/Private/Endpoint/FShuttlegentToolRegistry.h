#pragma once

#include "CoreMinimal.h"

#include "Tool/IShuttlegentTool.h"

/** 所有已注册 IShuttlegentToolProvider 的全部工具，按名称索引。名称无效或已被占用的工具会被跳过并给出警告。 */
class FShuttlegentToolRegistry
{
public:
	/** 按工具提供者的注册顺序重新收集工具。 */
	static TArray<TSharedRef<IShuttlegentTool>> Collect();

	/** 返回该名称的工具，不存在时返回 nullptr。 */
	static TSharedPtr<IShuttlegentTool> Find(const FString& Name);
};
