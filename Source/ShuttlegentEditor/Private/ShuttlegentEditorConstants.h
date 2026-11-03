#pragma once

#include "CoreMinimal.h"

/** 编辑器模块注册的名称。 */
namespace ShuttlegentEditorName
{
	// 聊天面板的 Nomad 标签页
	inline const FName ChatTabId = TEXT("ShuttlegentChat");

	// Slate 样式集
	inline const FName StyleSetName = TEXT("ShuttlegentStyle");

	// 面板快捷键的绑定上下文（FShuttlegentPanelCommands）
	inline const FName CommandContext = TEXT("Shuttlegent");

	// 扩展了 "Shuttlegent" 条目的菜单
	inline const TCHAR* WindowMenu = TEXT("LevelEditor.MainMenu.Window");
	inline const FName WindowMenuSection = TEXT("ShuttlegentSection");

	// 承载设置查看器的模块
	inline const FName SettingsModule = TEXT("Settings");

	// 编辑器端点所在的模块，聊天模型关闭时它可能已不存在
	inline const FName McpServerModule = TEXT("ShuttlegentMcpServer");

	// UShuttlegentSettings 在设置查看器中的位置
	inline const FName SettingsContainer = TEXT("Editor");
	inline const FName SettingsCategory = TEXT("Shuttlegent");
	inline const FName SettingsSection = TEXT("General");
}

/** 网页周围保留的 Slate 部分所用的 Shuttlegent VI 1.1 尺寸，单位为 Slate 单位。 */
/** Shuttlegent 页签色条（SDockTab::TabColorScale）的颜色，与面板会话列表的提醒圆点一致。 */
namespace ShuttlegentEditorColor
{
	// 有对话完成、用户还没看：#5B9BE6
	inline const FLinearColor AttentionDone = FLinearColor(FColor(0x5B, 0x9B, 0xE6));

	// 有对话等待用户输入：#D5B57B，即面板的 --warning
	inline const FLinearColor AttentionInput = FLinearColor(FColor(0xD5, 0xB5, 0x7B));
}

namespace ShuttlegentEditorMetric
{
	// 编辑器要求的标签页图标尺寸
	inline constexpr float TabIconSize = 16.0f;

	// 加载占位的品牌标志（空状态标志尺寸，Shuttlegent UI 交互规范第 14.3.3 节）
	inline constexpr float MarkSizePlaceholder = 64.0f;

	// 占位标志与其说明文字之间的间距
	inline constexpr float PlaceholderSpacing = 16.0f;

	// 占位说明文字，原型中为 12 px，此处为编辑器字号（pt）
	inline constexpr int32 FontCaption = 9;
}

/** <Plugin>/Resources 下的资源文件名。 */
namespace ShuttlegentEditorResource
{
	inline const TCHAR* ResourcesDir = TEXT("Resources");
	inline const TCHAR* PngDir = TEXT("png");
	inline const TCHAR* PngExtension = TEXT(".png");

	// 深色背景用品牌标志；停靠标签页图标使用它（VI：双色标志仅用于 32 px 及以上）
	inline const TCHAR* MarkReverse = TEXT("mark-reverse");

	// 丝质经线与红色梭子；加载占位显示它，与页面的标志一致
	inline const TCHAR* MarkRedWhite = TEXT("mark-red-white");
}

/** 占位颜色：页面背景与说明文字（Shuttlegent UI design-tokens.css --bg、--muted）。 */
namespace ShuttlegentEditorColor
{
	inline const FColor Background(0x1C, 0x1C, 0x1B);
	inline const FColor Muted(0xA9, 0xA8, 0xA2);
}

/** 网页面板宿主（SShuttlegentWebPanel）：页面位置、JS 入口与 Content Browser 抽屉。 */
namespace ShuttlegentEditorWeb
{
	// SWebBrowser 创建前必须已加载的引擎模块；SWebBrowserView 自身不会加载它
	inline const FName WebBrowserModule = TEXT("WebBrowser");

	// <Plugin>/Resources 下的页面，以及加载它所用的文件 URL 方案
	inline const TCHAR* PageDir = TEXT("Web");
	inline const TCHAR* PageFile = TEXT("index.html");
	inline const TCHAR* FileUrlPrefix = TEXT("file:///");

	// JS 绑定名称：页面调用 window.ue.bridge.<小写函数名>
	inline const TCHAR* BridgeName = TEXT("bridge");

	// 面板的 CEF 绘制帧率，单位为帧每秒
	inline constexpr int32 BrowserFrameRate = 60;

	// 由 C++ 调用的页面入口，形式为 JsCallPrefix + 入口 + JsCallOpen + 参数 + JsCallSuffix
	inline const TCHAR* JsCallPrefix = TEXT("window.Shuttlegent && window.Shuttlegent.");
	inline const TCHAR* JsCallOpen = TEXT("(");
	inline const TCHAR* JsCallSuffix = TEXT(");");
	inline const TCHAR* JsApply = TEXT("apply");
	inline const TCHAR* JsRunCommand = TEXT("runCommand");
	inline const TCHAR* JsOnReferenceResults = TEXT("onReferenceResults");
	inline const TCHAR* JsOnReferenceValidity = TEXT("onReferenceValidity");
	inline const TCHAR* JsOnSubmitResult = TEXT("onSubmitResult");
	inline const TCHAR* JsOnQueueItemTaken = TEXT("onQueueItemTaken");
	inline const TCHAR* JsOnThumbnails = TEXT("onThumbnails");
	inline const TCHAR* JsOnChoiceRejected = TEXT("onChoiceRejected");

