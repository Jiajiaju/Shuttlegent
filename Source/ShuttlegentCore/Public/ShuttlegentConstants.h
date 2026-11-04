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

	// 内置 DeepSeek 路由（deepseek-official）读取密钥的变量；取值来自内置模型的密钥
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

/** 模型供应商与生成的运行时补丁。 */
namespace ShuttlegentProviderConstant
{
	// 运行时内置的 DeepSeek 路由，未指定默认供应商时使用
	inline const TCHAR* BuiltinRoute = TEXT("deepseek-official");

	// 内置路由的官方端点（Anthropic Messages）；内置模型地址的默认值
	inline const TCHAR* DeepSeekBaseUrl = TEXT("https://api.deepseek.com/anthropic");

	// DeepSeek 的 OpenAI 兼容端点；新增模型条目的默认基础 URL
	inline const TCHAR* DeepSeekOpenAiBaseUrl = TEXT("https://api.deepseek.com");

	// 自动生成的路由名：前缀 + “地址 + 协议”的 CRC（8 位十六进制）；同地址同协议而密钥不同的组再加 -2、-3
	inline const TCHAR* GeneratedRoutePrefix = TEXT("sg-");

	// 分组键与 CRC 输入中各段的分隔符
	inline const TCHAR* RouteKeySeparator = TEXT("|");

	// 路由的密钥变量：SHUTTLEGENT_<ROUTE>_API_KEY，连字符换成下划线
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

	// 后台作业工具，随 Shell 开关一同开关
	inline const TCHAR* RowToolJobs = TEXT("tool-jobs");
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

	// 插入 session control 插件的行，按绝对路径加载
	inline const TCHAR* RowSessionControl = TEXT("shuttlegent-session-control");

	// 插入 session tap 插件的行，按绝对路径加载
	inline const TCHAR* RowSessionTap = TEXT("shuttlegent-session-tap");

	// 插入记忆插件的行，按绝对路径加载
	inline const TCHAR* RowMemory = TEXT("shuttlegent-memory");

	// 默认循环所在的基础补丁行；分发插件接管工厂槽位时停用
	inline const TCHAR* RowAgentLoop = TEXT("agent-loop");

	// 插入 Agent 分发插件的行，按绝对路径加载；配置 agents 为各第三方循环的 id 与入口模块绝对路径
	inline const TCHAR* RowAgentDispatch = TEXT("shuttlegent-agent-dispatch");
	inline const TCHAR* FieldAgents = TEXT("agents");
	inline const TCHAR* FieldAgentId = TEXT("id");
	inline const TCHAR* FieldAgentEntry = TEXT("entry");
	inline const TCHAR* FieldInsert = TEXT("insert");
	inline const TCHAR* FieldPluginName = TEXT("name");
}

/**
 * 检测连接的请求。路径与运行时同源，出处相对 ThirdParty/deepseek-harness：
 * 内置 DeepSeek 见 packages/llm/llm-deepseek/src/messages-api.ts；其余三种协议由 pi-ai 交给官方 SDK，
 * 见 node_modules/.pnpm 下 @anthropic-ai/sdk 0.129.0 的 resources/messages/messages.js 与 openai 7.19.0 的
 * resources/chat/completions/completions.js、resources/responses/responses.js；SDK 把地址去掉末尾斜杠后直接拼接路径。
 */
namespace ShuttlegentProbeConstant
{
	// 内置 DeepSeek 地址已以此结尾时不再补版本段（messages-api.ts 的 messagesApiRoot）
	inline const TCHAR* VersionSegment = TEXT("/v1");

	// 各协议接在地址后的路径
	inline const TCHAR* BuiltinMessagesPath = TEXT("/messages");
	inline const TCHAR* AnthropicMessagesPath = TEXT("/v1/messages");
	inline const TCHAR* OpenAiCompletionsPath = TEXT("/chat/completions");
	inline const TCHAR* OpenAiResponsesPath = TEXT("/responses");

	// 地址末尾要去掉的斜杠
	inline const TCHAR* UrlSlash = TEXT("/");

	inline const TCHAR* Verb = TEXT("POST");

	// 请求头
	inline const TCHAR* HeaderContentType = TEXT("Content-Type");
	inline const TCHAR* ContentTypeJson = TEXT("application/json");
	inline const TCHAR* HeaderAnthropicKey = TEXT("x-api-key");
	inline const TCHAR* HeaderAnthropicVersion = TEXT("anthropic-version");

