#pragma once

#include "CoreMinimal.h"

/** Names registered by the editor module. */
namespace ShuttlegentEditorName
{
	// Nomad tab of the chat panel
	inline const FName ChatTabId = TEXT("ShuttlegentChat");

	// Slate style set
	inline const FName StyleSetName = TEXT("ShuttlegentStyle");

	// Binding context of the panel shortcuts (FShuttlegentPanelCommands)
	inline const FName CommandContext = TEXT("Shuttlegent");

	// Menu extended with the "Shuttlegent" entry
	inline const TCHAR* WindowMenu = TEXT("LevelEditor.MainMenu.Window");
	inline const FName WindowMenuSection = TEXT("ShuttlegentSection");

	// Module that hosts the settings viewer
	inline const FName SettingsModule = TEXT("Settings");

	// Module of the editor endpoint, which may already be gone when the chat model shuts down
	inline const FName McpServerModule = TEXT("ShuttlegentMcpServer");

	// Settings viewer location of UShuttlegentSettings
	inline const FName SettingsContainer = TEXT("Editor");
	inline const FName SettingsCategory = TEXT("Shuttlegent");
	inline const FName SettingsSection = TEXT("General");
}

/** Shuttlegent VI 1.1 metrics of the Slate parts left around the web page, in Slate units. */
namespace ShuttlegentEditorMetric
{
	// Tab icon size required by the editor
	inline constexpr float TabIconSize = 16.0f;

	// Brand mark of the loading placeholder (empty-state mark size, Shuttlegent UI interaction spec section 14.3.3)
	inline constexpr float MarkSizePlaceholder = 64.0f;

	// Space between the placeholder mark and its caption
	inline constexpr float PlaceholderSpacing = 16.0f;

	// Placeholder caption, 12 px in the prototype, as an editor font size (pt)
	inline constexpr int32 FontCaption = 9;
}

/** Resource file names under <Plugin>/Resources. */
namespace ShuttlegentEditorResource
{
	inline const TCHAR* ResourcesDir = TEXT("Resources");
	inline const TCHAR* PngDir = TEXT("png");
	inline const TCHAR* PngExtension = TEXT(".png");

	// Brand mark for dark backgrounds; the dock tab icon uses it (VI: two-colour marks only from 32 px)
	inline const TCHAR* MarkReverse = TEXT("mark-reverse");

	// Silk warps and red shuttle; the loading placeholder shows it, matching the page's mark
	inline const TCHAR* MarkRedWhite = TEXT("mark-red-white");
}

/** Placeholder colours: the page background and caption text (Shuttlegent UI design-tokens.css --bg, --muted). */
namespace ShuttlegentEditorColor
{
	inline const FColor Background(0x1C, 0x1C, 0x1B);
	inline const FColor Muted(0xA9, 0xA8, 0xA2);
}

/** Web panel host (SShuttlegentWebPanel): page location, JS entry points and the Content Browser drawer. */
namespace ShuttlegentEditorWeb
{
	// Engine module SWebBrowser needs loaded before it is created; SWebBrowserView does not load it itself
	inline const FName WebBrowserModule = TEXT("WebBrowser");

	// Page under <Plugin>/Resources and the file URL scheme it is loaded with
	inline const TCHAR* PageDir = TEXT("Web");
	inline const TCHAR* PageFile = TEXT("index.html");
	inline const TCHAR* FileUrlPrefix = TEXT("file:///");

	// JS binding name: the page calls window.ue.bridge.<lower-case function>
	inline const TCHAR* BridgeName = TEXT("bridge");

	// CEF paint rate of the panel, frames per second
	inline constexpr int32 BrowserFrameRate = 60;

