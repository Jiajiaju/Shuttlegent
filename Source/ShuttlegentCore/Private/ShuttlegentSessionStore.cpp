#include "ShuttlegentSessionStore.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"

namespace ShuttlegentSessionStorePrivate
{
	// MIME 类型的主次类型分隔符，以及文件名与扩展名的分隔符
	inline const TCHAR* MimeSeparator = TEXT("/");
	inline const TCHAR* ExtensionSeparator = TEXT(".");

	// 索引文件的字段
	inline const TCHAR* FieldSessions = TEXT("sessions");
	inline const TCHAR* FieldSessionId = TEXT("sessionId");
	inline const TCHAR* FieldTitle = TEXT("title");
	inline const TCHAR* FieldCwd = TEXT("cwd");
	inline const TCHAR* FieldModel = TEXT("model");
	inline const TCHAR* FieldModelName = TEXT("modelName");
	inline const TCHAR* FieldAgent = TEXT("agent");
	inline const TCHAR* FieldWriteMode = TEXT("writeMode");
	inline const TCHAR* FieldTitleFinal = TEXT("titleFinal");
	inline const TCHAR* FieldArchived = TEXT("archived");

	// writeMode 的取值，与面板页面的写入方式取值一致
	inline const TCHAR* WriteModeManual = TEXT("manual");
	inline const TCHAR* WriteModeAcceptEdits = TEXT("acceptEdits");
	inline const TCHAR* WriteModeAuto = TEXT("auto");
	inline const TCHAR* FieldCreatedAt = TEXT("createdAt");
	inline const TCHAR* FieldUpdatedAt = TEXT("updatedAt");

	// 会话记录条目的字段
	inline const TCHAR* FieldTime = TEXT("t");
	inline const TCHAR* FieldKind = TEXT("kind");
	inline const TCHAR* FieldData = TEXT("data");

	inline const TCHAR* LineTerminator = TEXT("\n");

	using FCondensedWriterFactory = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

	const TCHAR* WriteModeName(EShuttlegentWriteMode Mode)
	{
		switch (Mode)
		{
		case EShuttlegentWriteMode::AcceptEdits:
			{
				return WriteModeAcceptEdits;
			}
		case EShuttlegentWriteMode::Auto:
			{
				return WriteModeAuto;
			}
		case EShuttlegentWriteMode::Manual:
		default:
			{
				return WriteModeManual;
			}
		}
	}

	/** 认不出的取值（手改的索引、更新版本写入的新取值）视为未记录。 */
	TOptional<EShuttlegentWriteMode> ParseWriteMode(const FString& Name)
	{
		if (Name == WriteModeManual) return EShuttlegentWriteMode::Manual;
		if (Name == WriteModeAcceptEdits) return EShuttlegentWriteMode::AcceptEdits;
		if (Name == WriteModeAuto) return EShuttlegentWriteMode::Auto;
		return TOptional<EShuttlegentWriteMode>();
	}

	TSharedRef<FJsonObject> RecordToJson(const FShuttlegentSessionRecord& Record)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(FieldSessionId, Record.SessionId);
		Object->SetStringField(FieldTitle, Record.Title);
		Object->SetStringField(FieldCwd, Record.WorkingDirectory);
		Object->SetStringField(FieldModel, Record.Model);
		Object->SetStringField(FieldModelName, Record.ModelName);
		if (!Record.AgentId.IsEmpty()) Object->SetStringField(FieldAgent, Record.AgentId);
		if (Record.WriteMode.IsSet()) Object->SetStringField(FieldWriteMode, WriteModeName(Record.WriteMode.GetValue()));
		if (Record.bTitleFinal) Object->SetBoolField(FieldTitleFinal, true);
		Object->SetBoolField(FieldArchived, Record.bArchived);
		Object->SetStringField(FieldCreatedAt, Record.CreatedAt.ToIso8601());
		Object->SetStringField(FieldUpdatedAt, Record.UpdatedAt.ToIso8601());
		return Object;
	}

	bool JsonToRecord(const TSharedPtr<FJsonObject>& Object, FShuttlegentSessionRecord& OutRecord)
	{
		if (!Object.IsValid() || !Object->TryGetStringField(FieldSessionId, OutRecord.SessionId)) return false;

		Object->TryGetStringField(FieldTitle, OutRecord.Title);
		Object->TryGetStringField(FieldCwd, OutRecord.WorkingDirectory);
		Object->TryGetStringField(FieldModel, OutRecord.Model);
		Object->TryGetStringField(FieldModelName, OutRecord.ModelName);
		Object->TryGetStringField(FieldAgent, OutRecord.AgentId);
		Object->TryGetBoolField(FieldArchived, OutRecord.bArchived);
		Object->TryGetBoolField(FieldTitleFinal, OutRecord.bTitleFinal);

		FString WriteMode;
		if (Object->TryGetStringField(FieldWriteMode, WriteMode)) OutRecord.WriteMode = ParseWriteMode(WriteMode);

		FString CreatedAt;
		FString UpdatedAt;
		Object->TryGetStringField(FieldCreatedAt, CreatedAt);
		Object->TryGetStringField(FieldUpdatedAt, UpdatedAt);
		FDateTime::ParseIso8601(*CreatedAt, OutRecord.CreatedAt);
		FDateTime::ParseIso8601(*UpdatedAt, OutRecord.UpdatedAt);
		return true;
	}

	/** 会话 ID 来自运行时；只保留可安全用于文件名的字符。 */
	FString SanitizeFileName(const FString& SessionId)
	{
		FString Safe = SessionId;
		for (TCHAR& Character : Safe)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('-') && Character != TEXT('_')) Character = TEXT('_');
		}
		return Safe;
	}
}