	// 运行时发送的 Anthropic API 版本（llm-deepseek adapter.ts 与 @anthropic-ai/sdk client.js）
	inline const TCHAR* AnthropicVersion = TEXT("2023-06-01");
	inline const TCHAR* HeaderAuthorization = TEXT("Authorization");
	inline const TCHAR* BearerPrefix = TEXT("Bearer ");

	// 请求体字段
	inline const TCHAR* FieldModel = TEXT("model");
	inline const TCHAR* FieldMaxTokens = TEXT("max_tokens");
	inline const TCHAR* FieldMaxOutputTokens = TEXT("max_output_tokens");
	inline const TCHAR* FieldMessages = TEXT("messages");
	inline const TCHAR* FieldRole = TEXT("role");
	inline const TCHAR* FieldContent = TEXT("content");
	inline const TCHAR* FieldInput = TEXT("input");
	inline const TCHAR* RoleUser = TEXT("user");

	// 发给模型的唯一一条消息
	inline const TCHAR* PingText = TEXT("ping");

	// 输出上限：Messages 与 Chat Completions 取 1；Responses 接口要求 max_output_tokens 不小于 16
	inline constexpr int32 MinOutputTokens = 1;
	inline constexpr int32 MinResponsesOutputTokens = 16;

	// 整个请求的超时，单位秒
	inline constexpr float TimeoutSeconds = 15.0f;

	// HTTP 状态码分类
	inline constexpr int32 StatusOkMin = 200;
	inline constexpr int32 StatusOkMax = 299;
	inline constexpr int32 StatusBadRequest = 400;
	inline constexpr int32 StatusUnauthorized = 401;
	inline constexpr int32 StatusForbidden = 403;
	inline constexpr int32 StatusNotFound = 404;
	inline constexpr int32 StatusTooManyRequests = 429;

	// 判断“返回 HTML”：响应类型含此串，或正文以尖括号开头
	inline const TCHAR* HtmlContentType = TEXT("text/html");
	inline const TCHAR* HtmlLeadingChar = TEXT("<");

	// 列出模型的接口：OpenAI 及兼容服务为 {地址}/models（Bearer），Anthropic 为 {地址}/v1/models（x-api-key）
	inline const TCHAR* OpenAiModelsPath = TEXT("/models");
	inline const TCHAR* AnthropicModelsPath = TEXT("/v1/models");
	inline const TCHAR* VerbGet = TEXT("GET");

	// 模型列表响应字段：data[] 中每项的 id，OpenAI 的 created（Unix 秒），Anthropic 的 created_at（ISO 8601）
	inline const TCHAR* FieldData = TEXT("data");
	inline const TCHAR* FieldId = TEXT("id");
	inline const TCHAR* FieldCreated = TEXT("created");
	inline const TCHAR* FieldCreatedAt = TEXT("created_at");

	// 新的 OpenAI 模型不接受 max_tokens；报错正文含此字段名时改用它重试
	inline const TCHAR* FieldMaxCompletionTokens = TEXT("max_completion_tokens");

	// 识别协议时试发的输出上限，单位 token
	inline constexpr int32 DetectOutputTokens = 16;

	// 模型 ID 含这些词（不区分大小写）的不能用于对话，不列入连接的模型
	inline const TCHAR* NonChatModelKeywords[] = {
		TEXT("embed"), TEXT("tts"), TEXT("whisper"), TEXT("transcribe"), TEXT("dall-e"), TEXT("image"),
		TEXT("moderation"), TEXT("audio"), TEXT("realtime"), TEXT("search"), TEXT("sora"), TEXT("davinci"),
		TEXT("babbage"), TEXT("computer-use"), TEXT("rerank") };

	// 写进日志的响应正文长度，单位字符；正文不含密钥
	inline constexpr int32 DetailMaxChars = 300;

	// 秒转毫秒
	inline constexpr double MillisecondsPerSecond = 1000.0;
}

/** 会话的概括标题：第一轮回答结束后向会话所用的模型要一个短标题。 */
namespace ShuttlegentTitleConstant
{
	// 发给模型的要求，{0} 为用户的第一条消息，{1} 为第一段回答
	inline const TCHAR* PromptFormat = TEXT("用不超过 16 个字概括下面这段对话的主题，作为会话标题。只输出标题本身，不要引号、句号或任何解释。\n\n用户：{0}\n\n助手：{1}");