	// Page entry points called from C++, as JsCallPrefix + entry + JsCallOpen + argument + JsCallSuffix
	inline const TCHAR* JsCallPrefix = TEXT("window.Shuttlegent && window.Shuttlegent.");
	inline const TCHAR* JsCallOpen = TEXT("(");
	inline const TCHAR* JsCallSuffix = TEXT(");");
	inline const TCHAR* JsApply = TEXT("apply");
	inline const TCHAR* JsRunCommand = TEXT("runCommand");
	inline const TCHAR* JsOnReferenceResults = TEXT("onReferenceResults");
	inline const TCHAR* JsOnReferenceValidity = TEXT("onReferenceValidity");
	inline const TCHAR* JsOnSubmitResult = TEXT("onSubmitResult");
	inline const TCHAR* JsOnQueueItemTaken = TEXT("onQueueItemTaken");

	// Panel shortcut names passed to runCommand, one per FShuttlegentPanelCommands entry
	inline const TCHAR* CommandToggleSidebar = TEXT("toggleSidebar");
	inline const TCHAR* CommandToggleInspector = TEXT("toggleInspector");
	inline const TCHAR* CommandNewSession = TEXT("newSession");
	inline const TCHAR* CommandSearchSessions = TEXT("searchSessions");
	inline const TCHAR* CommandFocusInput = TEXT("focusInput");
	inline const TCHAR* JsOnDragState = TEXT("onDragState");
	inline const TCHAR* JsOnDrop = TEXT("onDrop");
	inline const TCHAR* JsOnDrawerState = TEXT("onDrawerState");
	inline const TCHAR* JsFocusDraft = TEXT("focusDraft");

	// JS literals passed to the entry points
	inline const TCHAR* JsTrue = TEXT("true");
	inline const TCHAR* JsFalse = TEXT("false");
	inline const TCHAR* JsNoArgument = TEXT("");

	// http(s) links open in the system browser instead of the panel
	inline const TCHAR* HttpPrefix = TEXT("http://");
	inline const TCHAR* HttpsPrefix = TEXT("https://");

	// Content Browser drawer hosted by the panel: module, global instance name, default height as a share of the panel and its floor
	inline const FName ContentBrowserModule = TEXT("ContentBrowser");
	inline const FName ContentBrowserInstance = TEXT("ShuttlegentContentBrowser");
	inline constexpr float DrawerHeightShare = 0.5f;
	inline constexpr float DrawerMinHeight = 240.0f;

	// Tallest drawer as a share of the panel (Shuttlegent UI interaction spec section 4.2)
	inline constexpr float DrawerMaxShare = 0.9f;

	// Height of the resize grip on the drawer's top edge
	inline constexpr float DrawerGripHeight = 6.0f;

	// Slide in / out duration, the same as the editor's status bar drawer (SWidgetDrawer.cpp, QuadOut ease)
	inline constexpr float DrawerSlideSeconds = 0.15f;

	// A page button click that just closed the drawer must not reopen it when the same click reaches the page (seconds)
	inline constexpr double ClickCloseGuardSeconds = 0.5;

	// Per-user config (GEditorPerProjectIni) holding the drawer height share, like the editor's DrawerSizes section
	inline const TCHAR* ConfigSection = TEXT("ShuttlegentPanel");
	inline const TCHAR* ConfigKeyDrawerHeightShare = TEXT("DrawerHeightShare");

	// Widths of the docked sidebar and inspector the user dragged, in CSS pixels; absent means the page's default
	inline const TCHAR* ConfigKeySidebarWidth = TEXT("SidebarWidth");
	inline const TCHAR* ConfigKeyInspectorWidth = TEXT("InspectorWidth");

	// Editor style brushes of the drawer body and its resize grip
	inline const FName DrawerBodyBrush = TEXT("Brushes.Panel");
	inline const FName DrawerGripBrush = TEXT("Brushes.Header");
}

/** Fields and values of the JSON pushed to the page (FShuttlegentWebProtocol); Resources/Web/app.js reads the same names. */
/** The personal settings page's editor tool switches. */
namespace ShuttlegentEditorSettingsUi
{
	// Category of UShuttlegentSettings::DisabledTools, where the switches replace the array
	inline const TCHAR* ToolsCategory = TEXT("Editor Tools");

