#pragma once

#include "CoreMinimal.h"

/** 编辑器写入门的计时参数（Shuttlegent.md 第 10.3 节）。 */
namespace ShuttlegentWriteGateConstant
{
	// 请求等待确认的时长；低于运行时 60 s 的 MCP 工具调用超时，确保写入门先给出答复
	inline constexpr double ConfirmTimeoutSeconds = 55.0;

	// 检查等待中请求是否超时的间隔，单位为秒
	inline constexpr float TimeoutCheckSeconds = 0.5f;
}

/** Shuttlegent.WriteProbe：经由写入门执行的一次编辑器写入，用于 MCP 端点就绪前的验收（Shuttlegent.md 第 14.3.6 节）。 */
namespace ShuttlegentWriteProbe
{
	inline const TCHAR* CommandName = TEXT("Shuttlegent.WriteProbe");
	inline const TCHAR* CommandHelp = TEXT("Ask through the write gate to move the selected actors up: Shuttlegent.WriteProbe [ZOffsetCm=100] [FailFromIndex=-1] [DelaySeconds=0]. FailFromIndex makes that actor fail and leaves the rest unrun; DelaySeconds waits that long after confirmation before writing, so a write can be in flight when the runtime is lost.");

	// 确认卡上显示的工具名，按 MCP 工具的命名方式命名
	inline const TCHAR* ToolName = TEXT("shuttlegent_write_probe");

	// 参数位置与默认值
	inline constexpr int32 ArgZOffset = 0;
	inline constexpr int32 ArgFailFrom = 1;
	inline constexpr int32 ArgDelay = 2;
	inline constexpr float DefaultZOffset = 100.0f;
	inline constexpr int32 NoFailure = -1;
	inline constexpr float NoDelay = 0.0f;

	// 原始参数字段
	inline const TCHAR* FieldTool = TEXT("tool");
	inline const TCHAR* FieldZOffset = TEXT("zOffsetCm");
	inline const TCHAR* FieldFailFrom = TEXT("failFromIndex");
	inline const TCHAR* FieldDelay = TEXT("delaySeconds");
	inline const TCHAR* FieldActors = TEXT("actors");

	// 显示值：Z 以 cm 为单位，保留的小数位数
	inline constexpr int32 ValueFractionalDigits = 1;
}

/** 工具参数、schema 与输出上限（Shuttlegent.md 第 10.2 节）。 */
namespace ShuttlegentMcpTool
{
	// 列表类工具的分页参数及其上下限
	inline const TCHAR* ArgOffset = TEXT("offset");
	inline const TCHAR* ArgLimit = TEXT("limit");
	inline constexpr int32 DefaultPageLimit = 50;
	inline constexpr int32 MaxPageLimit = 200;

	// 单次调用返回文本的最大长度，单位为字符；低于运行时 12500 token 的溢出阈值
	inline constexpr int32 MaxOutputChars = 24000;

	// 写入结果附带的 Output Log 警告与错误：最多条数与每条的最大长度（字符）
	inline constexpr int32 MaxWriteLogLines = 20;
	inline constexpr int32 MaxWriteLogLineChars = 300;

	// JSON Schema 关键字与类型
	inline const TCHAR* SchemaType = TEXT("type");
	inline const TCHAR* SchemaProperties = TEXT("properties");
	inline const TCHAR* SchemaRequired = TEXT("required");
	inline const TCHAR* SchemaDescription = TEXT("description");
	inline const TCHAR* SchemaItems = TEXT("items");
	inline const TCHAR* SchemaEnum = TEXT("enum");
	inline const TCHAR* SchemaMinItems = TEXT("minItems");
	inline const TCHAR* SchemaMaxItems = TEXT("maxItems");
	inline const TCHAR* SchemaAdditionalProperties = TEXT("additionalProperties");
	inline const TCHAR* TypeObject = TEXT("object");
	inline const TCHAR* TypeString = TEXT("string");
	inline const TCHAR* TypeInteger = TEXT("integer");
	inline const TCHAR* TypeNumber = TEXT("number");
	inline const TCHAR* TypeBoolean = TEXT("boolean");
	inline const TCHAR* TypeArray = TEXT("array");

	// 工具名：小写字母、数字与下划线
	inline const TCHAR* NamePattern = TEXT("^[a-z0-9_]+$");

	// IShuttlegentToolProvider 在 IModularFeatures 中的名称
	inline const TCHAR* ProviderFeatureName = TEXT("ShuttlegentToolProvider");
}

/** 编辑器 MCP 端点（Shuttlegent.md 第 10.1 节）。 */
namespace ShuttlegentMcpEndpointConstant
{
	// 从 FirstPort 起依次探测以寻找空闲端口的端口范围
	inline constexpr uint32 FirstPort = 47800;
	inline constexpr uint32 PortCount = 100;

