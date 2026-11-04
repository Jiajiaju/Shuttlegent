#include "Chat/FShuttlegentDraftStore.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "ShuttlegentConstants.h"
#include "ShuttlegentEditorConstants.h"
#include "ShuttlegentLog.h"
#include "Web/FShuttlegentWebJson.h"

namespace FShuttlegentDraftStorePrivate
{
	constexpr int32 MaxKeyPartChars = 128;

	bool IsValidKeyPart(const FString& Part)
	{
		if (Part.IsEmpty() || Part.Len() > MaxKeyPartChars) return false;
		for (const TCHAR Char : Part)
		{
			if (!FChar::IsAlnum(Char) && Char != TEXT('-') && Char != TEXT('_')) return false;
		}
		return true;
	}

	FString ViewPrefix()
	{
		return FString(ShuttlegentEditorDraft::ViewsDir) + TEXT("/");
	}

	TSharedRef<FJsonObject> MakeQueuedJson(const FShuttlegentQueuedPrompt& Item)
	{
		const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
		Object->SetStringField(ShuttlegentEditorWebField::Text, Item.Text);
		Object->SetArrayField(ShuttlegentEditorWebField::References, FShuttlegentWebJson::MakeReferences(Item.References));
		Object->SetStringField(ShuttlegentEditorDraft::FieldModel, Item.ModelValue);
		Object->SetStringField(ShuttlegentEditorDraft::FieldReasoning, Item.ReasoningValue);
		return Object;
	}
}

FShuttlegentDraftStore::FShuttlegentDraftStore()
	: Directory(FPaths::Combine(FPaths::ProjectSavedDir(), ShuttlegentPath::SessionsDir, ShuttlegentEditorDraft::DraftsDir))
{
}

FString FShuttlegentDraftStore::MakeViewKey(const FString& ViewKey)
{
	return FShuttlegentDraftStorePrivate::ViewPrefix() + ViewKey;
}

bool FShuttlegentDraftStore::IsValidKey(const FString& Key)
{
	const FString Prefix = FShuttlegentDraftStorePrivate::ViewPrefix();
	const FString Part = Key.StartsWith(Prefix, ESearchCase::CaseSensitive) ? Key.RightChop(Prefix.Len()) : Key;
	return FShuttlegentDraftStorePrivate::IsValidKeyPart(Part);
}

TSharedPtr<FJsonObject> FShuttlegentDraftStore::GetDraft(const FString& Key) const
{
	if (!IsValidKey(Key)) return nullptr;
	return Find(Key).Draft;
}

bool FShuttlegentDraftStore::SetDraftJson(const FString& Key, const FString& Json)
{
	if (!IsValidKey(Key) || Json.Len() > ShuttlegentEditorDraft::MaxDraftJsonChars) return false;

	TSharedPtr<FJsonObject> Draft;
	if (!Json.IsEmpty())
	{
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Draft) || !Draft.IsValid()) return false;
	}

	Find(Key);
	FEntry& Entry = Cache.FindChecked(Key);
	if (!Draft.IsValid() && !Entry.Draft.IsValid()) return true;

	Entry.Draft = Draft;
	Write(Key);
	return true;
}

void FShuttlegentDraftStore::MoveDraft(const FString& FromKey, const FString& ToKey)
{
	if (!IsValidKey(FromKey) || !IsValidKey(ToKey) || FromKey == ToKey) return;

	const TSharedPtr<FJsonObject> Moved = Find(FromKey).Draft;
	if (!Moved.IsValid()) return;

	Find(ToKey);
	FEntry& Target = Cache.FindChecked(ToKey);
	if (!Target.Draft.IsValid())
	{
		Target.Draft = Moved;
		Write(ToKey);
	}
	Cache.FindChecked(FromKey).Draft.Reset();
	Write(FromKey);
}

