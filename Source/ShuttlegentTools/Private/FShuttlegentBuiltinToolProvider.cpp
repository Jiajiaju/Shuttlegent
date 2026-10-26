#include "FShuttlegentBuiltinToolProvider.h"

#include "Tools/FShuttlegentAssetTools.h"
#include "Tools/FShuttlegentDiagnosticTools.h"
#include "Tools/FShuttlegentEditTools.h"
#include "Tools/FShuttlegentHistoryTools.h"
#include "Tools/FShuttlegentLevelTools.h"
#include "Tools/FShuttlegentReflectionTools.h"
#include "Tools/FShuttlegentUiGuideTool.h"

FShuttlegentBuiltinToolProvider::FShuttlegentBuiltinToolProvider(const TSharedRef<FShuttlegentLogCapture>& LogCapture)
{
	// Read tools first, write tools last, as tools/list shows them
	FShuttlegentAssetTools::Append(Tools);
	FShuttlegentLevelTools::Append(Tools);
	FShuttlegentReflectionTools::Append(Tools);
	FShuttlegentDiagnosticTools::Append(LogCapture, Tools);
	FShuttlegentEditTools::Append(Tools);
	FShuttlegentHistoryTools::Append(Tools);
}

void FShuttlegentBuiltinToolProvider::GetTools(TArray<TSharedRef<IShuttlegentTool>>& OutTools) const
{
	OutTools.Append(Tools);

	// Built per call so it follows the project's UI guide list
	const TSharedPtr<IShuttlegentTool> UiGuide = FShuttlegentUiGuideTool::Make();
	if (UiGuide.IsValid()) OutTools.Add(UiGuide.ToSharedRef());
}
