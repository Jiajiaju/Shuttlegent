#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "FShuttlegentProviderEntry.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentPermissionPolicy.h"

#include "ShuttlegentSettings.generated.h"

/** Shuttlegent 的个人设置（编辑器偏好设置 > Shuttlegent > General）。 */
UCLASS(Config = EditorPerProjectUserSettings, meta = (DisplayName = "Shuttlegent"))
class SHUTTLEGENTCORE_API UShuttlegentSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 代替插件自带 Runtime/shuttlegent-runtime.exe 启动的运行时可执行文件；留空使用默认值。 */
	UPROPERTY(Config, EditAnywhere, Category = "Runtime", meta = (FilePathFilter = "exe"))
	FFilePath RuntimeExecutableOverride {};

	/** 编辑器启动后空闲时预先启动运行时，打开面板时即已连接。 */
	UPROPERTY(Config, EditAnywhere, Category = "Runtime")
	bool bPrewarmRuntime = true;

	/** 内置 DeepSeek 路由的端点（Anthropic Messages 协议）；留空使用 DeepSeek 官方端点。下次启动运行时生效。 */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	FString DeepSeekBaseUrl = ShuttlegentProviderConstant::DeepSeekBaseUrl;

	/** 内置 DeepSeek 路由的密钥，只保存在本机设置中；未填写时运行时无法在该路由上启动。 */
	UPROPERTY(Config, EditAnywhere, Category = "Providers", meta = (PasswordField = true))
	FString DeepSeekApiKey;

	/** 内置 DeepSeek 之外的供应商路由，例如团队网关。下次启动运行时生效。 */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	TArray<FShuttlegentProviderEntry> Providers;

	/** 新会话使用的路由；留空使用内置 DeepSeek 路由。 */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	FString DefaultProvider;

	/** 新会话使用的模型，取自默认路由的模型列表；留空使用该路由的第一个模型。 */
	UPROPERTY(Config, EditAnywhere, Category = "Providers")
	FString DefaultModel;

	/** 允许运行时的文件工具不经询问写入项目目录；关闭时每次写入都需提权。下次启动运行时生效。 */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	bool bAllowWorkspaceWrite = false;

	/** 为运行时提供 PowerShell 工具。下次启动运行时生效。 */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	bool bAllowShell = false;

	/** 允许运行时请求让工具越出沙箱；关闭时此类请求立即拒绝，并在对话中注明。 */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	bool bAllowElevation = true;

	/** 不提供给模型的编辑器工具，按工具名填写；在 Editor Tools 分类下显示为开关。立即生效。 */
	UPROPERTY(Config, EditAnywhere, Category = "Editor Tools")
	TArray<FString> DisabledTools;

	/** 不提供给模型的引擎 Toolset，按 Toolset 名填写；在 Engine Toolsets 分类下显示为开关。立即生效。 */
	UPROPERTY(Config, EditAnywhere, Category = "Engine Toolsets")
	TArray<FString> DisabledToolsets;

	/** 外部 MCP 服务请求头所引用变量的值（变量在 项目设置 > Shuttlegent > General 中命名），例如 TEAM_KB_TOKEN -> <token>；只保存在本机设置中。 */
	UPROPERTY(Config, EditAnywhere, Category = "Credentials")
	TMap<FString, FString> McpHeaderValues;

	/** 没有编辑器对话框处理时，对运行时权限请求的答复。 */
	UPROPERTY(Config, EditAnywhere, Category = "Permissions")
	EShuttlegentPermissionPolicy PermissionPolicy = EShuttlegentPermissionPolicy::Reject;

	/** 在面板中播放标志动画（开场、工作中、悬停、完成）；关闭时标志始终显示静止帧。 */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	bool bAnimateMark = true;

	/** 面板中对话与输入框的字号，单位 CSS 像素；面板侧栏与控件保持原尺寸。 */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance", meta = (ClampMin = "11", ClampMax = "20", UIMin = "11", UIMax = "20"))
	int32 FontSize = ShuttlegentAppearanceConstant::DefaultFontSize;

	/** 对话与输入框占满两侧窗格之间的全部宽度，不再使用居中的阅读栏。 */
	UPROPERTY(Config, EditAnywhere, Category = "Appearance")
	bool bWideLayout = false;

	//~ Begin UDeveloperSettings Interface.
	virtual FName GetContainerName() const override;
	virtual FName GetCategoryName() const override;
	virtual FName GetSectionName() const override;
	//~ End UDeveloperSettings Interface.
};