FShuttlegentSessionStore::FShuttlegentSessionStore()
	: Directory(FPaths::Combine(FPaths::ProjectSavedDir(), ShuttlegentPath::SessionsDir))
{
}

const TArray<FShuttlegentSessionRecord>& FShuttlegentSessionStore::GetRecords()
{
	EnsureLoaded();
	return Records;
}

const FShuttlegentSessionRecord* FShuttlegentSessionStore::FindRecord(const FString& SessionId)
{
	EnsureLoaded();
	return Records.FindByPredicate([&SessionId](const FShuttlegentSessionRecord& Record) { return Record.SessionId == SessionId; });
}

void FShuttlegentSessionStore::UpsertRecord(const FShuttlegentSessionRecord& Record)
{
	EnsureLoaded();

	// 打开、恢复或修改会话配置不算新内容：它在列表中的位置不变
	const int32 ExistingIndex = Records.IndexOfByPredicate([&Record](const FShuttlegentSessionRecord& Existing) { return Existing.SessionId == Record.SessionId; });
	if (ExistingIndex != INDEX_NONE)
	{
		const FDateTime UpdatedAt = Records[ExistingIndex].UpdatedAt;
		Records[ExistingIndex] = Record;
		Records[ExistingIndex].UpdatedAt = UpdatedAt;
	}
	else
	{
		FShuttlegentSessionRecord Created = Record;
		Created.UpdatedAt = FDateTime::UtcNow();
		Records.Insert(MoveTemp(Created), 0);
	}

	SaveIndex();
}

void FShuttlegentSessionStore::TouchRecord(const FString& SessionId)
{
	EnsureLoaded();

	const int32 ExistingIndex = Records.IndexOfByPredicate([&SessionId](const FShuttlegentSessionRecord& Existing) { return Existing.SessionId == SessionId; });
	if (ExistingIndex == INDEX_NONE) return;

	FShuttlegentSessionRecord Touched = Records[ExistingIndex];
	Touched.UpdatedAt = FDateTime::UtcNow();
	Records.RemoveAt(ExistingIndex);
	Records.Insert(MoveTemp(Touched), 0);
	SaveIndex();
}

bool FShuttlegentSessionStore::DeleteRecord(const FString& SessionId)
{
	EnsureLoaded();

	const int32 ExistingIndex = Records.IndexOfByPredicate([&SessionId](const FShuttlegentSessionRecord& Existing) { return Existing.SessionId == SessionId; });
	if (ExistingIndex == INDEX_NONE) return false;

	Records.RemoveAt(ExistingIndex);
	SaveIndex();

	const FString Path = GetTranscriptPath(SessionId);
	if (IFileManager::Get().FileExists(*Path) && !IFileManager::Get().Delete(*Path)) UE_LOG(LogShuttlegent, Warning, TEXT("Deleting transcript failed: %s"), *Path);
	return true;
}

bool FShuttlegentSessionStore::RenameRecord(const FString& SessionId, const FString& Title)
{
	EnsureLoaded();

	const FString Trimmed = Title.TrimStartAndEnd();
	if (Trimmed.IsEmpty() || Trimmed.Len() > ShuttlegentSessionConstant::RenameMaxChars) return false;

	FShuttlegentSessionRecord* Record = Records.FindByPredicate([&SessionId](const FShuttlegentSessionRecord& Existing) { return Existing.SessionId == SessionId; });
	if (Record == nullptr) return false;

	Record->Title = Trimmed;
	Record->bTitleFinal = true;
	SaveIndex();
	return true;
}

bool FShuttlegentSessionStore::SetArchived(const FString& SessionId, bool bArchived)
{
	EnsureLoaded();

	FShuttlegentSessionRecord* Record = Records.FindByPredicate([&SessionId](const FShuttlegentSessionRecord& Existing) { return Existing.SessionId == SessionId; });
	if (Record == nullptr) return false;

	Record->bArchived = bArchived;
	SaveIndex();
	return true;
}

