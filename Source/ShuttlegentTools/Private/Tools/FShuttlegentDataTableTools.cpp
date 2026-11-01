#include "Tools/FShuttlegentDataTableTools.h"

#include "Engine/DataTable.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#include "FShuttlegentToolBase.h"
#include "FShuttlegentToolObjects.h"
#include "Tool/FShuttlegentToolArguments.h"
#include "Tool/FShuttlegentToolSchema.h"
#include "WriteGate/FShuttlegentWriteRequest.h"

#define LOCTEXT_NAMESPACE "ShuttlegentDataTableTools"

namespace FShuttlegentDataTableToolsPrivate
{
	// 工具名称
	inline const TCHAR* ExportDataTableName = TEXT("export_data_table");

	// 参数名称
	inline const TCHAR* ArgAsset = TEXT("asset");
	inline const TCHAR* ArgFormat = TEXT("format");
	inline const TCHAR* ArgPath = TEXT("path");

	// 导出格式，即文件扩展名
	inline const TCHAR* FormatCsv = TEXT("csv");
	inline const TCHAR* FormatJson = TEXT("json");

	// 文件名与扩展名之间的分隔符
	inline const TCHAR* ExtensionSeparator = TEXT(".");

	// 未给 path 时的导出目录，相对项目的 Saved 目录
	inline const TCHAR* DefaultExportFolder = TEXT("Shuttlegent/Exports");

	/** 导出的表、格式与目标文件的完整路径。 */
	struct FExportPlan
	{
		TWeakObjectPtr<UDataTable> Table = nullptr;
		bool bCsv = true;
		FString File;
	};

	/** 目标文件：未给 path 时放在 Saved/Shuttlegent/Exports；相对路径相对项目目录；必须位于项目目录内。 */
	bool PlanExport(const TSharedRef<FJsonObject>& Arguments, FExportPlan& OutPlan, FString& OutError)
	{
		const FString Asset = FShuttlegentToolArguments::GetString(Arguments, ArgAsset);
		UDataTable* Table = Cast<UDataTable>(FShuttlegentToolObjects::ResolveAsset(Asset));
		if (!IsValid(Table))
		{
			OutError = FString::Printf(TEXT("No DataTable found at %s; nothing was exported."), *Asset);
			return false;
		}

		const FString Format = FShuttlegentToolArguments::GetString(Arguments, ArgFormat, FormatCsv);
		if (!Format.Equals(FormatCsv, ESearchCase::IgnoreCase) && !Format.Equals(FormatJson, ESearchCase::IgnoreCase))
		{
			OutError = TEXT("format must be csv or json.");
			return false;
		}

		OutPlan.Table = Table;
		OutPlan.bCsv = Format.Equals(FormatCsv, ESearchCase::IgnoreCase);
		const FString Extension = OutPlan.bCsv ? FormatCsv : FormatJson;
		FString File = FShuttlegentToolArguments::GetString(Arguments, ArgPath);
		if (File.IsEmpty()) File = FPaths::Combine(FPaths::ProjectSavedDir(), DefaultExportFolder, Table->GetName() + ExtensionSeparator + Extension);
		else if (FPaths::IsRelative(File)) File = FPaths::Combine(FPaths::ProjectDir(), File);
		if (FPaths::GetExtension(File).IsEmpty()) File += ExtensionSeparator + Extension;

		OutPlan.File = FPaths::ConvertRelativePathToFull(File);
		FPaths::NormalizeFilename(OutPlan.File);
		const FString ProjectDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
		if (!FPaths::IsUnderDirectory(OutPlan.File, ProjectDir))
		{
			OutError = FString::Printf(TEXT("%s is outside the project folder %s; export only inside the project."), *OutPlan.File, *ProjectDir);
			return false;
		}
		return true;
	}