	// Category of UShuttlegentSettings::DisabledToolsets, where one switch per engine toolset replaces the array
	inline const TCHAR* ToolsetsCategory = TEXT("Engine Toolsets");

	// End of the first line of a tool description, shown as the switch's tooltip
	inline const TCHAR* LineBreak = TEXT("\n");

	// Module that registers detail customizations
	inline const TCHAR* PropertyEditorModule = TEXT("PropertyEditor");
}

/** Slash templates (Shuttlegent.md section 11). */
namespace ShuttlegentEditorTemplate
{
	// Template files of a template directory
	inline const TCHAR* FilePattern = TEXT("*.md");

	// End of the description line
	inline const TCHAR* LineBreak = TEXT("\n");

	// Markdown heading mark trimmed off the description line
	inline constexpr TCHAR HeadingMark = TEXT('#');

	// Longest description shown in the / menu, in characters
	inline constexpr int32 MaxDescriptionChars = 80;
}

/** Runtime start from the editor module. */
namespace ShuttlegentEditorRuntime
{
	// Build script named by the missing-runtime guide card, relative to the plugin directory
	inline const TCHAR* BuildScript = TEXT("Build/BuildRuntime.ps1");

	// Delay after the editor finished starting before the runtime is prewarmed, so startup work is done first, in seconds
	inline constexpr float PrewarmDelaySeconds = 3.0f;
}

namespace ShuttlegentEditorWebField
{
	// Top level: full snapshot flag, status fields, entry list and order, one-shot events
	inline const TCHAR* Reset = TEXT("reset");
	inline const TCHAR* Status = TEXT("status");
	inline const TCHAR* Connected = TEXT("connected");
	inline const TCHAR* Prompting = TEXT("prompting");
	inline const TCHAR* Stopping = TEXT("stopping");

	// A turn runs, a stop is pending, or the runtime or session is coming up: a new draft is queued
	inline const TCHAR* Busy = TEXT("busy");
	inline const TCHAR* RuntimeLost = TEXT("runtimeLost");
	inline const TCHAR* AnimateMark = TEXT("animateMark");

	// Appearance settings: conversation font size in CSS pixels and the wide layout switch
	inline const TCHAR* FontSize = TEXT("fontSize");
	inline const TCHAR* WideLayout = TEXT("wideLayout");

	// Remembered pane widths, sent with the snapshot; 0 when the user never resized the pane
	inline const TCHAR* SidebarWidth = TEXT("sidebarWidth");
	inline const TCHAR* InspectorWidth = TEXT("inspectorWidth");
	inline const TCHAR* UsedTokens = TEXT("usedTokens");
	inline const TCHAR* ContextSize = TEXT("contextSize");

	// Context breakdown estimated by the runtime (system prompts, tool definitions, messages), or null before the first report
	inline const TCHAR* ContextBreakdown = TEXT("contextBreakdown");
	inline const TCHAR* SystemTokens = TEXT("system");
	inline const TCHAR* ToolsTokens = TEXT("tools");
	inline const TCHAR* MessageTokens = TEXT("messages");
	inline const TCHAR* Config = TEXT("config");

	// Slash templates, sent with the snapshot: name, description, body
	inline const TCHAR* Templates = TEXT("templates");
	inline const TCHAR* TemplateDescription = TEXT("description");
	inline const TCHAR* TemplateBody = TEXT("body");
	inline const TCHAR* Entries = TEXT("entries");
	inline const TCHAR* Order = TEXT("order");
	inline const TCHAR* Events = TEXT("events");

