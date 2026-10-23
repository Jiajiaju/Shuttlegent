#include "Panel/FShuttlegentPanelCommands.h"

#include "ShuttlegentEditorConstants.h"

#define LOCTEXT_NAMESPACE "ShuttlegentPanelCommands"

FShuttlegentPanelCommands::FShuttlegentPanelCommands()
	: TCommands<FShuttlegentPanelCommands>(ShuttlegentEditorName::CommandContext, LOCTEXT("CommandContext", "Shuttlegent"), NAME_None, ShuttlegentEditorName::StyleSetName)
{
}

void FShuttlegentPanelCommands::RegisterCommands()
{
	UI_COMMAND(ToggleSidebar, "切换会话侧栏", "显示或隐藏会话侧栏", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::B));
	UI_COMMAND(ToggleInspector, "切换检视窗格", "显示或隐藏检视窗格", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::I));
	UI_COMMAND(NewSession, "新会话", "开始一个新会话", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::N));
	UI_COMMAND(SearchSessions, "搜索会话", "打开会话侧栏并聚焦搜索框", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::K));
	UI_COMMAND(FocusInput, "聚焦输入框", "把键盘焦点移到输入框", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::L));
}

#undef LOCTEXT_NAMESPACE
