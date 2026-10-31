#pragma once

#include "CoreMinimal.h"

/** ACP 方法名（Agent Client Protocol v1）。 */
namespace ShuttlegentAcpMethod
{
	inline const TCHAR* Initialize = TEXT("initialize");
	inline const TCHAR* SessionNew = TEXT("session/new");
	inline const TCHAR* SessionResume = TEXT("session/resume");
	inline const TCHAR* SessionClose = TEXT("session/close");
	inline const TCHAR* SessionPrompt = TEXT("session/prompt");
	inline const TCHAR* SessionCancel = TEXT("session/cancel");
	inline const TCHAR* SessionUpdate = TEXT("session/update");
	inline const TCHAR* SessionSetConfigOption = TEXT("session/set_config_option");
	inline const TCHAR* SessionRequestPermission = TEXT("session/request_permission");
}

/** ACP 协议取值。 */
namespace ShuttlegentAcpConstant
{
	// 本客户端使用的 ACP 协议版本
	inline constexpr int32 ProtocolVersion = 1;

	// JSON-RPC“方法不存在”错误码
	inline constexpr int32 MethodNotFoundErrorCode = -32601;

	// 运行时在答复前退出时，本地使用的 JSON-RPC 错误码
	inline constexpr int32 RuntimeExitedErrorCode = -32000;

	// session/request_permission 提供的权限选项类型
	inline const TCHAR* PermissionKindAllowOnce = TEXT("allow_once");
	inline const TCHAR* PermissionKindRejectOnce = TEXT("reject_once");
}

/** 传给 DeepSeek Harness 运行时的环境变量。 */
namespace ShuttlegentEnvVar
{
	inline const TCHAR* DshHome = TEXT("DSH_HOME");
	inline const TCHAR* DshAgentsHome = TEXT("DSH_AGENTS_HOME");
	inline const TCHAR* DshPermissionMode = TEXT("DSH_PERMISSION_MODE");
	inline const TCHAR* DshTelemetryDisabled = TEXT("DSH_TELEMETRY_DISABLED");
	inline const TCHAR* LocalAppData = TEXT("LOCALAPPDATA");

	// 内置 DeepSeek 路由（deepseek-official）读取密钥的变量；取值来自 UShuttlegentSettings::DeepSeekApiKey
	inline const TCHAR* ProviderKey = TEXT("DEEPSEEK_API_KEY");

	// 该供应商在启动引导卡中的显示名
	inline const TCHAR* ProviderName = TEXT("DeepSeek");

	// 编辑器端点的 URL 与 bearer token，供 Shuttlegent 自带的运行时插件（context tap）使用
	inline const TCHAR* EditorEndpointUrl = TEXT("SHUTTLEGENT_EDITOR_ENDPOINT");
	inline const TCHAR* EditorEndpointToken = TEXT("SHUTTLEGENT_EDITOR_TOKEN");

	// 写入 DSH_TELEMETRY_DISABLED 的值；任何非空值都表示关闭遥测
	inline const TCHAR* TelemetryDisabledValue = TEXT("1");

	// 从编辑器环境复制的变量；其余一律不传
	inline const TCHAR* InheritedNames[] =
	{
		TEXT("SystemRoot"),
		TEXT("SystemDrive"),
		TEXT("windir"),
		TEXT("PATH"),
		TEXT("PATHEXT"),
		TEXT("COMSPEC"),
		TEXT("TEMP"),
		TEXT("TMP"),
		TEXT("USERPROFILE"),
		TEXT("APPDATA"),
		TEXT("LOCALAPPDATA"),
		TEXT("HOMEDRIVE"),
		TEXT("HOMEPATH"),
		TEXT("USERNAME"),
		TEXT("COMPUTERNAME"),
	};
}

/** 模型供应商与生成的运行时补丁（Shuttlegent.md 第 8.2、9 节）。 */
namespace ShuttlegentProviderConstant
{
	// 运行时内置的 DeepSeek 路由，未指定默认供应商时使用
	inline const TCHAR* BuiltinRoute = TEXT("deepseek-official");

	// 内置路由的官方端点（Anthropic Messages）；UShuttlegentSettings::DeepSeekBaseUrl 的默认值
	inline const TCHAR* DeepSeekBaseUrl = TEXT("https://api.deepseek.com/anthropic");

	// DeepSeek 的 OpenAI 兼容端点；新增供应商条目的默认基础 URL
	inline const TCHAR* DeepSeekOpenAiBaseUrl = TEXT("https://api.deepseek.com");

	// 供应商条目未指定密钥变量时使用的变量名：SHUTTLEGENT_<ROUTE>_API_KEY，连字符换成下划线
	inline const TCHAR* KeyVariablePrefix = TEXT("SHUTTLEGENT_");
	inline const TCHAR* KeyVariableSuffix = TEXT("_API_KEY");
	inline const TCHAR* RouteSeparator = TEXT("-");
	inline const TCHAR* VariableSeparator = TEXT("_");