	// 传给 runCommand 的面板快捷键名称，与 FShuttlegentPanelCommands 的条目一一对应
	inline const TCHAR* CommandToggleSidebar = TEXT("toggleSidebar");
	inline const TCHAR* CommandToggleInspector = TEXT("toggleInspector");
	inline const TCHAR* CommandNewSession = TEXT("newSession");
	inline const TCHAR* CommandSearchSessions = TEXT("searchSessions");
	inline const TCHAR* CommandFocusInput = TEXT("focusInput");
	inline const TCHAR* JsOnDragState = TEXT("onDragState");
	inline const TCHAR* JsOnDrop = TEXT("onDrop");
	inline const TCHAR* JsOnImages = TEXT("onImages");
	inline const TCHAR* JsOnDrawerState = TEXT("onDrawerState");
	inline const TCHAR* JsFocusDraft = TEXT("focusDraft");

	// 传给入口的 JS 字面量
	inline const TCHAR* JsTrue = TEXT("true");
	inline const TCHAR* JsFalse = TEXT("false");
	inline const TCHAR* JsNoArgument = TEXT("");

	// http(s) 链接在系统浏览器而非面板中打开
	inline const TCHAR* HttpPrefix = TEXT("http://");
	inline const TCHAR* HttpsPrefix = TEXT("https://");

	// 面板承载的 Content Browser 抽屉：模块、全局实例名、占面板比例的默认高度及其下限
	inline const FName ContentBrowserModule = TEXT("ContentBrowser");
	inline const FName ContentBrowserInstance = TEXT("ShuttlegentContentBrowser");
	inline constexpr float DrawerHeightShare = 0.5f;
	inline constexpr float DrawerMinHeight = 240.0f;

	// 抽屉最大高度占面板的比例（Shuttlegent UI 交互规范第 4.2 节）
	inline constexpr float DrawerMaxShare = 0.9f;

	// 抽屉顶边调整尺寸手柄的高度
	inline constexpr float DrawerGripHeight = 6.0f;

	// 滑入 / 滑出时长，与编辑器状态栏抽屉相同（SWidgetDrawer.cpp，QuadOut 缓动）
	inline constexpr float DrawerSlideSeconds = 0.15f;

	// 刚关闭抽屉的页面按钮点击在到达页面时不得重新打开抽屉（秒）
	inline constexpr double ClickCloseGuardSeconds = 0.5;

	// 保存抽屉高度比例的按用户配置（GEditorPerProjectIni），类似编辑器的 DrawerSizes 节
	inline const TCHAR* ConfigSection = TEXT("ShuttlegentPanel");
	inline const TCHAR* ConfigKeyDrawerHeightShare = TEXT("DrawerHeightShare");

	// 用户拖拽调整的停靠侧栏与检视器宽度，单位为 CSS 像素；缺省表示使用页面默认值
	inline const TCHAR* ConfigKeySidebarWidth = TEXT("SidebarWidth");
	inline const TCHAR* ConfigKeyInspectorWidth = TEXT("InspectorWidth");

	// 抽屉主体及其调整尺寸手柄所用的编辑器样式画刷
	inline const FName DrawerBodyBrush = TEXT("Brushes.Panel");
	inline const FName DrawerGripBrush = TEXT("Brushes.Header");
}

/** 推送给页面的 JSON（FShuttlegentWebProtocol）的字段与取值；Resources/Web/app.js 读取相同的名称。 */
/** 个人设置页面中的编辑器工具开关。 */
namespace ShuttlegentEditorSettingsUi
{
	// UShuttlegentSettings::DisabledTools 所在的类别，开关在此替代该数组
	inline const TCHAR* ToolsCategory = TEXT("Editor Tools");

	// UShuttlegentSettings::DisabledToolsets 所在的类别，每个引擎 Toolset 一个开关，在此替代该数组
	inline const TCHAR* ToolsetsCategory = TEXT("Engine Toolsets");

	// 工具描述第一行的结束符，该行显示为开关的提示信息
	inline const TCHAR* LineBreak = TEXT("\n");

	// 字体设置行：值区域最小宽度（Slate 单位）
	inline constexpr float FontValueMinWidth = 260.0f;

	// 字体输入框“推荐”分组中的界面与对话字体（本机已安装的才列出；空字符串忽略）
	inline const TCHAR* TextFontPresets[] = { TEXT(""), TEXT("Microsoft YaHei UI"), TEXT("DengXian"), TEXT("Noto Sans SC"), TEXT("Noto Serif SC"), TEXT("Source Han Serif SC") };

