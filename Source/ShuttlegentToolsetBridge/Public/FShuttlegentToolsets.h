#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Dom/JsonObject.h"
#include "Templates/ValueOrError.h"

/** 设置与 list_toolsets 所展示的单个引擎 Toolset。 */
struct FShuttlegentToolsetInfo
{
	/** 注册表中的名称，例如 EditorToolset.EditorActorTools；即模型作为 "toolset" 传入的值。 */
	FString Name;

	FString Description;

	/** 助手可使用的工具数；Toolset 被屏蔽或关闭时为 0。 */
	int32 ToolCount = 0;

	/** 已通过 Project Settings > Shuttlegent > General > Blocked Toolset Names 对整个团队隐藏。 */
	bool bBlockedByProject = false;

	/** 已被当前用户关闭（Editor Preferences > Shuttlegent > General > Engine Toolsets）。 */
	bool bDisabledByUser = false;
};

/** 引擎 Toolset 中的单个工具。 */
struct FShuttlegentToolsetTool
{
	/** 工具在其 Toolset 内的名称，例如 GetSelectedActors；即模型作为 "tool" 传入的值。 */
	FString Name;

	/** Toolset.Tool */
	FString FullName;

	FString Description;
	TSharedPtr<FJsonObject> InputSchema;

	/** 无需确认即可执行；其他工具均须先征得用户同意。 */
	bool bReadOnly = false;

	/** 在编辑器事务中执行；对于项目的 Non Undoable Tools 为 false。 */
	bool bUndoable = true;

	/** 会删除、移动或重命名对象（项目的 Destructive Tool Prefixes）；其确认卡片会对此加以提示。 */
	bool bDestructive = false;
};

/**
 * Shuttlegent 视角下的引擎 Toolset Registry：助手可使用哪些 Toolset 与工具（注册表自身的过滤、项目的屏蔽列表、
 * 用户的开关），以及其中哪些为只读（项目的只读列表）。
 * 仅限游戏线程。
 */
class SHUTTLEGENTTOOLSETBRIDGE_API FShuttlegentToolsets
{
public:
	/** 注册表存在（编辑器已启动且 Toolset Registry 插件已加载）。 */
	static bool IsAvailable();

	/** 注册表提供的全部 Toolset（包括被屏蔽和已关闭的），按名称排序。 */
	static TArray<FShuttlegentToolsetInfo> GetToolsets();

	/** 该 Toolset 中助手可使用的工具；Toolset 未知或不可用时返回 false，并填写 OutError。 */
	static bool GetTools(const FString& ToolsetName, TArray<FShuttlegentToolsetTool>& OutTools, FString& OutError);

	/** 工具返回了 {"returnValue": false}；对写入工具而言，这表示其未完成所请求的操作（删除或重命名失败）。 */
	static bool ReturnedFalse(const FString& ResultText);

	/** 查找该名称的可用工具；否则返回 false，并填写 OutError。 */
	static bool FindTool(const FString& ToolsetName, const FString& ToolName, FShuttlegentToolsetTool& OutTool, FString& OutError);

	/**
	 * 以 JSON 参数执行工具；future 中保存工具的 JSON 结果或错误。工具以无人值守方式运行：
	 * 其触发的引擎消息对话框不会显示，引擎在其于游戏线程运行期间记录的警告、错误和对话框文本
	 * 会附加到结果中。
	 */
	static TFuture<TValueOrError<FString, FString>> Execute(const FString& ToolsetName, const FString& ToolName, const FString& JsonArguments);
};