	// llm-pi-ai 的 api 取值（EShuttlegentProviderApi）
	inline const TCHAR* ApiOpenAiCompletions = TEXT("openai-completions");
	inline const TCHAR* ApiOpenAiResponses = TEXT("openai-responses");
	inline const TCHAR* ApiAnthropicMessages = TEXT("anthropic-messages");

	// 插件每次启动前写入 DSH_HOME、作为第二个 --patch 传入的补丁（JSON 也是合法的 YAML）
	inline const TCHAR* GeneratedPatchFile = TEXT("generated.patch.yml");

	// 生成补丁中的行与字段
	inline const TCHAR* RowLlmPiAi = TEXT("llm-pi-ai");
	inline const TCHAR* RowLlmDeepSeek = TEXT("llm-deepseek");
	inline const TCHAR* RowAcp = TEXT("acp");
	inline const TCHAR* RowSkillFilesystem = TEXT("skill-filesystem");
	inline const TCHAR* RowToolPwsh = TEXT("tool-pwsh");
	inline const TCHAR* FieldId = TEXT("id");
	inline const TCHAR* FieldConfig = TEXT("config");
	inline const TCHAR* FieldDisabled = TEXT("disabled");
	inline const TCHAR* FieldProviders = TEXT("providers");
	inline const TCHAR* FieldDisplayName = TEXT("displayName");
	inline const TCHAR* FieldApiKeyEnv = TEXT("apiKeyEnv");
	inline const TCHAR* FieldApi = TEXT("api");
	inline const TCHAR* FieldBaseUrl = TEXT("baseURL");
	inline const TCHAR* FieldModels = TEXT("models");
	inline const TCHAR* FieldModelId = TEXT("id");
	inline const TCHAR* FieldModelName = TEXT("name");
	inline const TCHAR* FieldProvider = TEXT("provider");
	inline const TCHAR* FieldModel = TEXT("model");
	inline const TCHAR* FieldIncludeDefaultRoots = TEXT("includeDefaultRoots");
	inline const TCHAR* FieldCustomSkillDirs = TEXT("customSkillDirs");

	// 插入 context tap 插件的行，按绝对路径加载
	inline const TCHAR* RowContextTap = TEXT("shuttlegent-context-tap");
	inline const TCHAR* FieldInsert = TEXT("insert");
	inline const TCHAR* FieldPluginName = TEXT("name");
}

/** 相对插件目录的路径与用户级路径。 */
namespace ShuttlegentPath
{
	inline const TCHAR* PluginName = TEXT("Shuttlegent");

	// 运行时可执行文件，相对插件目录
	inline const TCHAR* RuntimeExecutable = TEXT("Runtime/shuttlegent-runtime.exe");

	// Shuttlegent 配置补丁，相对插件目录
	inline const TCHAR* ProfilePatch = TEXT("Runtime/profile/shuttlegent.patch.yml");

	// 经 ACP 启动的 dsh 配置
	inline const TCHAR* AcpProfile = TEXT("acp");

	// 向编辑器端点上报各会话上下文分项的运行时插件，相对插件目录
	inline const TCHAR* ContextTapPlugin = TEXT("Runtime/plugins/shuttlegent-context-tap.mjs");

	// DSH_HOME，相对 %LOCALAPPDATA%；同时是运行时的启动目录，必须在系统盘
	inline const TCHAR* DshHome = TEXT("Shuttlegent/dsh-home");

	// DSH_AGENTS_HOME，相对 DSH_HOME
	inline const TCHAR* DshAgentsHome = TEXT("agents");

	// 会话记录与索引，相对项目 Saved 目录
	inline const TCHAR* SessionsDir = TEXT("Shuttlegent/Sessions");
	inline const TCHAR* SessionIndexFile = TEXT("index.json");
	inline const TCHAR* TranscriptExtension = TEXT(".jsonl");
}

/** 运行时进程的时间参数。 */
namespace ShuttlegentProcessConstant
{
	// 关闭 stdin 后留给运行时自行退出的时间，超时即强制结束，单位秒
	inline constexpr float GracefulExitTimeoutSeconds = 5.0f;

	// 每次读取管道的大小，单位字节
	inline constexpr int32 PipeReadChunkBytes = 64 * 1024;

	// 运行时被 Shuttlegent 强制结束时使用的退出码
	inline constexpr int32 TerminatedExitCode = -1;
}

/** 会话管理。 */
namespace ShuttlegentSessionConstant
{
	// 由首条提问生成的会话标题的最大长度，单位字符
	inline constexpr int32 TitleMaxChars = 40;

	// 用户重命名时标题的最大长度，单位字符（Shuttlegent 界面交互规范第 6 节）
	inline constexpr int32 RenameMaxChars = 60;
}

namespace ShuttlegentAppearanceConstant
{
	// 面板页面中对话的字号，单位 CSS 像素（UShuttlegentSettings::FontSize）
	inline constexpr int32 DefaultFontSize = 13;
}
