#include "FShuttlegentBuiltinToolProvider.h"

#include "Tools/FShuttlegentAssetTools.h"
#include "Tools/FShuttlegentDataTableTools.h"
#include "Tools/FShuttlegentDiagnosticTools.h"
#include "Tools/FShuttlegentEditTools.h"
#include "Tools/FShuttlegentGraphTools.h"
#include "Tools/FShuttlegentHistoryTools.h"
#include "Tools/FShuttlegentInteractionTools.h"
#include "Tools/FShuttlegentLevelBuildTools.h"
#include "Tools/FShuttlegentLevelTools.h"
#include "Tools/FShuttlegentMaterialTools.h"
#include "Tools/FShuttlegentMemoryTools.h"
#include "Tools/FShuttlegentReflectionTools.h"
#include "Tools/FShuttlegentUiGuideTool.h"

FShuttlegentBuiltinToolProvider::FShuttlegentBuiltinToolProvider(const TSharedRef<FShuttlegentLogCapture>& LogCapture)
{
	// 先读取类工具、后写入类工具，与 tools/list 的展示顺序一致
	FShuttlegentAssetTools::Append(Tools);
	FShuttlegentLevelTools::Append(Tools);
	FShuttlegentReflectionTools::Append(Tools);
	FShuttlegentDiagnosticTools::Append(LogCapture, Tools);
	FShuttlegentInteractionTools::Append(Tools);
	FShuttlegentMemoryTools::Append(Tools);
	FShuttlegentMaterialTools::AppendRead(Tools);
	FShuttlegentEditTools::Append(Tools);
	FShuttlegentMaterialTools::AppendWrite(Tools);
	FShuttlegentGraphTools::Append(Tools);
	FShuttlegentLevelBuildTools::Append(Tools);
	FShuttlegentDataTableTools::Append(Tools);
	FShuttlegentHistoryTools::Append(Tools);
}

void FShuttlegentBuiltinToolProvider::GetTools(TArray<TSharedRef<IShuttlegentTool>>& OutTools) const
{
	OutTools.Append(Tools);

	// 每次调用时重新构建，以跟随项目的 UI 指引列表
	const TSharedPtr<IShuttlegentTool> UiGuide = FShuttlegentUiGuideTool::Make();
	if (UiGuide.IsValid()) OutTools.Add(UiGuide.ToSharedRef());
}