	inline const TCHAR* Path = TEXT("/mcp");
	inline const TCHAR* Host = TEXT("127.0.0.1");
	inline const TCHAR* UrlFormat = TEXT("http://{0}:{1}{2}");

	// 服务器在 ACP mcpServers 声明中的名称；运行时以它作为工具名前缀
	inline const TCHAR* ServerName = TEXT("ue");

	// Authorization 请求头及其认证方案
	inline const TCHAR* HeaderAuthorization = TEXT("Authorization");
	inline const TCHAR* BearerPrefix = TEXT("Bearer ");

	// 标明请求来自哪个对话的请求头，值为对话的 ConversationKey，随每个会话的 mcpServers 声明下发
	inline const TCHAR* HeaderConversation = TEXT("X-Shuttlegent-Conversation");

	// 日志中没有所属对话时的写法
	inline const TCHAR* LogNoConversation = TEXT("(none)");
	inline const TCHAR* ContentTypeJson = TEXT("application/json");
	inline const TCHAR* HeaderContentType = TEXT("Content-Type");

	// 客户端未指定时答复的 MCP 协议版本
	inline const TCHAR* DefaultProtocolVersion = TEXT("2025-06-18");
	inline const TCHAR* ServerInfoName = TEXT("shuttlegent-editor");

	// JSON-RPC
	inline const TCHAR* JsonRpcVersion = TEXT("2.0");
	inline constexpr int32 MethodNotFound = -32601;
	inline constexpr int32 InvalidParams = -32602;
	inline constexpr int32 ParseError = -32700;

	// MCP 方法
	inline const TCHAR* MethodInitialize = TEXT("initialize");
	inline const TCHAR* MethodToolsList = TEXT("tools/list");
	inline const TCHAR* MethodToolsCall = TEXT("tools/call");
	inline const TCHAR* MethodPing = TEXT("ping");

	// 来自 Shuttlegent context tap 运行时插件的通知：某个会话的上下文构成，以 token 计
	inline const TCHAR* MethodContextBreakdown = TEXT("shuttlegent/contextBreakdown");

	// 来自 session control 运行时插件的通知：其本机控制端口的 URL（字段 url）
	inline const TCHAR* MethodRuntimeControl = TEXT("shuttlegent/runtimeControl");

	// session control 插件的 fork 请求：把会话截在第 userMessage 条用户消息（从 0 计）之前或该轮结束之后，另存为新会话
	inline const TCHAR* MethodFork = TEXT("fork");
	inline const TCHAR* FieldUserMessage = TEXT("userMessage");
	inline const TCHAR* FieldCut = TEXT("cut");
	inline const TCHAR* CutBefore = TEXT("before");
	inline const TCHAR* CutAfter = TEXT("after");

	// fork 请求的超时，单位秒
	inline constexpr float ForkTimeoutSeconds = 30.0f;

	// fork 答复里没有新会话 id 时给出的原因
	inline const TCHAR* ForkNoSession = TEXT("The fork returned no session.");

	// 来自 session tap 运行时插件的通知：一个 ACP 不转发的会话事件（字段 sessionId、type、data）
	inline const TCHAR* MethodSessionEvent = TEXT("shuttlegent/sessionEvent");
	inline const TCHAR* FieldData = TEXT("data");

	// 来自 session control 插件的通知：计划模式下 Agent 交出了待审批的计划（字段 sessionId、reviewId、plan），以及回合取消后审批不再等待
	inline const TCHAR* MethodPlanReview = TEXT("shuttlegent/planReview");
	inline const TCHAR* MethodPlanReviewClosed = TEXT("shuttlegent/planReviewClosed");
	inline const TCHAR* FieldReviewId = TEXT("reviewId");
	inline const TCHAR* FieldPlan = TEXT("plan");

	// session control 插件的 command 请求：执行一条斜杠命令（字段 line），答复 kind（success 或 error）与 text
	inline const TCHAR* MethodCommand = TEXT("command");
	inline const TCHAR* FieldLine = TEXT("line");
	inline const TCHAR* FieldKind = TEXT("kind");
	inline const TCHAR* CommandKindSuccess = TEXT("success");

	// command 请求的超时，单位秒；/compact 要调用模型生成摘要，耗时可达数分钟
	inline constexpr float CommandTimeoutSeconds = 600.0f;

	// session control 插件的 planMode 请求：进入或退出计划模式（字段 active），答复 outcome
	inline const TCHAR* MethodPlanMode = TEXT("planMode");
	inline const TCHAR* FieldActive = TEXT("active");
	inline const TCHAR* FieldOutcome = TEXT("outcome");
	inline const TCHAR* FieldState = TEXT("state");