TArray<FShuttlegentQueuedPrompt> FShuttlegentDraftStore::GetQueue(const FString& SessionId) const
{
	TArray<FShuttlegentQueuedPrompt> Items;
	if (!IsValidKey(SessionId)) return Items;

	for (const TSharedPtr<FJsonValue>& Value : Find(SessionId).Queue)
	{
		const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Object.IsValid()) continue;

		FShuttlegentQueuedPrompt Item;
		Object->TryGetStringField(ShuttlegentEditorWebField::Text, Item.Text);
		const TArray<TSharedPtr<FJsonValue>>* References = nullptr;
		if (Object->TryGetArrayField(ShuttlegentEditorWebField::References, References)) Item.References = FShuttlegentWebJson::ParseReferenceValues(*References);
		Object->TryGetStringField(ShuttlegentEditorDraft::FieldModel, Item.ModelValue);
		Object->TryGetStringField(ShuttlegentEditorDraft::FieldReasoning, Item.ReasoningValue);
		if (!Item.Text.TrimStartAndEnd().IsEmpty() || !Item.References.IsEmpty()) Items.Add(MoveTemp(Item));
	}
	return Items;
}

void FShuttlegentDraftStore::SetQueue(const FString& SessionId, const TArray<FShuttlegentQueuedPrompt>& Items)
{
	if (!IsValidKey(SessionId)) return;

	Find(SessionId);
	FEntry& Entry = Cache.FindChecked(SessionId);
	if (Items.IsEmpty() && Entry.Queue.IsEmpty()) return;

	Entry.Queue.Reset();
	for (const FShuttlegentQueuedPrompt& Item : Items)
	{
		Entry.Queue.Add(MakeShared<FJsonValueObject>(FShuttlegentDraftStorePrivate::MakeQueuedJson(Item)));
	}
	Write(SessionId);
}

void FShuttlegentDraftStore::Remove(const FString& Key)
{
	if (!IsValidKey(Key)) return;

	Cache.Add(Key, FEntry());
	Write(Key);
}

void FShuttlegentDraftStore::RemoveViewsExcept(const TSet<FString>& KeepViewKeys)
{
	TArray<FString> Files;
	IFileManager::Get().FindFiles(Files, *FPaths::Combine(Directory, ShuttlegentEditorDraft::ViewsDir, FString(TEXT("*")) + ShuttlegentEditorDraft::FileExtension), true, false);
	for (const FString& File : Files)
	{
		const FString ViewKey = FPaths::GetBaseFilename(File);
		if (ViewKey == ShuttlegentEditorDraft::HubKey || KeepViewKeys.Contains(ViewKey)) continue;
		Remove(MakeViewKey(ViewKey));
	}
}

const FShuttlegentDraftStore::FEntry& FShuttlegentDraftStore::Find(const FString& Key) const
{
	if (const FEntry* Cached = Cache.Find(Key)) return *Cached;

	FEntry& Entry = Cache.Add(Key);
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *GetPath(Key))) return Entry;

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Draft store: %s is not valid JSON; it is ignored."), *GetPath(Key));
		return Entry;
	}

	const TSharedPtr<FJsonObject>* Draft = nullptr;
	if (Root->TryGetObjectField(ShuttlegentEditorDraft::FieldDraft, Draft)) Entry.Draft = *Draft;
	const TArray<TSharedPtr<FJsonValue>>* Queue = nullptr;
	if (Root->TryGetArrayField(ShuttlegentEditorDraft::FieldQueue, Queue)) Entry.Queue = *Queue;
	return Entry;
}

void FShuttlegentDraftStore::Write(const FString& Key)
{
	const FEntry& Entry = Cache.FindChecked(Key);
	const FString Path = GetPath(Key);
	if (!Entry.Draft.IsValid() && Entry.Queue.IsEmpty())
	{
		IFileManager::Get().Delete(*Path, false, true, true);
		return;
	}

	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	if (Entry.Draft.IsValid()) Root->SetObjectField(ShuttlegentEditorDraft::FieldDraft, Entry.Draft);
	if (!Entry.Queue.IsEmpty()) Root->SetArrayField(ShuttlegentEditorDraft::FieldQueue, Entry.Queue);
	if (!FFileHelper::SaveStringToFile(FShuttlegentWebJson::ToJson(Root), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Draft store: could not write %s."), *Path);
	}
}

FString FShuttlegentDraftStore::GetPath(const FString& Key) const
{
	return FPaths::Combine(Directory, Key + ShuttlegentEditorDraft::FileExtension);
}
