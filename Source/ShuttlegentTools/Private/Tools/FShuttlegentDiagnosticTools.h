#pragma once

#include "CoreMinimal.h"

class FShuttlegentLogCapture;
class IShuttlegentTool;

/** 诊断工具：read_log（读取本模块的日志捕获）与 get_cvar。 */
struct FShuttlegentDiagnosticTools
{
	static void Append(const TSharedRef<FShuttlegentLogCapture>& LogCapture, TArray<TSharedRef<IShuttlegentTool>>& OutTools);
};
