#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

/** 面板快捷键；由网页面板宿主在按键到达页面之前匹配，因此仅在面板拥有焦点时生效。可在 Editor Preferences > Keyboard Shortcuts 中重新绑定。 */
class FShuttlegentPanelCommands : public TCommands<FShuttlegentPanelCommands>
{
public:
	TSharedPtr<FUICommandInfo> ToggleSidebar;
	TSharedPtr<FUICommandInfo> ToggleInspector;
	TSharedPtr<FUICommandInfo> NewSession;
	TSharedPtr<FUICommandInfo> SearchSessions;
	TSharedPtr<FUICommandInfo> FocusInput;

	/** 由宿主自行处理：从磁盘重新加载页面，用于在不重启编辑器的情况下检查对 Resources/Web 的修改。 */
	TSharedPtr<FUICommandInfo> ReloadPage;

public:
	FShuttlegentPanelCommands();

	//~ Begin TCommands Interface.
	virtual void RegisterCommands() override;
	//~ End TCommands Interface.
};