	// 字体输入框“推荐”分组中的等宽字体
	inline const TCHAR* MonoFontPresets[] = { TEXT(""), TEXT("Cascadia Mono"), TEXT("Consolas"), TEXT("JetBrains Mono"), TEXT("Sarasa Mono SC") };

	// 注册细节面板定制的模块
	inline const TCHAR* PropertyEditorModule = TEXT("PropertyEditor");

	// 检测连接行（SShuttlegentProbeRow）：值区域最小宽度，按钮与提示文字之间、结果各行之间的间距，单位为 Slate 单位
	inline constexpr float ProbeValueMinWidth = 260.0f;
	inline constexpr float ProbeButtonGap = 8.0f;
	inline constexpr float ProbeLineGap = 2.0f;

	// 检测连接结果中连接各段的分隔符
	inline const TCHAR* ProbeFieldSeparator = TEXT(" · ");

	// 字体输入框（SShuttlegentFontField）：下拉列表最大高度、框右端为下拉箭头留出的宽度与箭头边长、列表行与分组标题的内边距，单位为 Slate 单位
	inline constexpr float FontListMaxHeight = 320.0f;
	inline constexpr float FontFieldChevronSpace = 22.0f;
	inline constexpr float FontFieldChevronSize = 12.0f;
	inline constexpr float FontFieldChevronInset = 6.0f;
	inline constexpr float FontRowPaddingX = 8.0f;
	inline constexpr float FontRowPaddingY = 3.0f;
	inline constexpr float FontWarningGap = 3.0f;

	// 字体输入框的箭头与提示图标（引擎样式）
	inline const FName FontChevronBrush = TEXT("Icons.ChevronDown");
	inline const FName FontWarningBrush = TEXT("Icons.Warning");
	inline const FName FontWarningColor = TEXT("Colors.Warning");
	inline const FName FontListBackgroundBrush = TEXT("Menu.Background");

	// 多个字体的分隔符，以及选中一项后写回时的连接符
	inline const TCHAR* FontSeparator = TEXT(",");
	inline const TCHAR* FontJoiner = TEXT(", ");

	// CSS 通用字族名，面板总能识别，不提示“未安装”
	inline const TCHAR* GenericFontFamilies[] = { TEXT("serif"), TEXT("sans-serif"), TEXT("monospace"), TEXT("system-ui"), TEXT("cursive"), TEXT("fantasy") };
}

/** 斜杠模板（Shuttlegent.md 第 11 节）。 */
namespace ShuttlegentEditorTemplate
{
	// 模板目录中的模板文件
	inline const TCHAR* FilePattern = TEXT("*.md");

	// 描述行的结束符
	inline const TCHAR* LineBreak = TEXT("\n");

	// 从描述行中去除的 Markdown 标题标记
	inline constexpr TCHAR HeadingMark = TEXT('#');

	// / 菜单中显示的描述最大长度，单位为字符
	inline constexpr int32 MaxDescriptionChars = 80;
}

/** 由编辑器模块启动运行时。 */
namespace ShuttlegentEditorRuntime
{
	// 缺少运行时引导卡片所指明的构建脚本，相对于插件目录
	inline const TCHAR* BuildScript = TEXT("Build/BuildRuntime.ps1");

	// 编辑器完成启动后到预热运行时之间的延迟，使启动工作先完成，单位为秒
	inline constexpr float PrewarmDelaySeconds = 3.0f;

	// 检查 Shuttlegent 页签是否在前台（据此清除“完成未看”提醒）的间隔，单位为秒
	inline constexpr float TabForegroundCheckSeconds = 0.25f;
}

namespace ShuttlegentEditorWebField
{
	// 顶层：完整快照标志、状态字段、条目列表与顺序、一次性事件
	inline const TCHAR* Reset = TEXT("reset");
	inline const TCHAR* Status = TEXT("status");
	inline const TCHAR* Connected = TEXT("connected");
	inline const TCHAR* Prompting = TEXT("prompting");
	inline const TCHAR* Stopping = TEXT("stopping");

	// 有回合正在运行、停止待完成，或运行时或会话正在就绪：新草稿将进入队列
	inline const TCHAR* Busy = TEXT("busy");
	inline const TCHAR* RuntimeLost = TEXT("runtimeLost");
	inline const TCHAR* AnimateMark = TEXT("animateMark");

	// 外观设置：以 CSS 像素为单位的对话字号，以及宽布局开关
	inline const TCHAR* FontSize = TEXT("fontSize");
	inline const TCHAR* WideLayout = TEXT("wideLayout");

	// 外观设置中的字体：界面、对话、等宽三项，值为清理后的逗号分隔字体族，空字符串表示默认
	inline const TCHAR* Fonts = TEXT("fonts");
	inline const TCHAR* FontUi = TEXT("ui");
	inline const TCHAR* FontChat = TEXT("chat");
	inline const TCHAR* FontMono = TEXT("mono");

	// 记录的窗格宽度，随快照发送；用户从未调整过窗格时为 0
	inline const TCHAR* SidebarWidth = TEXT("sidebarWidth");
	inline const TCHAR* InspectorWidth = TEXT("inspectorWidth");