	// Sessions: the active and archived lists, the current session, the rename limit and the read-only permission preset
	inline const TCHAR* Sessions = TEXT("sessions");
	inline const TCHAR* Archived = TEXT("archived");
	inline const TCHAR* CurrentSessionId = TEXT("currentSessionId");
	inline const TCHAR* CurrentTitle = TEXT("currentTitle");
	inline const TCHAR* CanArchiveCurrent = TEXT("canArchiveCurrent");
	inline const TCHAR* RenameMaxChars = TEXT("renameMaxChars");
	inline const TCHAR* PermissionMode = TEXT("permissionMode");

	// The user's Shell switch
	inline const TCHAR* ShellEnabled = TEXT("shellEnabled");
	inline const TCHAR* Title = TEXT("title");
	inline const TCHAR* ModelName = TEXT("modelName");
	inline const TCHAR* UpdatedAt = TEXT("updatedAt");

	// Session item of a conversation the panel holds: its status (Status values below) and its queued prompt count
	inline const TCHAR* QueuedCount = TEXT("queuedCount");

	// Session item: whether it may be archived or deleted now (not running and not waiting for a confirmation)
	inline const TCHAR* CanArchive = TEXT("canArchive");

	// Sandbox of the runtime's file tools (UShuttlegentSettings::bAllowWorkspaceWrite)
	inline const TCHAR* PermissionReadOnly = TEXT("readOnly");
	inline const TCHAR* PermissionWorkspaceWrite = TEXT("workspaceWrite");

	// Run log of the current conversation: event time (Unix milliseconds), text, warning flag
	inline const TCHAR* RunLog = TEXT("runLog");
	inline const TCHAR* Time = TEXT("time");
	inline const TCHAR* Warning = TEXT("warning");

	// Oldest unanswered runtime permission request, or null: the operation, its target, its content and the tool's justification
	inline const TCHAR* PendingPermission = TEXT("pendingPermission");
	inline const TCHAR* Target = TEXT("target");
	inline const TCHAR* Content = TEXT("content");
	inline const TCHAR* Detail = TEXT("detail");

	// Oldest editor write waiting for confirmation, or null: tool, action, level, undoable, objects, raw arguments
	inline const TCHAR* PendingWrite = TEXT("pendingWrite");
	inline const TCHAR* Action = TEXT("action");
	inline const TCHAR* Level = TEXT("level");
	inline const TCHAR* Undoable = TEXT("undoable");
	inline const TCHAR* Destructive = TEXT("destructive");
	inline const TCHAR* Objects = TEXT("objects");
	inline const TCHAR* Raw = TEXT("raw");

	// One object of a write: what changes, from what to what, how it went
	inline const TCHAR* Field = TEXT("field");
	inline const TCHAR* CurrentValue = TEXT("current");
	inline const TCHAR* PlannedValue = TEXT("planned");
	inline const TCHAR* ObjectStatus = TEXT("status");
	inline const TCHAR* Message = TEXT("message");
	inline const TCHAR* ObjectNotRun = TEXT("notRun");
	inline const TCHAR* ObjectCompleted = TEXT("completed");
	inline const TCHAR* ObjectFailed = TEXT("failed");
	inline const TCHAR* ObjectRolledBack = TEXT("rolledBack");

	// Pending write: when it times out unanswered (Unix milliseconds)
	inline const TCHAR* ExpiresAt = TEXT("expiresAt");

	// Oldest question the agent asked (ask_user), or null: id, question, options, allowOther, expiresAt
	inline const TCHAR* PendingQuestion = TEXT("pendingQuestion");
	inline const TCHAR* Question = TEXT("question");
	inline const TCHAR* Options = TEXT("options");
	inline const TCHAR* AllowOther = TEXT("allowOther");

	// Write result entry: whether "undo this change" is available now, why not, and whether it was undone
	inline const TCHAR* CanUndo = TEXT("canUndo");
	inline const TCHAR* UndoReason = TEXT("undoReason");
	inline const TCHAR* Undone = TEXT("undone");

	// MCP card: the editor endpoint (name, listening, port, tools), engine toolsets and external servers
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

