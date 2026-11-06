#include "Web/FShuttlegentPanelCommands.h"

#include "ShuttlegentEditorConstants.h"

#define LOCTEXT_NAMESPACE "ShuttlegentPanelCommands"

FShuttlegentPanelCommands::FShuttlegentPanelCommands()
	: TCommands<FShuttlegentPanelCommands>(ShuttlegentEditorName::CommandContext, LOCTEXT("CommandContext", "Shuttlegent"), NAME_None, ShuttlegentEditorName::StyleSetName)
{
}

void FShuttlegentPanelCommands::RegisterCommands()
{
	UI_COMMAND(ToggleSidebar, "Toggle Session Sidebar", "Show or hide the session sidebar", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::B));
	UI_COMMAND(ToggleInspector, "Toggle Inspector Pane", "Show or hide the inspector pane", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::I));
	UI_COMMAND(NewSession, "New Session", "Start a new session", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::N));
	UI_COMMAND(SearchSessions, "Search Sessions", "Open the session sidebar and focus the search box", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::K));
	UI_COMMAND(FocusInput, "Focus Input Box", "Move keyboard focus to the input box", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::L));
	UI_COMMAND(ReloadPage, "Reload Panel Page", "Reload the panel page from disk; conversations, sessions, queues and saved drafts are not affected (drafts are saved about 1 second after typing pauses)", EUserInterfaceActionType::Button, FInputChord(EModifierKey::Control, EKeys::R));
}

#undef LOCTEXT_NAMESPACE
