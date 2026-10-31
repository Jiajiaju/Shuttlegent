#include "Tools/FShuttlegentDiagnosticTools.h"

#include "HAL/IConsoleManager.h"

#include "FShuttlegentToolBase.h"
#include "ShuttlegentToolsConstants.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"
#include "Tools/FShuttlegentLogCapture.h"

namespace FShuttlegentDiagnosticToolsPrivate
{
	// 工具名称
	inline const TCHAR* ReadLogName = TEXT("read_log");
	inline const TCHAR* GetCVarName = TEXT("get_cvar");

	// 参数名称
	inline const TCHAR* ArgLines = TEXT("lines");
	inline const TCHAR* ArgLevel = TEXT("level");
	inline const TCHAR* ArgCategory = TEXT("category");
	inline const TCHAR* ArgName = TEXT("name");
	inline const TCHAR* ArgQuery = TEXT("query");
	inline const TCHAR* ArgOffset = TEXT("offset");
	inline const TCHAR* ArgLimit = TEXT("limit");

	// read_log 的日志级别，按严重程度从高到低排列
	inline const TCHAR* LevelError = TEXT("error");
	inline const TCHAR* LevelWarning = TEXT("warning");
	inline const TCHAR* LevelLog = TEXT("log");
	inline const TCHAR* LevelVerbose = TEXT("verbose");

	// 每行日志显示的当日时间
	inline const TCHAR* TimeFormat = TEXT("%H:%M:%S");

	ELogVerbosity::Type ParseLevel(const FString& Level)
	{
		if (Level.Equals(LevelError, ESearchCase::IgnoreCase)) return ELogVerbosity::Error;
		if (Level.Equals(LevelWarning, ESearchCase::IgnoreCase)) return ELogVerbosity::Warning;
		if (Level.Equals(LevelVerbose, ESearchCase::IgnoreCase)) return ELogVerbosity::VeryVerbose;
		return ELogVerbosity::Log;
	}

	FString DescribeConsoleObject(const TCHAR* Name, IConsoleObject* Object)
	{
		IConsoleVariable* Variable = Object != nullptr ? Object->AsVariable() : nullptr;
		if (Variable == nullptr) return FString::Printf(TEXT("%s (command)"), Name);

		return FString::Printf(TEXT("%s = %s"), Name, *Variable->GetString());
	}

	class FReadLog : public FShuttlegentToolBase
	{
	private:
		TSharedRef<FShuttlegentLogCapture> LogCapture;

	public:
		explicit FReadLog(const TSharedRef<FShuttlegentLogCapture>& InLogCapture)
			: LogCapture(InLogCapture)
		{
			Definition.Name = ReadLogName;
			Definition.Description = TEXT("The latest lines of the editor's Output Log since the editor started, oldest first: time [category] verbosity: text.");
			Definition.InputSchema = FShuttlegentToolSchema()
				.Integer(ArgLines, TEXT("How many lines, 1-500. Defaults to 100."))
				.Enum(ArgLevel, TEXT("Least severe level to include. Defaults to log."), { LevelError, LevelWarning, LevelLog, LevelVerbose })
				.String(ArgCategory, TEXT("Only this log category, e.g. LogTemp, LogBlueprint; empty includes all."))
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			const int32 Count = FMath::Clamp(FShuttlegentToolArguments::GetInteger(Arguments, ArgLines, ShuttlegentToolsConstant::DefaultLogLines), 1, ShuttlegentToolsConstant::MaxLogLines);
			const ELogVerbosity::Type Level = ParseLevel(FShuttlegentToolArguments::GetString(Arguments, ArgLevel));
			const TArray<FShuttlegentLogLine> Lines = LogCapture->GetLatest(Count, Level, FShuttlegentToolArguments::GetString(Arguments, ArgCategory));
			if (Lines.IsEmpty()) return FShuttlegentToolResult::Ok(TEXT("No matching log lines."));

			TArray<FString> Texts;
			for (const FShuttlegentLogLine& Line : Lines)
			{
				Texts.Add(FString::Printf(TEXT("%s [%s] %s: %s"), *Line.Time.ToString(TimeFormat), *Line.Category.ToString(), ToString(Line.Verbosity), *Line.Text));
			}
			return FShuttlegentToolResult::Ok(FString::Printf(TEXT("%d line(s):\n"), Texts.Num()) + FString::Join(Texts, TEXT("\n")));
		}
	};

	class FGetCVar : public FShuttlegentToolBase
	{
	public:
		FGetCVar()
		{
			Definition.Name = GetCVarName;
			Definition.Description = TEXT("A console variable's value, flags and help; or, with query, the console variables and commands whose names contain it.");
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgName, TEXT("Exact console variable name, e.g. r.ScreenPercentage."))
				.String(ArgQuery, TEXT("Part of a name to search for, used when name is empty."))
				.Integer(ArgOffset, TEXT("Index of the first search result, for paging. Defaults to 0."))
				.Integer(ArgLimit, TEXT("Search results per page, 1-200. Defaults to 50."))
				.Build();
		}

		virtual FShuttlegentToolResult Run(const TSharedRef<FJsonObject>& Arguments) override
		{
			const FString Name = FShuttlegentToolArguments::GetString(Arguments, ArgName);
			if (!Name.IsEmpty())
			{
				IConsoleObject* Object = IConsoleManager::Get().FindConsoleObject(*Name);
				if (Object == nullptr) return FShuttlegentToolResult::Error(FString::Printf(TEXT("No console variable or command named %s."), *Name));

				return FShuttlegentToolResult::Ok(DescribeConsoleObject(*Name, Object) + TEXT("\n") + Object->GetHelp());
			}

			const FString Query = FShuttlegentToolArguments::GetString(Arguments, ArgQuery);
			if (Query.IsEmpty()) return FShuttlegentToolResult::Error(TEXT("Give name or query."));

			int32 Offset = 0;
			int32 Limit = 0;
			FShuttlegentToolArguments::GetPage(Arguments, Offset, Limit);

			TArray<FString> Matches;
			IConsoleManager::Get().ForEachConsoleObjectThatContains(FConsoleObjectVisitor::CreateLambda([&Matches](const TCHAR* ObjectName, IConsoleObject* Object)
			{
				Matches.Add(DescribeConsoleObject(ObjectName, Object));
			}), *Query);
			Matches.Sort();

			TArray<FString> Page;
			const int32 End = FMath::Min(Offset + Limit, Matches.Num());
			for (int32 Index = Offset; Index < End; ++Index)
			{
				Page.Add(Matches[Index]);
			}
			return FShuttlegentToolResult::Ok(FShuttlegentToolArguments::MakePageHeader(Offset, Page.Num(), Matches.Num()) + TEXT("\n") + FString::Join(Page, TEXT("\n")));
		}
	};
}

void FShuttlegentDiagnosticTools::Append(const TSharedRef<FShuttlegentLogCapture>& LogCapture, TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentDiagnosticToolsPrivate::FReadLog>(LogCapture));
	OutTools.Add(MakeShared<FShuttlegentDiagnosticToolsPrivate::FGetCVar>());
}