	// External server: why it is left out of the next session's declaration; empty when declared
	inline const TCHAR* Issue = TEXT("issue");
	inline const TCHAR* IssueNone = TEXT("");
	inline const TCHAR* IssueNameInvalid = TEXT("nameInvalid");
	inline const TCHAR* IssueNameTaken = TEXT("nameTaken");
	inline const TCHAR* IssueCommandNotAbsolute = TEXT("commandNotAbsolute");
	inline const TCHAR* IssueUrlMissing = TEXT("urlMissing");
	inline const TCHAR* IssueHeaderValueMissing = TEXT("headerValueMissing");

	// Config option: current value and choices
	inline const TCHAR* Current = TEXT("current");
	inline const TCHAR* Choices = TEXT("choices");
	inline const TCHAR* Value = TEXT("value");
	inline const TCHAR* Name = TEXT("name");

	// Reference (dragged, searched or sent): kind, asset object path or actor path, display name, class
	inline const TCHAR* Path = TEXT("path");
	inline const TCHAR* Class = TEXT("cls");
	inline const TCHAR* KindAsset = TEXT("asset");
	inline const TCHAR* KindActor = TEXT("actor");

	// @ menu results: the query they answer, matching actors and assets, the selection snapshot (empty query only)
	inline const TCHAR* Query = TEXT("query");
	inline const TCHAR* Actors = TEXT("actors");
	inline const TCHAR* Assets = TEXT("assets");
	inline const TCHAR* Selection = TEXT("selection");

	// Submit result: whether the prompt was sent, and the references that no longer exist
	inline const TCHAR* Accepted = TEXT("accepted");
	inline const TCHAR* Queued = TEXT("queued");
	inline const TCHAR* Invalid = TEXT("invalid");

	// User entry: the reference lines split off its prompt; queue item and taken item: the frozen references
	inline const TCHAR* References = TEXT("references");

	// Current session's queue: whether it is paused, its items; whether a failed turn can be retried
	inline const TCHAR* Queue = TEXT("queue");
	inline const TCHAR* Paused = TEXT("paused");
	inline const TCHAR* Items = TEXT("items");
	inline const TCHAR* CanRetry = TEXT("canRetry");

	// Entry fields
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

	// Entry kinds (EShuttlegentChatEntryKind)
	inline const TCHAR* KindUser = TEXT("user");
	inline const TCHAR* KindAssistant = TEXT("assistant");
	inline const TCHAR* KindToolCall = TEXT("toolCall");
	inline const TCHAR* KindPermission = TEXT("permission");
	inline const TCHAR* KindTurnEnd = TEXT("turnEnd");
	inline const TCHAR* KindNotice = TEXT("notice");
	inline const TCHAR* KindWriteResult = TEXT("writeResult");
	inline const TCHAR* KindWriteUnknown = TEXT("writeUnknown");

	// Status values (EShuttlegentChatStatus)
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

	// Start guide card (null when nothing blocks the runtime): kind, launcher reason, provider, runtime path, build script
	inline const TCHAR* StartIssue = TEXT("startIssue");
	inline const TCHAR* IssueKind = TEXT("kind");
	inline const TCHAR* IssueProvider = TEXT("provider");
	inline const TCHAR* IssueRuntimePath = TEXT("runtimePath");
	inline const TCHAR* IssueBuildScript = TEXT("buildScript");

	// Start issue kinds (EShuttlegentStartIssue)
	inline const TCHAR* IssueMissingRuntime = TEXT("missingRuntime");
	inline const TCHAR* IssueMissingKey = TEXT("missingKey");
	inline const TCHAR* IssueAuthFailed = TEXT("authFailed");
	inline const TCHAR* IssueOther = TEXT("other");

	// The current session's last resume failed; the current session has an unverified unknown write result
	inline const TCHAR* ResumeFailed = TEXT("resumeFailed");
	inline const TCHAR* NeedsVerify = TEXT("needsVerify");

