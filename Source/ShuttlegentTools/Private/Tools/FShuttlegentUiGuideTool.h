#pragma once

#include "CoreMinimal.h"

class IShuttlegentTool;

/**
 * show_editor_ui: opens a place in the editor the project lists as a UI guide (an editor tab or a settings page) and
 * returns the guide's sentence. Built anew on every tools/list, so the guide list follows the project settings; absent
 * while the project lists none. Opens a view only, so it needs no confirmation.
 */
struct FShuttlegentUiGuideTool
{
	/** The tool, or nullptr when the project lists no UI guides. */
	static TSharedPtr<IShuttlegentTool> Make();
};