void FShuttlegentSessionStore::AppendTranscript(const FString& SessionId, const FString& Kind, const TSharedPtr<FJsonObject>& Data)
{
	if (SessionId.IsEmpty()) return;

	TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
	Entry->SetStringField(ShuttlegentSessionStorePrivate::FieldTime, FDateTime::UtcNow().ToIso8601());
	Entry->SetStringField(ShuttlegentSessionStorePrivate::FieldKind, Kind);
	Entry->SetObjectField(ShuttlegentSessionStorePrivate::FieldData, Data.IsValid() ? Data : MakeShared<FJsonObject>());

	FString Line;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = ShuttlegentSessionStorePrivate::FCondensedWriterFactory::Create(&Line);
	FJsonSerializer::Serialize(Entry, Writer);
	Line.Append(ShuttlegentSessionStorePrivate::LineTerminator);

	const FString Path = GetTranscriptPath(SessionId);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Line, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append))
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Appending to transcript failed: %s"), *Path);
	}
}

bool FShuttlegentSessionStore::WriteTranscript(const FString& SessionId, const TArray<TSharedPtr<FJsonObject>>& Entries) const
{
	if (SessionId.IsEmpty()) return false;

	FString Text;
	for (const TSharedPtr<FJsonObject>& Entry : Entries)
	{
		if (!Entry.IsValid()) continue;

		FString Line;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = ShuttlegentSessionStorePrivate::FCondensedWriterFactory::Create(&Line);
		FJsonSerializer::Serialize(Entry.ToSharedRef(), Writer);
		Text.Append(Line);
		Text.Append(ShuttlegentSessionStorePrivate::LineTerminator);
	}

	const FString Path = GetTranscriptPath(SessionId);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return true;

	UE_LOG(LogShuttlegent, Warning, TEXT("Writing transcript failed: %s"), *Path);
	return false;
}

TArray<TSharedPtr<FJsonObject>> FShuttlegentSessionStore::LoadTranscript(const FString& SessionId) const
{
	TArray<TSharedPtr<FJsonObject>> Entries;
	TArray<FString> Lines;
	if (!FFileHelper::LoadFileToStringArray(Lines, *GetTranscriptPath(SessionId))) return Entries;

	for (const FString& Line : Lines)
	{
		if (Line.IsEmpty()) continue;

		TSharedPtr<FJsonObject> Entry;
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Line);
		if (FJsonSerializer::Deserialize(Reader, Entry) && Entry.IsValid()) Entries.Add(Entry);
	}
	return Entries;
}

void FShuttlegentSessionStore::EnsureLoaded()
{
	if (bLoaded) return;
	bLoaded = true;

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *GetIndexPath())) return;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Session index is not valid JSON: %s"), *GetIndexPath());
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Sessions = nullptr;
	if (!Root->TryGetArrayField(ShuttlegentSessionStorePrivate::FieldSessions, Sessions)) return;

	for (const TSharedPtr<FJsonValue>& Value : *Sessions)
	{
		FShuttlegentSessionRecord Record;
		if (ShuttlegentSessionStorePrivate::JsonToRecord(Value->AsObject(), Record)) Records.Add(MoveTemp(Record));
	}

	Records.Sort([](const FShuttlegentSessionRecord& Left, const FShuttlegentSessionRecord& Right) { return Left.UpdatedAt > Right.UpdatedAt; });
}

void FShuttlegentSessionStore::SaveIndex() const
{
	TArray<TSharedPtr<FJsonValue>> Sessions;
	for (const FShuttlegentSessionRecord& Record : Records)
	{
		Sessions.Add(MakeShared<FJsonValueObject>(ShuttlegentSessionStorePrivate::RecordToJson(Record)));
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(ShuttlegentSessionStorePrivate::FieldSessions, Sessions);

	FString Text;
	const TSharedRef<TJsonWriter<TCHAR>> Writer = TJsonWriterFactory<TCHAR>::Create(&Text);
	FJsonSerializer::Serialize(Root, Writer);

	const FString Path = GetIndexPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (!FFileHelper::SaveStringToFile(Text, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Saving session index failed: %s"), *Path);
	}
}

FString FShuttlegentSessionStore::SaveImage(const FString& MimeType, const TArray<uint8>& Bytes) const
{
	// 扩展名取 MIME 子类型（png、jpeg、webp、gif），文件名取内容的 MD5
	FString Extension;
	MimeType.Split(ShuttlegentSessionStorePrivate::MimeSeparator, nullptr, &Extension);
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(Directory, ShuttlegentPath::SessionImagesDir, FMD5::HashBytes(Bytes.GetData(), Bytes.Num()) + ShuttlegentSessionStorePrivate::ExtensionSeparator + Extension));
	if (IFileManager::Get().FileExists(*Path)) return Path;

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	if (FFileHelper::SaveArrayToFile(Bytes, *Path)) return Path;

	UE_LOG(LogShuttlegent, Warning, TEXT("Saving prompt image failed: %s"), *Path);
	return FString();
}

FString FShuttlegentSessionStore::GetTranscriptPath(const FString& SessionId) const
{
	return FPaths::Combine(Directory, ShuttlegentSessionStorePrivate::SanitizeFileName(SessionId) + ShuttlegentPath::TranscriptExtension);
}

FString FShuttlegentSessionStore::GetIndexPath() const
{
	return FPaths::Combine(Directory, ShuttlegentPath::SessionIndexFile);
}