	// 回答里可显示的本机图片所在目录（Saved/Shuttlegent/）的 file:/// 地址，以 / 结尾，随快照发送
	inline const TCHAR* LocalImageRoot = TEXT("localImageRoot");
	inline const TCHAR* UsedTokens = TEXT("usedTokens");
	inline const TCHAR* ContextSize = TEXT("contextSize");

	// 运行时估算的上下文构成（系统提示、工具定义、消息），首次上报前为 null
	inline const TCHAR* ContextBreakdown = TEXT("contextBreakdown");
	inline const TCHAR* SystemTokens = TEXT("system");
	inline const TCHAR* ToolsTokens = TEXT("tools");
	inline const TCHAR* MessageTokens = TEXT("messages");
	inline const TCHAR* Config = TEXT("config");

	// 斜杠模板，随快照发送：名称、描述、正文
	inline const TCHAR* Templates = TEXT("templates");
	inline const TCHAR* TemplateDescription = TEXT("description");
	inline const TCHAR* TemplateBody = TEXT("body");
	inline const TCHAR* TemplateBuiltin = TEXT("builtin");
	inline const TCHAR* Entries = TEXT("entries");
	inline const TCHAR* Order = TEXT("order");
	inline const TCHAR* Events = TEXT("events");

	// 会话：活动列表与已归档列表、当前会话、重命名长度上限与只读权限预设
	inline const TCHAR* Sessions = TEXT("sessions");
	inline const TCHAR* Archived = TEXT("archived");
	inline const TCHAR* CurrentSessionId = TEXT("currentSessionId");
	inline const TCHAR* CurrentTitle = TEXT("currentTitle");
	inline const TCHAR* CanArchiveCurrent = TEXT("canArchiveCurrent");
	inline const TCHAR* RenameMaxChars = TEXT("renameMaxChars");
	inline const TCHAR* PermissionMode = TEXT("permissionMode");

	// 用户的 Shell 开关
	inline const TCHAR* ShellEnabled = TEXT("shellEnabled");
	inline const TCHAR* Title = TEXT("title");
	inline const TCHAR* ModelName = TEXT("modelName");
	inline const TCHAR* UpdatedAt = TEXT("updatedAt");

	// 面板所持有对话的会话项：其状态（取值见下方 Status）及排队提示数量
	inline const TCHAR* QueuedCount = TEXT("queuedCount");

	// 会话项：当前能否归档或删除（未在运行且未在等待确认）
	inline const TCHAR* CanArchive = TEXT("canArchive");

	// 运行时文件工具的沙箱（UShuttlegentSettings::bAllowWorkspaceWrite）
	inline const TCHAR* PermissionReadOnly = TEXT("readOnly");
	inline const TCHAR* PermissionWorkspaceWrite = TEXT("workspaceWrite");

	// 当前对话的运行日志：事件时间（Unix 毫秒）、文本、警告标志
	inline const TCHAR* RunLog = TEXT("runLog");
	inline const TCHAR* Time = TEXT("time");
	inline const TCHAR* Warning = TEXT("warning");

	// 最早一个未答复的运行时权限请求，或 null：操作、目标、内容与工具的理由
	inline const TCHAR* PendingPermission = TEXT("pendingPermission");
	inline const TCHAR* Target = TEXT("target");
	inline const TCHAR* Content = TEXT("content");
	inline const TCHAR* Detail = TEXT("detail");

	// 最早一个等待确认的编辑器写入，或 null：工具、动作、级别、可否撤销、对象、原始参数
	inline const TCHAR* PendingWrite = TEXT("pendingWrite");
	inline const TCHAR* Action = TEXT("action");
	inline const TCHAR* Level = TEXT("level");
	inline const TCHAR* Undoable = TEXT("undoable");
	inline const TCHAR* Destructive = TEXT("destructive");
	inline const TCHAR* Objects = TEXT("objects");
	inline const TCHAR* Raw = TEXT("raw");

	// 一次写入中的一个对象：改动内容、由何值改为何值、执行结果
	inline const TCHAR* Field = TEXT("field");
	inline const TCHAR* CurrentValue = TEXT("current");
	inline const TCHAR* PlannedValue = TEXT("planned");
	inline const TCHAR* ObjectStatus = TEXT("status");
	inline const TCHAR* Message = TEXT("message");
	inline const TCHAR* ObjectNotRun = TEXT("notRun");
	inline const TCHAR* ObjectCompleted = TEXT("completed");
	inline const TCHAR* ObjectFailed = TEXT("failed");
	inline const TCHAR* ObjectRolledBack = TEXT("rolledBack");

	// 待处理写入：无人答复时的超时时刻（Unix 毫秒）
	inline const TCHAR* ExpiresAt = TEXT("expiresAt");

	// Agent 提出的最早一个问题（ask_user），或 null：id、question、options、allowOther、expiresAt
	inline const TCHAR* PendingQuestion = TEXT("pendingQuestion");
	inline const TCHAR* Question = TEXT("question");
	inline const TCHAR* Options = TEXT("options");
	inline const TCHAR* AllowOther = TEXT("allowOther");