	// 用户消息与回答各截到的长度，单位字符
	inline constexpr int32 ExcerptMaxChars = 500;

	// 输出上限，单位 token；Responses 接口要求 max_output_tokens 不小于 16
	inline constexpr int32 MaxOutputTokens = 64;

	// 整个请求的超时，单位秒
	inline constexpr float TimeoutSeconds = 20.0f;

	// 响应字段：Messages 的 content[].text，Chat Completions 的 choices[0].message.content，Responses 的 output[].content[].text
	inline const TCHAR* FieldType = TEXT("type");
	inline const TCHAR* FieldText = TEXT("text");
	inline const TCHAR* FieldChoices = TEXT("choices");
	inline const TCHAR* FieldMessage = TEXT("message");
	inline const TCHAR* FieldOutput = TEXT("output");
	inline const TCHAR* TypeText = TEXT("text");
	inline const TCHAR* TypeOutputText = TEXT("output_text");

	// 标题首尾要去掉的引号与标点
	inline const TCHAR* TrimChars = TEXT("\"'“”‘’「」《》。．.!！?？:：");
}

/** 相对插件目录的路径与用户级路径。 */
namespace ShuttlegentPath
{
	inline const TCHAR* PluginName = TEXT("Shuttlegent");

	// 运行时可执行文件，相对插件目录
	inline const TCHAR* RuntimeExecutable = TEXT("Runtime/shuttlegent-runtime.exe");

	// Shuttlegent 配置补丁，相对插件目录
	inline const TCHAR* ProfilePatch = TEXT("Runtime/profile/shuttlegent.patch.yml");

	// 插件自带的技能与斜杠模板目录，相对插件目录；排在项目目录之后，同名时项目的优先
	inline const TCHAR* BuiltinSkillsDir = TEXT("Resources/Skills");
	inline const TCHAR* BuiltinTemplatesDir = TEXT("Resources/Templates");

	// 经 ACP 启动的 dsh 配置
	inline const TCHAR* AcpProfile = TEXT("acp");

	// 向编辑器端点上报各会话上下文分项的运行时插件，相对插件目录
	inline const TCHAR* ContextTapPlugin = TEXT("Runtime/plugins/shuttlegent-context-tap.mjs");

	// 供编辑器分叉会话（回到这里、分支）的运行时插件，相对插件目录
	inline const TCHAR* SessionControlPlugin = TEXT("Runtime/plugins/shuttlegent-session-control.mjs");

	// 把 ACP 不转发的会话事件（待办、计划模式、压缩、目标）推给编辑器端点的运行时插件，相对插件目录
	inline const TCHAR* SessionTapPlugin = TEXT("Runtime/plugins/shuttlegent-session-tap.mjs");

	// 每次请求带上 Saved/Shuttlegent 下个人提示词与记忆的运行时插件，相对插件目录
	inline const TCHAR* MemoryPlugin = TEXT("Runtime/plugins/shuttlegent-memory.mjs");

	// 占据 Agent 工厂槽位、按会话的 Agent 转交各循环的运行时插件，相对插件目录
	inline const TCHAR* AgentDispatchPlugin = TEXT("Runtime/plugins/shuttlegent-agent-dispatch.mjs");

	// DSH_HOME，相对 %LOCALAPPDATA%；同时是运行时的启动目录，必须在系统盘
	inline const TCHAR* DshHome = TEXT("Shuttlegent/dsh-home");

	// DSH_AGENTS_HOME，相对 DSH_HOME
	inline const TCHAR* DshAgentsHome = TEXT("agents");

	// 会话记录与索引，相对项目 Saved 目录
	inline const TCHAR* SessionsDir = TEXT("Shuttlegent/Sessions");
	inline const TCHAR* SessionIndexFile = TEXT("index.json");

	// 提问所附图片的保存目录，相对会话目录；按内容命名，多个会话（包括分叉出的会话）可共用同一个文件
	inline const TCHAR* SessionImagesDir = TEXT("Images");
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
	// 由首条提问整理出的临时标题、以及模型生成的概括标题的最大长度，单位字符
	inline constexpr int32 TitleMaxChars = 24;

