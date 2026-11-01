#pragma once

#include "CoreMinimal.h"

#include "FShuttlegentMemoryChange.h"
#include "FShuttlegentMemoryItem.h"

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShuttlegentMemoryChanged, const FShuttlegentMemoryChange& /*Change*/, bool /*bByAgent*/);

/**
 * 个人记忆：<Project>/Saved/Shuttlegent/MEMORY.md，每条一行（“- 内容（日期）”），其余行原样保留。
 * 运行时插件 shuttlegent-memory 每次请求读取该文件，修改从下一次请求起生效。仅限游戏线程。
 */
class SHUTTLEGENTCORE_API FShuttlegentMemoryStore
{
public:
	/** 文件经本类修改后广播；bByAgent 为 Agent 经 remember、forget 工具所做的修改。 */
	static FOnShuttlegentMemoryChanged& OnChanged();

	/** 本地目录 <Project>/Saved/Shuttlegent，记忆与个人提示词都在这里。 */
	static FString GetLocalDir();
	static FString GetMemoryFile();

	/** 全部记忆，按文件中的顺序；文件不存在时为空。 */
	static TArray<FShuttlegentMemoryItem> Load();

	/** 记忆文件的大小，单位字节。 */
	static int32 GetFileBytes();

	/**
	 * 记一条：Replaces 非空时替换内容与之相同的那一条，否则新增；与已有的一条相同时不做修改并返回 true、
	 * OutChanged 为 false。超出上限、要替换的那一条不存在等情况返回 false 并给出原因（英文，交给 Agent）。
	 */
	static bool Remember(const FString& Text, const FString& Replaces, bool bByAgent, bool& OutChanged, FString& OutError);

	/** 删除内容与 Text 相同的那一条；没有时返回 false 并给出原因。 */
	static bool Forget(const FString& Text, bool bByAgent, FString& OutError);

	/** 撤销一次修改：仍能对应上文件当前内容时执行并返回 true。 */
	static bool Revert(const FShuttlegentMemoryChange& Change, FString& OutError);

	/** 项目根目录还有旧位置的个人提示词 SHUTTLEGENT.local.md、而本地目录没有时，把它移过去。 */
	static void MigrateLocalPrompt();
};
