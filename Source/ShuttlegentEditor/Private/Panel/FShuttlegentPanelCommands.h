#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/** Panel shortcuts; bound by the chat panel's command list, so they act only while the panel has focus. Rebindable in Editor Preferences > Keyboard Shortcuts. */
class FShuttlegentPanelCommands : public TCommands<FShuttlegentPanelCommands>
{
public:
	TSharedPtr<FUICommandInfo> ToggleSidebar;
	TSharedPtr<FUICommandInfo> ToggleInspector;
	TSharedPtr<FUICommandInfo> NewSession;
	TSharedPtr<FUICommandInfo> SearchSessions;
	TSharedPtr<FUICommandInfo> FocusInput;

public:
	FShuttlegentPanelCommands();

	//~ Begin TCommands Interface.
	virtual void RegisterCommands() override;
	//~ End TCommands Interface.
};