	class FExportDataTable : public FShuttlegentToolBase
	{
	public:
		FExportDataTable()
		{
			Definition.Name = ExportDataTableName;
			Definition.Description = TEXT("Export a DataTable to a CSV or JSON file, in the format the editor's DataTable import reads back. CSV is written as UTF-8 with a BOM so Excel shows non-English text correctly. Defaults to Saved/Shuttlegent/Exports/<table name>.csv; files go only inside the project folder. The user confirms first (and is warned before an existing file is overwritten); writing the file cannot be undone. The result gives the file's full path.");
			Definition.bReadOnly = false;
			Definition.bTransactional = false;
			Definition.InputSchema = FShuttlegentToolSchema()
				.String(ArgAsset, TEXT("DataTable asset path, e.g. /Game/Data/DT_Items."), true)
				.Enum(ArgFormat, TEXT("File format. Defaults to csv."), { FormatCsv, FormatJson })
				.String(ArgPath, TEXT("File to write, absolute or relative to the project folder. Defaults to Saved/Shuttlegent/Exports/<table name>.<format>."))
				.Build();
		}

		virtual bool Preview(const TSharedRef<FJsonObject>& Arguments, FShuttlegentWriteRequest& OutRequest, FString& OutError) override
		{
			FExportPlan Plan;
			if (!PlanExport(Arguments, Plan, OutError)) return false;

			const UDataTable* Table = Plan.Table.Get();
			const bool bExists = IFileManager::Get().FileExists(*Plan.File);
			OutRequest.Action = LOCTEXT("ExportAction", "导出数据表到文件").ToString();
			OutRequest.bDestructive = bExists;

			FShuttlegentWriteObject& Item = OutRequest.Objects.AddDefaulted_GetRef();
			Item.Name = Table->GetName();
			Item.Path = Table->GetPathName();
			Item.Field = LOCTEXT("ExportField", "导出文件").ToString();
			Item.CurrentValue = bExists ? LOCTEXT("FileExists", "文件已存在，将被覆盖").ToString() : LOCTEXT("NewFile", "（新建）").ToString();
			Item.PlannedValue = FText::Format(LOCTEXT("ExportPlanned", "{0} 行 → {1}"), FText::AsNumber(Table->GetRowMap().Num()), FText::FromString(Plan.File)).ToString();
			return true;
		}

		virtual void Execute(const TSharedRef<FJsonObject>& Arguments, TArray<FShuttlegentWriteObject>& InOutObjects) override
		{
			FExportPlan Plan;
			FString Error;
			const bool bPlanned = PlanExport(Arguments, Plan, Error);
			for (FShuttlegentWriteObject& Item : InOutObjects)
			{
				const UDataTable* Table = Plan.Table.Get();
				if (!bPlanned || !IsValid(Table))
				{
					Item.Status = EShuttlegentWriteObjectStatus::Failed;
					Item.Message = Error.IsEmpty() ? LOCTEXT("TableGone", "数据表已不存在").ToString() : Error;
					continue;
				}

				// CSV 带 BOM 以便 Excel 按 UTF-8 打开；JSON 不带
				const FString Content = Plan.bCsv ? Table->GetTableAsCSV() : Table->GetTableAsJSON();
				const FFileHelper::EEncodingOptions Encoding = Plan.bCsv ? FFileHelper::EEncodingOptions::ForceUTF8 : FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM;
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(Plan.File), true);
				const bool bSaved = FFileHelper::SaveStringToFile(Content, *Plan.File, Encoding);
				Item.Status = bSaved ? EShuttlegentWriteObjectStatus::Completed : EShuttlegentWriteObjectStatus::Failed;
				Item.Message = bSaved ? Plan.File : FText::Format(LOCTEXT("ExportFailed", "无法写入 {0}：文件可能被其他程序占用或只读"), FText::FromString(Plan.File)).ToString();
			}
		}
	};
}

void FShuttlegentDataTableTools::Append(TArray<TSharedRef<IShuttlegentTool>>& OutTools)
{
	OutTools.Add(MakeShared<FShuttlegentDataTableToolsPrivate::FExportDataTable>());
}

#undef LOCTEXT_NAMESPACE