	// 写入结果条目：“撤销本次修改”当前是否可用、不可用的原因，以及是否已撤销
	inline const TCHAR* CanUndo = TEXT("canUndo");
	inline const TCHAR* UndoReason = TEXT("undoReason");
	inline const TCHAR* Undone = TEXT("undone");

	// MCP 卡片：编辑器端点（名称、是否监听、端口、工具）、引擎 Toolset 与外部服务器
	inline const TCHAR* Mcp = TEXT("mcp");
	inline const TCHAR* Endpoint = TEXT("endpoint");
	inline const TCHAR* Listening = TEXT("listening");
	inline const TCHAR* Port = TEXT("port");
	inline const TCHAR* Tools = TEXT("tools");
	inline const TCHAR* Enabled = TEXT("enabled");
	inline const TCHAR* Toolsets = TEXT("toolsets");
	inline const TCHAR* ToolCount = TEXT("toolCount");
	inline const TCHAR* Blocked = TEXT("blocked");
	inline const TCHAR* Servers = TEXT("servers");
	inline const TCHAR* Stdio = TEXT("stdio");

	// 外部服务器：未列入下一个会话声明的原因；已声明时为空
	inline const TCHAR* Issue = TEXT("issue");
	inline const TCHAR* IssueNone = TEXT("");
	inline const TCHAR* IssueNameInvalid = TEXT("nameInvalid");
	inline const TCHAR* IssueNameTaken = TEXT("nameTaken");
	inline const TCHAR* IssueCommandNotAbsolute = TEXT("commandNotAbsolute");
	inline const TCHAR* IssueUrlMissing = TEXT("urlMissing");
	inline const TCHAR* IssueHeaderValueMissing = TEXT("headerValueMissing");

	// 配置选项：当前值与可选值；Agent 选项另带是否已锁定
	inline const TCHAR* Current = TEXT("current");
	inline const TCHAR* Choices = TEXT("choices");
	inline const TCHAR* Locked = TEXT("locked");
	inline const TCHAR* Value = TEXT("value");
	inline const TCHAR* Name = TEXT("name");

	// 引用（拖入、搜索或已发送）：类型、资产对象路径或 Actor 路径、显示名称、类
	inline const TCHAR* Path = TEXT("path");
	inline const TCHAR* Class = TEXT("cls");
	inline const TCHAR* KindAsset = TEXT("asset");
	inline const TCHAR* KindActor = TEXT("actor");

	// @ 菜单结果：所对应的查询、匹配的 Actor 与资产、选中项快照（仅限空查询）
	inline const TCHAR* Query = TEXT("query");
	inline const TCHAR* Actors = TEXT("actors");
	inline const TCHAR* Assets = TEXT("assets");
	inline const TCHAR* Selection = TEXT("selection");

	// 提交结果：提示是否已发送，以及已不存在的引用
	inline const TCHAR* Accepted = TEXT("accepted");
	inline const TCHAR* Queued = TEXT("queued");
	inline const TCHAR* Invalid = TEXT("invalid");

	// 用户条目：从其提示中拆出的引用行；队列条目与取回的条目：固定的引用
	inline const TCHAR* References = TEXT("references");

	// 用户消息所附图片（file:/// 地址）；页面提交的草稿图片 { mimeType, data }；能否附带图片及原因；提交被拒的原因
	inline const TCHAR* Images = TEXT("images");
	inline const TCHAR* MimeType = TEXT("mimeType");
	inline const TCHAR* Data = TEXT("data");
	inline const TCHAR* CanAttachImages = TEXT("canAttachImages");
	inline const TCHAR* AttachImagesReason = TEXT("attachImagesReason");
	inline const TCHAR* Reason = TEXT("reason");

	// 当前会话的队列：是否已暂停及其条目；失败的回合能否重试
	inline const TCHAR* Queue = TEXT("queue");
	inline const TCHAR* Paused = TEXT("paused");
	inline const TCHAR* Items = TEXT("items");
	inline const TCHAR* CanRetry = TEXT("canRetry");

	// 条目字段
	inline const TCHAR* Id = TEXT("id");
	inline const TCHAR* Kind = TEXT("kind");
	inline const TCHAR* Text = TEXT("text");
	inline const TCHAR* Thought = TEXT("thought");
	inline const TCHAR* ThoughtSeconds = TEXT("thoughtSeconds");
	inline const TCHAR* Tool = TEXT("tool");
	inline const TCHAR* DisplayName = TEXT("displayName");
	inline const TCHAR* Summary = TEXT("summary");
	inline const TCHAR* ToolStatus = TEXT("toolStatus");
	inline const TCHAR* ReadOnly = TEXT("readOnly");
	inline const TCHAR* FailureReason = TEXT("failureReason");
	inline const TCHAR* Input = TEXT("input");
	inline const TCHAR* Result = TEXT("result");
	inline const TCHAR* Answered = TEXT("answered");
	inline const TCHAR* Allowed = TEXT("allowed");
	inline const TCHAR* Error = TEXT("error");

