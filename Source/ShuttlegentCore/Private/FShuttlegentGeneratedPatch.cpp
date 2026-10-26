#include "FShuttlegentGeneratedPatch.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "FShuttlegentProjectConfig.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentProjectSettings.h"

namespace FShuttlegentGeneratedPatchPrivate
{
	const TCHAR* ApiName(EShuttlegentProviderApi Api)
	{
		switch (Api)
		{
		case EShuttlegentProviderApi::OpenAiResponses:
			{
				return ShuttlegentProviderConstant::ApiOpenAiResponses;
			}
		case EShuttlegentProviderApi::AnthropicMessages:
			{
				return ShuttlegentProviderConstant::ApiAnthropicMessages;
			}
		case EShuttlegentProviderApi::OpenAiCompletions:
		default:
			{
				return ShuttlegentProviderConstant::ApiOpenAiCompletions;
			}
		}
	}

	TSharedRef<FJsonObject> MakeRow(const TCHAR* Id, const TSharedRef<FJsonObject>& Config)
	{
		const TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetStringField(ShuttlegentProviderConstant::FieldId, Id);
		Row->SetObjectField(ShuttlegentProviderConstant::FieldConfig, Config);
		return Row;
	}

	TSharedRef<FJsonObject> MakeProvider(const FShuttlegentProviderEntry& Entry)
	{
		TArray<TSharedPtr<FJsonValue>> Models;
		for (const FShuttlegentProviderModel& Model : Entry.Models)
		{
			if (Model.Id.IsEmpty()) continue;

			const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetStringField(ShuttlegentProviderConstant::FieldModelId, Model.Id);
			Item->SetStringField(ShuttlegentProviderConstant::FieldModelName, Model.Name.IsEmpty() ? Model.Id : Model.Name);
			Models.Add(MakeShared<FJsonValueObject>(Item));
		}

		const TSharedRef<FJsonObject> Provider = MakeShared<FJsonObject>();
		Provider->SetStringField(ShuttlegentProviderConstant::FieldDisplayName, Entry.DisplayName.IsEmpty() ? Entry.Route : Entry.DisplayName);
		Provider->SetStringField(ShuttlegentProviderConstant::FieldApiKeyEnv, Entry.KeyVariable);
		Provider->SetStringField(ShuttlegentProviderConstant::FieldApi, ApiName(Entry.Api));
		if (!Entry.BaseUrl.IsEmpty()) Provider->SetStringField(ShuttlegentProviderConstant::FieldBaseUrl, Entry.BaseUrl);
		if (!Models.IsEmpty()) Provider->SetArrayField(ShuttlegentProviderConstant::FieldModels, Models);
		return Provider;
	}
}

bool FShuttlegentGeneratedPatch::Write(const FString& DshHome, bool bShell, FString& OutPath, FString& OutError)
{
	const UShuttlegentProjectSettings* Project = GetDefault<UShuttlegentProjectSettings>();
	TArray<TSharedPtr<FJsonValue>> Rows;

	// Provider routes, keyed by route; the runtime reads each key from the variable the entry names
	const TSharedRef<FJsonObject> Providers = MakeShared<FJsonObject>();
	if (IsValid(Project))
	{
		for (const FShuttlegentProviderEntry& Entry : Project->Providers)
		{
			if (!Entry.Route.IsEmpty()) Providers->SetObjectField(Entry.Route, FShuttlegentGeneratedPatchPrivate::MakeProvider(Entry));
		}
	}
	if (!Providers->Values.IsEmpty())
	{
		const TSharedRef<FJsonObject> Config = MakeShared<FJsonObject>();
		Config->SetObjectField(ShuttlegentProviderConstant::FieldProviders, Providers);
		Rows.Add(MakeShared<FJsonValueObject>(FShuttlegentGeneratedPatchPrivate::MakeRow(ShuttlegentProviderConstant::RowLlmPiAi, Config)));
	}

	// Default route and model of new sessions; the built-in route keeps the runtime's own default
	if (IsValid(Project) && !Project->DefaultProvider.IsEmpty())
	{
		const TSharedRef<FJsonObject> Config = MakeShared<FJsonObject>();
		Config->SetStringField(ShuttlegentProviderConstant::FieldProvider, Project->DefaultProvider);
		if (!Project->DefaultModel.IsEmpty()) Config->SetStringField(ShuttlegentProviderConstant::FieldModel, Project->DefaultModel);
		Rows.Add(MakeShared<FJsonValueObject>(FShuttlegentGeneratedPatchPrivate::MakeRow(ShuttlegentProviderConstant::RowAcp, Config)));
	}

	// Only the project's skills: the runtime's default roots stay off
	TArray<TSharedPtr<FJsonValue>> SkillDirs;
	for (const FString& Dir : FShuttlegentProjectConfig::GetSkillDirs())
	{
		SkillDirs.Add(MakeShared<FJsonValueString>(Dir));
	}
	const TSharedRef<FJsonObject> SkillConfig = MakeShared<FJsonObject>();
	SkillConfig->SetBoolField(ShuttlegentProviderConstant::FieldIncludeDefaultRoots, false);
	SkillConfig->SetArrayField(ShuttlegentProviderConstant::FieldCustomSkillDirs, SkillDirs);
	Rows.Add(MakeShared<FJsonValueObject>(FShuttlegentGeneratedPatchPrivate::MakeRow(ShuttlegentProviderConstant::RowSkillFilesystem, SkillConfig)));

	// The base patch disables PowerShell; the user's Shell switch turns it back on
	const TSharedRef<FJsonObject> Shell = MakeShared<FJsonObject>();
	Shell->SetStringField(ShuttlegentProviderConstant::FieldId, ShuttlegentProviderConstant::RowToolPwsh);
	Shell->SetBoolField(ShuttlegentProviderConstant::FieldDisabled, !bShell);
	Rows.Add(MakeShared<FJsonValueObject>(Shell));

	FString Json;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
	FJsonSerializer::Serialize(Rows, Writer);

	OutPath = FPaths::Combine(DshHome, ShuttlegentProviderConstant::GeneratedPatchFile);
	if (FFileHelper::SaveStringToFile(Json, *OutPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return true;

	OutError = FString::Printf(TEXT("Cannot write the generated profile patch: %s."), *OutPath);
	return false;
}
