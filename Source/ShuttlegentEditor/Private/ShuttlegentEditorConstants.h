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

	// Settings viewer location of UShuttlegentSettings
	inline const FName SettingsContainer = TEXT("Editor");
	inline const FName SettingsCategory = TEXT("Plugins");
	inline const FName SettingsSection = TEXT("Shuttlegent");
}

/** Shuttlegent VI 1.1 dark theme (tokens/shuttlegent.tokens.json). */
namespace ShuttlegentEditorColor
{
	inline const FColor Background(0x24, 0x23, 0x21);
	inline const FColor Surface(0x30, 0x2D, 0x2A);
	inline const FColor Raised(0x3C, 0x38, 0x34);
	inline const FColor Text(0xE8, 0xDF, 0xCF);
	inline const FColor Muted(0xB6, 0xAA, 0x98);
	inline const FColor Border(0x56, 0x50, 0x4A);
	inline const FColor Accent(0xE1, 0x7D, 0x73);
	inline const FColor Primary(0xA9, 0x47, 0x43);
	inline const FColor OnPrimary(0xFF, 0xF9, 0xEF);
	inline const FColor Success(0x8C, 0xAD, 0x92);
	inline const FColor Warning(0xD5, 0xB5, 0x7B);
	inline const FColor Error(0xE6, 0x8F, 0x82);
	inline const FColor Info(0x9E, 0xB4, 0xC1);

	// Hover tint of the primary button: brand red lightened for feedback
	inline const FColor PrimaryHovered(0xB8, 0x55, 0x50);

	// Session sidebar and inspector background (prototype.css .sidebar, .inspector)
	inline const FColor Pane(0x29, 0x27, 0x24);

	// User message bubble outline (prototype.css .message.user)
	inline const FColor BubbleBorder(0x4B, 0x43, 0x3B);

	// Composer outline, and its outline while the input has focus (prototype.css .composer)
	inline const FColor ComposerBorder(0x71, 0x60, 0x53);
	inline const FColor ComposerBorderFocused(0xA1, 0x84, 0x69);

	// Outline of the current session in the sidebar (prototype.css .session[aria-current])
	inline const FColor CurrentSessionBorder(0x64, 0x53, 0x4A);

	// Warning banner fill and bottom line (prototype.css .banner)
	inline const FColor BannerFill(0x3B, 0x30, 0x27);
	inline const FColor BannerLine(0x76, 0x60, 0x3F);

	// Outline of the stop button (prototype.css .stop)
	inline const FColor StopBorder(0x8A, 0x70, 0x64);

	// Dimming layer behind an overlay pane: black at 53 % (prototype.css .scrim #0008)
	inline const FColor Scrim(0x00, 0x00, 0x00, 0x88);
}

/** Shuttlegent VI 1.1 metrics, in Slate units. */
namespace ShuttlegentEditorMetric
{
	// VI body 14 px, caption 12 px, section 16 px, mapped to editor font sizes (pt)
	inline constexpr int32 FontBody = 10;
	inline constexpr int32 FontCaption = 9;
	inline constexpr int32 FontSection = 12;

	// Markdown heading sizes (pt): "#", "##", "###" and deeper
	inline constexpr int32 FontHeading1 = 14;
	inline constexpr int32 FontHeading2 = 12;
	inline constexpr int32 FontHeading3 = 10;

	// VI spacing unit and its multiples
	inline constexpr float Space1 = 4.0f;
	inline constexpr float Space2 = 8.0f;
	inline constexpr float Space3 = 12.0f;
	inline constexpr float Space4 = 16.0f;

	// Corner radius of controls and panels
	inline constexpr float RadiusControl = 4.0f;
	inline constexpr float RadiusPanel = 6.0f;

	// Border width
	inline constexpr float BorderWidth = 1.0f;

	// Function icons: 24 x 24 grid, 48 px PNG shown at 24 logical px; compact variant for inline rows
	inline constexpr float IconSize = 24.0f;
	inline constexpr float IconSizeSmall = 16.0f;

	// Brand mark sizes in the header and next to assistant messages
	inline constexpr float MarkSizeHeader = 24.0f;
	inline constexpr float MarkSizeInline = 16.0f;

	// Tab icon size required by the editor
	inline constexpr float TabIconSize = 16.0f;

	// Connection status dot
	inline constexpr float StatusDotSize = 8.0f;

	// Width of the model and reasoning-effort pickers
	inline constexpr float PickerWidth = 180.0f;

	// Space above the empty-state mark in the message area
	inline constexpr float EmptyStateTopPadding = 64.0f;

	// Empty-state title, 22 px in the prototype, as an editor font size (pt)
	inline constexpr int32 FontDisplay = 16;