	// 条目类型（EShuttlegentChatEntryKind）
	inline const TCHAR* KindUser = TEXT("user");
	inline const TCHAR* KindAssistant = TEXT("assistant");
	inline const TCHAR* KindToolCall = TEXT("toolCall");
	inline const TCHAR* KindPermission = TEXT("permission");
	inline const TCHAR* KindTurnEnd = TEXT("turnEnd");
	inline const TCHAR* KindNotice = TEXT("notice");
	inline const TCHAR* KindWriteResult = TEXT("writeResult");
	inline const TCHAR* KindWriteUnknown = TEXT("writeUnknown");
	inline const TCHAR* KindCompaction = TEXT("compaction");

	// 压缩条目：手动或自动、被压缩掉的 token 数（未知时为 0）、状态（running、done、failed）
	inline const TCHAR* Manual = TEXT("manual");
	inline const TCHAR* ShadowedTokens = TEXT("shadowedTokens");
	inline const TCHAR* CompactionStatus = TEXT("compactionStatus");

	// 记忆条目：动作（也是条目的 Status）、是否已撤销；读取提示的条数在 Text 中
	inline const TCHAR* KindMemory = TEXT("memory");
	inline const TCHAR* MemoryAction = TEXT("memoryAction");
	inline const TCHAR* MemoryRead = TEXT("read");
	inline const TCHAR* MemoryAdded = TEXT("added");
	inline const TCHAR* MemoryUpdated = TEXT("updated");
	inline const TCHAR* MemoryRemoved = TEXT("removed");

	// 记忆卡片：全部记忆（text、date）、文件大小与上限（字节）
	inline const TCHAR* Memory = TEXT("memory");
	inline const TCHAR* MemoryItems = TEXT("items");
	inline const TCHAR* MemoryDate = TEXT("date");
	inline const TCHAR* MemoryBytes = TEXT("bytes");
	inline const TCHAR* MemoryMaxBytes = TEXT("maxBytes");

	// 计划条目：审批状态（也是条目的 Status）与退回意见
	inline const TCHAR* KindPlan = TEXT("plan");
	inline const TCHAR* PlanStatus = TEXT("planStatus");
	inline const TCHAR* PlanFeedback = TEXT("feedback");
	inline const TCHAR* PlanPending = TEXT("pending");
	inline const TCHAR* PlanApproved = TEXT("approved");
	inline const TCHAR* PlanRevised = TEXT("revised");
	inline const TCHAR* PlanCancelled = TEXT("cancelled");

	// 选择卡条目（ask_choices）：条目上的 choice 对象，也是会话记录 choice 的内容（FShuttlegentChoiceJson）。
	// key、question、options（id、title、description、image）、multiSelect、itemNotes、overallNote、status、selected、notes、overall
	inline const TCHAR* KindChoice = TEXT("choice");
	inline const TCHAR* Choice = TEXT("choice");
	inline const TCHAR* ChoiceKey = TEXT("key");
	inline const TCHAR* ChoiceOptionDescription = TEXT("description");
	inline const TCHAR* ChoiceOptionImage = TEXT("image");
	inline const TCHAR* ChoiceMultiSelect = TEXT("multiSelect");
	inline const TCHAR* ChoiceItemNotes = TEXT("itemNotes");
	inline const TCHAR* ChoiceOverallNote = TEXT("overallNote");
	inline const TCHAR* ChoiceSelected = TEXT("selected");
	inline const TCHAR* ChoiceNotes = TEXT("notes");
	inline const TCHAR* ChoiceOverall = TEXT("overall");

	// 选择卡状态：等待作答、已提交、用户改发了别的消息、回合被停止
	inline const TCHAR* ChoicePending = TEXT("pending");
	inline const TCHAR* ChoiceAnswered = TEXT("answered");
	inline const TCHAR* ChoiceUnanswered = TEXT("unanswered");
	inline const TCHAR* ChoiceCancelled = TEXT("cancelled");

	// 计划模式：是否开启、能否切换及原因；等待审批的计划（reviewId、plan）
	inline const TCHAR* PlanMode = TEXT("planMode");
	inline const TCHAR* CanSetPlanMode = TEXT("canSetPlanMode");
	inline const TCHAR* PlanModeReason = TEXT("planModeReason");
	inline const TCHAR* PendingPlanReview = TEXT("pendingPlanReview");
	inline const TCHAR* ReviewId = TEXT("reviewId");
	inline const TCHAR* Plan = TEXT("plan");
	inline const TCHAR* StatusAwaitingPlanReview = TEXT("awaitingPlanReview");

	// 写入模式：当前对话计划模式之外的写入确认方式；写入结果条目是否按写入模式直接执行
	inline const TCHAR* WriteMode = TEXT("writeMode");
	inline const TCHAR* WriteModeManual = TEXT("manual");
	inline const TCHAR* WriteModeAcceptEdits = TEXT("acceptEdits");
	inline const TCHAR* WriteModeAuto = TEXT("auto");
	inline const TCHAR* AutoApproved = TEXT("autoApproved");

	// 会话列表的提醒圆点：done 为完成未看（蓝），input 为等待用户输入（黄）；没有提醒时不写该字段
	inline const TCHAR* Attention = TEXT("attention");
	inline const TCHAR* AttentionDone = TEXT("done");
	inline const TCHAR* AttentionInput = TEXT("input");