	// One-shot events: a turn ended normally, or ended any other way
	inline const TCHAR* EventTurnCompleted = TEXT("turnCompleted");
	inline const TCHAR* EventTurnStopped = TEXT("turnStopped");

	// Thought duration reported for an assistant entry without timed reasoning
	inline constexpr int32 NoThoughtSeconds = -1;

	// Milliseconds per second, for the Unix times sent to the page
	inline constexpr int64 MillisecondsPerSecond = 1000;
}

/** Draft references (FShuttlegentReferenceFinder, FShuttlegentReferencePrompt; Shuttlegent UI interaction spec section 4.2). */
namespace ShuttlegentEditorReference
{
	// Module that owns the asset registry
	inline const FName AssetRegistryModule = TEXT("AssetRegistry");

	// Content root searched for @ menu assets
	inline const FName SearchRootPath = TEXT("/Game");

	// Most actors and assets the @ menu lists for one query
	inline constexpr int32 MaxActorResults = 8;
	inline constexpr int32 MaxAssetResults = 12;

	// Reference block appended to a prompt: separator, header, one line per reference ({0} kind, {1} name, {2} class, {3} path)
	inline const TCHAR* BlockSeparator = TEXT("\n\n");
	inline const TCHAR* BlockHeader = TEXT("引用的编辑器对象：");
	inline const TCHAR* LineBreak = TEXT("\n");
	inline const TCHAR* LineFormat = TEXT("- {0} {1}（{2}）：{3}");
	inline const TCHAR* KindLabelAsset = TEXT("资产");
	inline const TCHAR* KindLabelActor = TEXT("Actor");
}

/** Session config option ids the page offers as pickers. */
namespace ShuttlegentEditorConfigId
{
	inline const TCHAR* Model = TEXT("model");
	inline const TCHAR* Reasoning = TEXT("reasoning_effort");
}

/** Runtime tool names as reported in tool_call titles (dsh packages fs/tool-fs, fs/tool-fs-search, web/tool-web, subagent, todo, goal). */
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

	// Editor and external MCP tools appear as mcp__<server>__<tool>
	inline const TCHAR* McpPrefix = TEXT("mcp__");
	inline const TCHAR* McpSeparator = TEXT("__");

	// rawInput keys tried in order for the one-line object summary
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

	// An engine toolset call (call_toolset_tool, describe_toolset) is summarized as tool and toolset; the toolset shows its last name part
	inline const TCHAR* ToolsetKey = TEXT("toolset");
	inline const TCHAR* ToolKey = TEXT("tool");
	inline const TCHAR* ToolsetNameSeparator = TEXT(".");

	// A pattern summary also shows the searched path; keys ending in PathKeySuffix (or equal to SearchPathKey) hold paths
	inline const TCHAR* PatternKey = TEXT("pattern");
	inline const TCHAR* SearchPathKey = TEXT("path");
	inline const TCHAR* PathKeySuffix = TEXT("_path");

	// rawInput keys tried in order for the content line of a permission request (written text, edit replacement, command)
	inline const TCHAR* ContentKeys[] =
	{
		TEXT("content"),
		TEXT("new_string"),
		TEXT("command"),
	};

	// Prefix the runtime puts before failure text
	inline const TCHAR* ErrorPrefix = TEXT("Error:");

	// Longest summary and failure reason shown on a tool row, in characters
	inline constexpr int32 MaxSummaryChars = 80;
	inline constexpr int32 MaxReasonChars = 160;

	// Tool call status of a failed call (ACP tool_call_update)
	inline const TCHAR* StatusFailed = TEXT("failed");
}

/** Style keys registered by FShuttlegentStyle. */
namespace ShuttlegentEditorStyleKey
{
	inline const FName Mark = TEXT("Shuttlegent.Mark");
	inline const FName TabIcon = TEXT("Shuttlegent.TabIcon");
}