	// session control 插件的 answerPlanReview 请求：批准（approve）或继续规划并附意见（feedback）
	inline const TCHAR* MethodAnswerPlanReview = TEXT("answerPlanReview");
	inline const TCHAR* FieldApprove = TEXT("approve");
	inline const TCHAR* FieldFeedback = TEXT("feedback");

	// 没有界面审批计划时交回的意见，让回合不必无限等待
	inline const TCHAR* PlanReviewUnavailable = TEXT("Plan review is not available in this editor yet; do not use plan mode. Continue without presenting a plan.");

	// session control 插件的 imageInput 请求：某个模型路由（字段 model，为会话 "model" 配置项的取值）是否接受图片，答复 image
	inline const TCHAR* MethodImageInput = TEXT("imageInput");
	inline const TCHAR* FieldModel = TEXT("model");
	inline const TCHAR* FieldImage = TEXT("image");

	// session control 插件的 nextAgent 请求：下一个新会话使用的 Agent（字段 agentId），由 Agent 分发插件保留到该会话建立
	inline const TCHAR* MethodNextAgent = TEXT("nextAgent");
	inline const TCHAR* FieldAgentId = TEXT("agentId");

	// planMode 与 answerPlanReview 请求的超时，单位秒
	inline constexpr float ControlTimeoutSeconds = 10.0f;
	inline const TCHAR* FieldSessionId = TEXT("sessionId");
	inline const TCHAR* FieldSystemTokens = TEXT("systemTokens");
	inline const TCHAR* FieldToolsTokens = TEXT("toolsTokens");
	inline const TCHAR* FieldMessageTokens = TEXT("messageTokens");

	// JSON-RPC 与 MCP 字段
	inline const TCHAR* FieldJsonRpc = TEXT("jsonrpc");
	inline const TCHAR* FieldId = TEXT("id");
	inline const TCHAR* FieldMethod = TEXT("method");
	inline const TCHAR* FieldParams = TEXT("params");
	inline const TCHAR* FieldResult = TEXT("result");
	inline const TCHAR* FieldError = TEXT("error");
	inline const TCHAR* FieldCode = TEXT("code");
	inline const TCHAR* FieldMessage = TEXT("message");
	inline const TCHAR* FieldProtocolVersion = TEXT("protocolVersion");
	inline const TCHAR* FieldCapabilities = TEXT("capabilities");
	inline const TCHAR* FieldTools = TEXT("tools");
	inline const TCHAR* FieldListChanged = TEXT("listChanged");
	inline const TCHAR* FieldServerInfo = TEXT("serverInfo");
	inline const TCHAR* FieldName = TEXT("name");
	inline const TCHAR* FieldVersion = TEXT("version");
	inline const TCHAR* FieldInstructions = TEXT("instructions");
	inline const TCHAR* FieldDescription = TEXT("description");
	inline const TCHAR* FieldInputSchema = TEXT("inputSchema");
	inline const TCHAR* FieldAnnotations = TEXT("annotations");
	inline const TCHAR* FieldReadOnlyHint = TEXT("readOnlyHint");
	inline const TCHAR* FieldDestructiveHint = TEXT("destructiveHint");
	inline const TCHAR* FieldArguments = TEXT("arguments");
	inline const TCHAR* FieldContent = TEXT("content");
	inline const TCHAR* FieldType = TEXT("type");
	inline const TCHAR* FieldText = TEXT("text");
	inline const TCHAR* FieldIsError = TEXT("isError");
	inline const TCHAR* ContentTypeText = TEXT("text");
	inline const TCHAR* ContentTypeImage = TEXT("image");
	inline const TCHAR* FieldMimeType = TEXT("mimeType");

	// ACP mcpServers 声明字段
	inline const TCHAR* DeclarationTypeHttp = TEXT("http");
	inline const TCHAR* FieldUrl = TEXT("url");
	inline const TCHAR* FieldHeaders = TEXT("headers");
	inline const TCHAR* FieldValue = TEXT("value");

	// 随 initialize 发送的说明
	inline const TCHAR* Instructions = TEXT("Unreal Editor tools of the open project. Read tools answer at once; write tools ask the user to confirm in the editor (55 s limit) and run as one undoable transaction that is rolled back as a whole if any object fails. Object paths are full paths such as /Game/Maps/Main.Main:PersistentLevel.Cube_1. Unreal Engine toolsets (UMG, Niagara, PCG and more) are reached through list_toolsets, describe_toolset and call_toolset_tool. When a request is ambiguous, ask_user lets the user pick from your options in the editor; for visual or design choices (icons, colours, materials, plans, item-by-item review) ask_choices shows a card with images and notes and the answer arrives as the next user message.");
}