	// 状态取值（EShuttlegentChatStatus）
	inline const TCHAR* StatusNotConnected = TEXT("notConnected");
	inline const TCHAR* StatusConnecting = TEXT("connecting");
	inline const TCHAR* StatusConnected = TEXT("connected");
	inline const TCHAR* StatusQuerying = TEXT("querying");
	inline const TCHAR* StatusAwaitingPermission = TEXT("awaitingPermission");
	inline const TCHAR* StatusAwaitingWrite = TEXT("awaitingWrite");
	inline const TCHAR* StatusWriting = TEXT("writing");
	inline const TCHAR* StatusStopping = TEXT("stopping");
	inline const TCHAR* StatusStopped = TEXT("stopped");
	inline const TCHAR* StatusFailed = TEXT("failed");
	inline const TCHAR* StatusLost = TEXT("lost");
	inline const TCHAR* StatusResuming = TEXT("resuming");
	inline const TCHAR* StatusStartBlocked = TEXT("startBlocked");
	inline const TCHAR* StatusResumeFailed = TEXT("resumeFailed");

	// 启动引导卡片（没有阻碍运行时启动的问题时为 null）：类型、启动器给出的原因、提供方、运行时路径、构建脚本
	inline const TCHAR* StartIssue = TEXT("startIssue");
	inline const TCHAR* IssueKind = TEXT("kind");
	inline const TCHAR* IssueProvider = TEXT("provider");
	inline const TCHAR* IssueRuntimePath = TEXT("runtimePath");
	inline const TCHAR* IssueBuildScript = TEXT("buildScript");

	// 启动问题类型（EShuttlegentStartIssue）
	inline const TCHAR* IssueMissingRuntime = TEXT("missingRuntime");
	inline const TCHAR* IssueMissingKey = TEXT("missingKey");
	inline const TCHAR* IssueAuthFailed = TEXT("authFailed");
	inline const TCHAR* IssueOther = TEXT("other");

	// 当前会话上一次恢复失败；当前会话存在未核对的未知写入结果
	inline const TCHAR* ResumeFailed = TEXT("resumeFailed");
	inline const TCHAR* NeedsVerify = TEXT("needsVerify");

	// 一次性事件：回合正常结束，或以其他方式结束
	inline const TCHAR* EventTurnCompleted = TEXT("turnCompleted");

	// 回到某条消息后要放回输入框的原文（一次性）
	inline const TCHAR* RestoreDraft = TEXT("restoreDraft");

	// 当前对话能否回到某条消息或从它分支，以及不能时的原因
	inline const TCHAR* CanEditHistory = TEXT("canEditHistory");
	inline const TCHAR* EditHistoryReason = TEXT("editHistoryReason");

	// Agent 的待办清单（每项 content、status），以及手动压缩：是否进行中、能否开始及不能的原因
	inline const TCHAR* Todos = TEXT("todos");
	inline const TCHAR* TodoContent = TEXT("content");
	inline const TCHAR* TodoStatus = TEXT("status");
	inline const TCHAR* Compacting = TEXT("compacting");
	inline const TCHAR* CanCompact = TEXT("canCompact");
	inline const TCHAR* CompactReason = TEXT("compactReason");
	inline const TCHAR* EventTurnStopped = TEXT("turnStopped");

	// 没有计时推理的助手条目所报告的思考时长
	inline constexpr int32 NoThoughtSeconds = -1;

	// 每秒的毫秒数，用于发送给页面的 Unix 时间
	inline constexpr int64 MillisecondsPerSecond = 1000;
}

/** 草稿引用（FShuttlegentReferenceFinder、FShuttlegentReferencePrompt；Shuttlegent UI 交互规范第 4.2 节）。 */
namespace ShuttlegentEditorReference
{
	// 拥有资产注册表的模块
	inline const FName AssetRegistryModule = TEXT("AssetRegistry");

	// 点击 Actor 链接时切到前台的关卡编辑器所在模块
	inline const FName LevelEditorModule = TEXT("LevelEditor");

	// 为 @ 菜单搜索资产的内容根目录
	inline const FName SearchRootPath = TEXT("/Game");

	// @ 菜单针对一次查询最多列出的 Actor 与资产数量
	inline constexpr int32 MaxActorResults = 8;
	inline constexpr int32 MaxAssetResults = 12;

	// 追加到提示后的引用块：分隔符、标题、每个引用一行（{0} 类型、{1} 名称、{2} 类、{3} 路径）
	inline const TCHAR* BlockSeparator = TEXT("\n\n");
	inline const TCHAR* BlockHeader = TEXT("引用的编辑器对象：");
	inline const TCHAR* LineBreak = TEXT("\n");
	inline const TCHAR* LineFormat = TEXT("- {0} {1}（{2}）：{3}");
	inline const TCHAR* KindLabelAsset = TEXT("资产");
	inline const TCHAR* KindLabelActor = TEXT("Actor");
}

