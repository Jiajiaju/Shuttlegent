#include "ShuttlegentSessionStore.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"

namespace ShuttlegentSessionStorePrivate
{
	// Index document fields
	inline const TCHAR* FieldSessions = TEXT("sessions");
	inline const TCHAR* FieldSessionId = TEXT("sessionId");
	inline const TCHAR* FieldTitle = TEXT("title");
	inline const TCHAR* FieldCwd = TEXT("cwd");
	inline const TCHAR* FieldModel = TEXT("model");
	inline const TCHAR* FieldModelName = TEXT("modelName");
	inline const TCHAR* FieldArchived = TEXT("archived");
	inline const TCHAR* FieldCreatedAt = TEXT("createdAt");
	inline const TCHAR* FieldUpdatedAt = TEXT("updatedAt");

	// Transcript entry fields
	inline const TCHAR* FieldTime = TEXT("t");
	inline const TCHAR* FieldKind = TEXT("kind");
	inline const TCHAR* FieldData = TEXT("data");

	inline const TCHAR* LineTerminator = TEXT("\n");

	using FCondensedWriterFactory = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>;

	TSharedRef<FJsonObject> RecordToJson(const FShuttlegentSessionRecord& Record)
	{
		TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(FieldSessionId, Record.SessionId);
		Object->SetStringField(FieldTitle, Record.Title);
		Object->SetStringField(FieldCwd, Record.WorkingDirectory);
		Object->SetStringField(FieldModel, Record.Model);
		Object->SetStringField(FieldModelName, Record.ModelName);
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
		Object->TryGetBoolField(FieldArchived, OutRecord.bArchived);

		FString CreatedAt;
		FString UpdatedAt;
		Object->TryGetStringField(FieldCreatedAt, CreatedAt);
		Object->TryGetStringField(FieldUpdatedAt, UpdatedAt);
		FDateTime::ParseIso8601(*CreatedAt, OutRecord.CreatedAt);
		FDateTime::ParseIso8601(*UpdatedAt, OutRecord.UpdatedAt);
		return true;
	}

	/** Session ids come from the runtime; keep only characters that are safe in a file name. */
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

	FShuttlegentSessionRecord Updated = Record;
	Updated.UpdatedAt = FDateTime::UtcNow();

	const int32 ExistingIndex = Records.IndexOfByPredicate([&Record](const FShuttlegentSessionRecord& Existing) { return Existing.SessionId == Record.SessionId; });
	if (ExistingIndex != INDEX_NONE) Records.RemoveAt(ExistingIndex);

	Records.Insert(MoveTemp(Updated), 0);
	SaveIndex();
}

bool FShuttlegentSessionStore::RenameRecord(const FString& SessionId, const FString& Title)
{
	EnsureLoaded();

	const FString Trimmed = Title.TrimStartAndEnd();
	if (Trimmed.IsEmpty() || Trimmed.Len() > ShuttlegentSessionConstant::RenameMaxChars) return false;

	FShuttlegentSessionRecord* Record = Records.FindByPredicate([&SessionId](const FShuttlegentSessionRecord& Existing) { return Existing.SessionId == SessionId; });
	if (Record == nullptr) return false;

	Record->Title = Trimmed;
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

FString FShuttlegentSessionStore::GetTranscriptPath(const FString& SessionId) const
{
	return FPaths::Combine(Directory, ShuttlegentSessionStorePrivate::SanitizeFileName(SessionId) + ShuttlegentPath::TranscriptExtension);
}

FString FShuttlegentSessionStore::GetIndexPath() const
{
	return FPaths::Combine(Directory, ShuttlegentPath::SessionIndexFile);
}
