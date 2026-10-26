#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/** Panel shortcuts; matched by the web panel host before keys reach the page, so they act only while the panel has focus. Rebindable in Editor Preferences > Keyboard Shortcuts. */
class FShuttlegentPanelCommands : public TCommands<FShuttlegentPanelCommands>
{
public:
	TSharedPtr<FUICommandInfo> ToggleSidebar;
	TSharedPtr<FUICommandInfo> ToggleInspector;
	TSharedPtr<FUICommandInfo> NewSession;
	TSharedPtr<FUICommandInfo> SearchSessions;
	TSharedPtr<FUICommandInfo> FocusInput;

	/** Handled by the host itself: reloads the page from disk, for checking edits to Resources/Web without restarting the editor. */
	TSharedPtr<FUICommandInfo> ReloadPage;

public:
	FShuttlegentPanelCommands();

	//~ Begin TCommands Interface.
	virtual void RegisterCommands() override;
	//~ End TCommands Interface.
};
