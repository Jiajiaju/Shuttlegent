#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "FShuttlegentExternalMcpServer.h"
#include "FShuttlegentUiGuide.h"

#include "ShuttlegentProjectSettings.generated.h"

/**
 * 项目经版本控制共享的设置（项目设置 > Shuttlegent > General，Shuttlegent.md 第 11 节）。
 * 插件默认值来自插件的 Config/DefaultShuttlegent.ini，项目在自己的 Config/DefaultShuttlegent.ini 中覆盖。
 * 个人选项（包括模型供应商及其密钥）在 UShuttlegentSettings 中。
 */
UCLASS(Config = Shuttlegent, DefaultConfig, meta = (DisplayName = "Shuttlegent"))
class SHUTTLEGENTCORE_API UShuttlegentProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 运行时加载的技能目录，相对项目目录。 */
	UPROPERTY(Config, EditAnywhere, Category = "Integration", meta = (RelativeToGameDir))
	TArray<FDirectoryPath> SkillDirs;

	/** 斜杠模板（*.md）所在目录，相对项目目录。 */
	UPROPERTY(Config, EditAnywhere, Category = "Integration", meta = (RelativeToGameDir))
	TArray<FDirectoryPath> TemplateDirs;

	/** 每个会话都声明给运行时的知识或工具服务。 */
	UPROPERTY(Config, EditAnywhere, Category = "Integration")
	TArray<FShuttlegentExternalMcpServer> ExternalMcpServers;

	/** 助手可以为用户打开的编辑器位置。 */
	UPROPERTY(Config, EditAnywhere, Category = "Integration")
	TArray<FShuttlegentUiGuide> UiGuides;

	/**
	 * 对全团队屏蔽的引擎 Toolset（Toolset Registry）及其工具，助手看不到它们。规则按不区分大小写的子串匹配
	 * Toolset 名或工具全名（Toolset.Tool）；写成 /pattern/ 时为正则表达式。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> BlockedToolsetNames;

	/**
	 * 名称以这些词开头的引擎 Toolset 工具视为只读，无需确认即可执行，例如 Get 匹配 GetSelectedActors 与 get_actors。
	 * 其余引擎 Toolset 工具执行前都要用户确认。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> ReadOnlyToolPrefixes;

	/** 另外视为只读的引擎 Toolset 工具，按工具名（Snapshot）或全名（Toolset.Tool）填写。 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> ReadOnlyTools;

	/** 名称像查询、实际会写入的引擎 Toolset 工具，执行前总要确认；优先于只读规则。 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> WriteTools;

	/**
	 * 不在编辑器事务中执行、并标为不可撤销的引擎 Toolset 工具，按工具名或全名填写。
	 * 用于撤销会损坏所编辑对象的工具，例如原地重建整张表的 DataTable 工具。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> NonUndoableTools;

	/**
	 * 名称以这些词开头的引擎 Toolset 工具会删除、移动或改名，其确认卡标为破坏性操作。匹配方式同 Read Only Tool Prefixes。
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> DestructiveToolPrefixes;

	//~ Begin UDeveloperSettings Interface.
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End UDeveloperSettings Interface.
};