	// Panel layout (Shuttlegent UI interaction spec section 2): pane widths and header height
	inline constexpr float SidebarWidth = 248.0f;
	inline constexpr float InspectorWidth = 360.0f;
	inline constexpr float HeaderHeight = 60.0f;

	// Widest overlay inspector between BreakpointMedium and BreakpointWide
	inline constexpr float InspectorOverlayMaxWidth = 420.0f;

	// Panel widths where the layout changes: centered conversation, docked inspector, docked sidebar
	inline constexpr float BreakpointMedium = 560.0f;
	inline constexpr float BreakpointWide = 900.0f;
	inline constexpr float BreakpointFull = 1180.0f;

	// Conversation column: widest column and the space around it
	inline constexpr float ConversationMaxWidth = 720.0f;
	inline constexpr float ConversationPaddingTop = 32.0f;
	inline constexpr float ConversationPaddingSide = 24.0f;
	inline constexpr float ConversationPaddingBottom = 24.0f;

	// Space between consecutive messages (prototype .message margin-bottom)
	inline constexpr float MessageSpacing = 24.0f;

	// Share of the conversation column a user bubble may take
	inline constexpr float UserBubbleMaxShare = 0.9f;

	// Brand mark next to the assistant name
	inline constexpr float MarkSizeAssistant = 22.0f;

	// Left rule of notes, active tab underline and current session marker
	inline constexpr float AccentLineWidth = 2.0f;

	// Width of the model picker and of the reasoning-effort picker in the composer toolbar
	inline constexpr float ComposerModelWidth = 160.0f;
	inline constexpr float ComposerReasoningWidth = 75.0f;

	// Width of popups opened from the panel: rename, archive, archived sessions
	inline constexpr float PopupWidth = 280.0f;

	// Percent scale of the usage indicator
	inline constexpr double PercentScale = 100.0;
}

/** Resource file names under <Plugin>/Resources. */
namespace ShuttlegentEditorResource
{
	inline const TCHAR* ResourcesDir = TEXT("Resources");
	inline const TCHAR* PngDir = TEXT("png");
	inline const TCHAR* PngExtension = TEXT(".png");

	// Brand mark for dark backgrounds; the dock tab icon uses it (VI: two-colour marks only from 32 px)
	inline const TCHAR* MarkReverse = TEXT("mark-reverse");

	// Silk warps and red shuttle; the panel mark, matching the rest frame of the panel's animation palette
	inline const TCHAR* MarkRedWhite = TEXT("mark-red-white");

	// Mark animation sprite sheets and their manifest, exported by Build/MarkAnim/ExportMarkAnim.ps1
	inline const TCHAR* AnimDir = TEXT("anim");
	inline const TCHAR* AnimManifest = TEXT("mark-anim.json");

	// Palette subfolder of AnimDir the panel plays (ExportMarkAnim.ps1 -Palette red-white)
	inline const TCHAR* AnimPalette = TEXT("red-white");

	// Copy icon, used by the chat panel and the Markdown code blocks
	inline const TCHAR* IconCopy = TEXT("copy");

	// Function icons registered as "Shuttlegent.Icon.<name>"
	inline const TCHAR* Icons[] =
	{
		TEXT("alert"),
		TEXT("attach"),
		TEXT("chat"),
		TEXT("check"),
		TEXT("chevron"),
		TEXT("close"),
		TEXT("copy"),
		TEXT("folder"),
		TEXT("history"),
		TEXT("plus"),
		TEXT("search"),
		TEXT("send"),
		TEXT("settings"),
		TEXT("stop"),
		TEXT("tools"),
		TEXT("undo"),
	};
}

/** Fields and values of Resources/anim/<palette>/mark-anim.json, plus how the sheets are shown. */
namespace ShuttlegentEditorMarkAnim
{
	// Manifest fields
	inline const TCHAR* FieldFps = TEXT("fps");
	inline const TCHAR* FieldPadding = TEXT("padding");
	inline const TCHAR* FieldSheets = TEXT("sheets");
	inline const TCHAR* FieldTier = TEXT("tier");
	inline const TCHAR* FieldMotion = TEXT("motion");
	inline const TCHAR* FieldFile = TEXT("file");
	inline const TCHAR* FieldFrameWidth = TEXT("frameWidth");
	inline const TCHAR* FieldFrameHeight = TEXT("frameHeight");
	inline const TCHAR* FieldFrameCount = TEXT("frameCount");
	inline const TCHAR* FieldColumns = TEXT("columns");
	inline const TCHAR* FieldWidth = TEXT("width");
	inline const TCHAR* FieldHeight = TEXT("height");
	inline const TCHAR* FieldLoop = TEXT("loop");