/** 回答中图片引用的资产缩略图与本机图片（FShuttlegentAssetThumbnails、FShuttlegentWebProtocol）。 */
namespace ShuttlegentEditorThumbnail
{
	// 需要现渲染时的边长，单位为像素；与 ThumbnailTools::DefaultThumbnailSize 及包内保存的缩略图一致
	inline constexpr uint32 RenderSize = 256;

	// 一次请求最多接收的资产数，与页面每页的缩略图数一致（markdown.js THUMBNAIL_PAGE；“显示下 N 张”再请求下一页）
	inline constexpr int32 MaxPerRequest = 24;

	// 每次 Core Ticker tick 处理的资产数；逐张取，避免一次请求叠成一次长卡顿
	inline constexpr int32 PerTick = 1;

	// 编码格式（FImageUtils::CompressImage 按扩展名选格式）与交给页面的 data URL 前缀
	inline const TCHAR* PngExtension = TEXT("png");
	inline const TCHAR* DataUrlPrefix = TEXT("data:image/png;base64,");

	// 回答里可显示的本机图片目录，相对于项目 Saved/；截图在 Captures/，导出在 Exports/
	inline const TCHAR* LocalImageDir = TEXT("Shuttlegent/");

	// 日志中的取图来源
	inline const TCHAR* SourceMemory = TEXT("memory cache");
	inline const TCHAR* SourcePackage = TEXT("package");
	inline const TCHAR* SourceRender = TEXT("render");
}

/** 页面以选择器形式提供的会话配置选项 id。 */
namespace ShuttlegentEditorConfigId
{
	inline const TCHAR* Model = TEXT("model");
	inline const TCHAR* Reasoning = TEXT("reasoning_effort");

	// 面板自己的选项，不发给运行时：对话的 Agent，经同一条 setconfig 通道选择
	inline const TCHAR* Agent = TEXT("agent");
}

/** tool_call 标题中报告的运行时工具名称（dsh 包 fs/tool-fs、fs/tool-fs-search、web/tool-web、subagent、todo、goal）。 */
namespace ShuttlegentEditorTool
{
	inline const TCHAR* Read = TEXT("read");
	inline const TCHAR* ReadImage = TEXT("read_image");
	inline const TCHAR* Write = TEXT("write");
	inline const TCHAR* Edit = TEXT("edit");
	inline const TCHAR* Glob = TEXT("glob");
	inline const TCHAR* Grep = TEXT("grep");
	inline const TCHAR* WebSearch = TEXT("web_search");
	inline const TCHAR* WebFetch = TEXT("web_fetch");
	inline const TCHAR* Subagent = TEXT("subagent");
	inline const TCHAR* ListSubagentModels = TEXT("list_subagent_models");
	inline const TCHAR* TodoWrite = TEXT("todo_write");
	inline const TCHAR* CreateGoal = TEXT("create_goal");
	inline const TCHAR* GetGoal = TEXT("get_goal");
	inline const TCHAR* UpdateGoal = TEXT("update_goal");

	// 编辑器与外部 MCP 工具以 mcp__<server>__<tool> 形式出现
	inline const TCHAR* McpPrefix = TEXT("mcp__");
	inline const TCHAR* McpSeparator = TEXT("__");

	// 生成单行对象摘要时依次尝试的 rawInput 键
	inline const TCHAR* SummaryKeys[] =
	{
		TEXT("file_path"),
		TEXT("path"),
		TEXT("pattern"),
		TEXT("query"),
		TEXT("url"),
		TEXT("description"),
		TEXT("title"),
	};

	// 引擎 Toolset 调用（call_toolset_tool、describe_toolset）摘要为工具与 Toolset；Toolset 仅显示其名称的最后一段
	inline const TCHAR* ToolsetKey = TEXT("toolset");
	inline const TCHAR* ToolKey = TEXT("tool");
	inline const TCHAR* ToolsetNameSeparator = TEXT(".");

	// 模式摘要同时显示搜索路径；以 PathKeySuffix 结尾（或等于 SearchPathKey）的键存放路径
	inline const TCHAR* PatternKey = TEXT("pattern");
	inline const TCHAR* SearchPathKey = TEXT("path");
	inline const TCHAR* PathKeySuffix = TEXT("_path");

	// 生成权限请求内容行时依次尝试的 rawInput 键（写入的文本、编辑替换内容、命令）
	inline const TCHAR* ContentKeys[] =
	{
		TEXT("content"),
		TEXT("new_string"),
		TEXT("command"),
	};

	// 运行时在失败文本前添加的前缀
	inline const TCHAR* ErrorPrefix = TEXT("Error:");

	// 工具行上显示的摘要与失败原因的最大长度，单位为字符
	inline constexpr int32 MaxSummaryChars = 80;
	inline constexpr int32 MaxReasonChars = 160;

	// 失败调用的工具调用状态（ACP tool_call_update）
	inline const TCHAR* StatusFailed = TEXT("failed");
}

/** FShuttlegentStyle 注册的样式键。 */
namespace ShuttlegentEditorStyleKey
{
	inline const FName Mark = TEXT("Shuttlegent.Mark");
	inline const FName TabIcon = TEXT("Shuttlegent.TabIcon");
}