	// 用户重命名时标题的最大长度，单位字符
	inline constexpr int32 RenameMaxChars = 60;
}

/** 内置 DeepSeek 模型：运行时 llm-deepseek 自带目录中的两个模型（packages/llm/llm-deepseek/src/models.ts），设置中预装、不可删除。 */
namespace ShuttlegentBuiltinModel
{
	// 模型 ID 与运行时目录中的显示名，按预装顺序；第一个为未指定默认模型时的默认模型
	inline const TCHAR* Ids[] = { TEXT("deepseek-flash"), TEXT("deepseek-v4-pro") };
	inline const TCHAR* Names[] = { TEXT("DeepSeek-V4.1-Flash"), TEXT("DeepSeek-V4-Pro") };
}

/** Agent 的提供与选择。 */
namespace ShuttlegentAgentConstant
{
	// 默认循环 dsh-agent-loop 的 AgentId；始终存在、不可停用
	inline const TCHAR* DefaultAgentId = TEXT("default");

	// 默认循环声明的能力：全部能力项
	inline const TCHAR* AllCapabilities[] = { TEXT("rewind"), TEXT("slashCommands"), TEXT("planMode"), TEXT("todo"), TEXT("contextUsage"), TEXT("memory"), TEXT("modelSwitch"), TEXT("imageInput") };

	// 插件自带 Agent 的目录（相对插件目录），每个 Agent 一个子目录，内含说明文件
	inline const TCHAR* AgentsDir = TEXT("Runtime/plugins/agents");
	inline const TCHAR* ManifestFile = TEXT("agent.json");

	// 列出 Agent 子目录的通配符
	inline const TCHAR* DirWildcard = TEXT("*");

	// 说明文件的字段
	inline const TCHAR* FieldId = TEXT("id");
	inline const TCHAR* FieldDisplayName = TEXT("displayName");
	inline const TCHAR* FieldDescription = TEXT("description");
	inline const TCHAR* FieldCapabilities = TEXT("capabilities");
	inline const TCHAR* FieldSuggestedModel = TEXT("suggestedModel");
	inline const TCHAR* FieldEntry = TEXT("entry");
}


/** 个人记忆与本地目录（<Project>/Saved/Shuttlegent，不进版本库）。 */
namespace ShuttlegentMemoryConstant
{
	// 本地目录，相对项目的 Saved 目录；记忆文件与个人提示词都在这里
	inline const TCHAR* LocalDir = TEXT("Shuttlegent");
	inline const TCHAR* MemoryFile = TEXT("MEMORY.md");
	inline const TCHAR* LocalPromptFile = TEXT("SHUTTLEGENT.local.md");

	// 记忆文件新建时的标题行
	inline const TCHAR* FileHeading = TEXT("# Shuttlegent 记忆");

	// 每条记忆一行：以此开头，末尾为全角括号中的记录日期
	inline const TCHAR* ItemPrefix = TEXT("- ");
	inline const TCHAR* DateOpen = TEXT("（");
	inline const TCHAR* DateClose = TEXT("）");
	inline const TCHAR* DateFormat = TEXT("%Y-%m-%d");

	// 记忆文件的上限，单位字节（UTF-8）；运行时插件每次请求都带上全文
	inline constexpr int32 MaxFileBytes = 8192;

	// 一条记忆的上限，单位字符
	inline constexpr int32 MaxItemChars = 300;
}

namespace ShuttlegentAppearanceConstant
{
	// 面板页面中对话的字号，单位 CSS 像素（UShuttlegentSettings::FontSize）
	inline constexpr int32 DefaultFontSize = 14;

	// 字体设置中多个字体族之间的分隔符（UShuttlegentSettings::UiFontFamily 等）
	inline const TCHAR* FontSeparator = TEXT(",");

	// 一项字体设置最多下发给面板的字体族个数，与 Resources/Web/app.js 的 MAX_FONT_NAMES 一致
	inline constexpr int32 MaxFontNames = 4;

	// 字体族名称中除字母、数字外允许的字符，与 Resources/Web/app.js 的 FONT_NAME 一致
	inline const TCHAR* FontNameExtraChars = TEXT(" _.-");

	// 第一个非 ASCII 字符的码位；之前的字符按 FontNameExtraChars 与字母数字逐个检查
	inline constexpr TCHAR FirstNonAsciiChar = 0x80;
}