	// Tier values
	inline const TCHAR* TierLarge = TEXT("l");
	inline const TCHAR* TierSmall = TEXT("s");

	// Motion values
	inline const TCHAR* MotionIntro = TEXT("intro");
	inline const TCHAR* MotionWorking = TEXT("working");
	inline const TCHAR* MotionHover = TEXT("hover");
	inline const TCHAR* MotionDone = TEXT("done");

	// Sheets are exported at 2x; one frame pixel is half a Slate unit
	inline constexpr float FrameScale = 0.5f;

	// Opacity of the mark while the runtime is not connected
	inline constexpr float DisconnectedOpacity = 0.45f;
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

	// Consecutive read-only calls folded into one group from this many on
	inline constexpr int32 MinGroupSize = 2;
}

/** Style keys registered by FShuttlegentStyle. */
namespace ShuttlegentEditorStyleKey
{
	inline const TCHAR* IconPrefix = TEXT("Shuttlegent.Icon.");

	// Mark animation sheets registered as "Shuttlegent.MarkAnim.<file>"; Slate only resolves file brushes held by a registered style set
	inline const TCHAR* MarkAnimPrefix = TEXT("Shuttlegent.MarkAnim.");
	inline const FName Mark = TEXT("Shuttlegent.Mark");
	inline const FName TabIcon = TEXT("Shuttlegent.TabIcon");
	inline const FName Background = TEXT("Shuttlegent.Background");
	inline const FName Surface = TEXT("Shuttlegent.Surface");
	inline const FName Raised = TEXT("Shuttlegent.Raised");
	inline const FName Input = TEXT("Shuttlegent.Input");
	inline const FName Divider = TEXT("Shuttlegent.Divider");
	inline const FName StatusDot = TEXT("Shuttlegent.StatusDot");
	inline const FName PrimaryButton = TEXT("Shuttlegent.Button.Primary");
	inline const FName SecondaryButton = TEXT("Shuttlegent.Button.Secondary");
	inline const FName IconButton = TEXT("Shuttlegent.Button.Icon");
	inline const FName InputBox = TEXT("Shuttlegent.EditableTextBox");

	// Panel layout brushes and styles added with the M3.3 structure
	inline const FName Pane = TEXT("Shuttlegent.Pane");
	inline const FName Scrim = TEXT("Shuttlegent.Scrim");
	inline const FName AccentLine = TEXT("Shuttlegent.AccentLine");
	inline const FName UserBubble = TEXT("Shuttlegent.UserBubble");
	inline const FName Composer = TEXT("Shuttlegent.Composer");
	inline const FName ComposerFocused = TEXT("Shuttlegent.Composer.Focused");
	inline const FName Banner = TEXT("Shuttlegent.Banner");
	inline const FName BannerLine = TEXT("Shuttlegent.BannerLine");
	inline const FName DecisionCard = TEXT("Shuttlegent.DecisionCard");
	inline const FName CurrentSession = TEXT("Shuttlegent.CurrentSession");
	inline const FName ToolRow = TEXT("Shuttlegent.ToolRow");
	inline const FName StopButton = TEXT("Shuttlegent.Button.Stop");
	inline const FName FlatButton = TEXT("Shuttlegent.Button.Flat");
	inline const FName SearchBox = TEXT("Shuttlegent.SearchBox");

	// Markdown text styles; the inline ones double as rich-text tag names in FShuttlegentMarkdown markup
	inline const TCHAR* MarkdownBody = TEXT("Shuttlegent.Md.Body");
	inline const TCHAR* MarkdownBold = TEXT("Shuttlegent.Md.Bold");
	inline const TCHAR* MarkdownItalic = TEXT("Shuttlegent.Md.Italic");
	inline const TCHAR* MarkdownCode = TEXT("Shuttlegent.Md.Code");
	inline const TCHAR* MarkdownHeading1 = TEXT("Shuttlegent.Md.H1");
	inline const TCHAR* MarkdownHeading2 = TEXT("Shuttlegent.Md.H2");
	inline const TCHAR* MarkdownHeading3 = TEXT("Shuttlegent.Md.H3");
	inline const TCHAR* MarkdownQuote = TEXT("Shuttlegent.Md.Quote");
	inline const TCHAR* MarkdownCodeBlock = TEXT("Shuttlegent.Md.CodeBlock");

	// Hyperlink style and decorator id of Markdown links
	inline const TCHAR* MarkdownLink = TEXT("Shuttlegent.Md.Link");
	inline const TCHAR* MarkdownLinkId = TEXT("link");
}
