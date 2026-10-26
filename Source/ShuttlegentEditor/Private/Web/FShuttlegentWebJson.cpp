#include "Web/FShuttlegentWebJson.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "Reference/FShuttlegentReference.h"
#include "ShuttlegentEditorConstants.h"

TSharedPtr<FJsonValue> FShuttlegentWebJson::MakeReference(const FShuttlegentReference& Reference)
{
	const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
	Item->SetStringField(ShuttlegentEditorWebField::Kind, Reference.Kind == EShuttlegentReferenceKind::Actor ? ShuttlegentEditorWebField::KindActor : ShuttlegentEditorWebField::KindAsset);
	Item->SetStringField(ShuttlegentEditorWebField::Path, Reference.Path);
	Item->SetStringField(ShuttlegentEditorWebField::Name, Reference.Name);
	Item->SetStringField(ShuttlegentEditorWebField::Class, Reference.ClassName);
	return MakeShared<FJsonValueObject>(Item);
}

TArray<TSharedPtr<FJsonValue>> FShuttlegentWebJson::MakeReferences(const TArray<FShuttlegentReference>& References)
{
	TArray<TSharedPtr<FJsonValue>> Values;
	for (const FShuttlegentReference& Reference : References)
	{
		Values.Add(MakeReference(Reference));
	}
	return Values;
}

TArray<FShuttlegentReference> FShuttlegentWebJson::ParseReferences(const FString& Json)
{
	TArray<FShuttlegentReference> References;
	TArray<TSharedPtr<FJsonValue>> Values;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Values)) return References;

	for (const TSharedPtr<FJsonValue>& Value : Values)
	{
		const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
		if (!Item.IsValid()) continue;

		FShuttlegentReference Reference;
		Reference.Kind = Item->GetStringField(ShuttlegentEditorWebField::Kind) == ShuttlegentEditorWebField::KindActor ? EShuttlegentReferenceKind::Actor : EShuttlegentReferenceKind::Asset;
		Item->TryGetStringField(ShuttlegentEditorWebField::Path, Reference.Path);
		Item->TryGetStringField(ShuttlegentEditorWebField::Name, Reference.Name);
		Item->TryGetStringField(ShuttlegentEditorWebField::Class, Reference.ClassName);
		if (!Reference.Path.IsEmpty()) References.Add(MoveTemp(Reference));
	}
	return References;
}

FString FShuttlegentWebJson::ToJson(const TSharedRef<FJsonObject>& Object)
{
	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	FJsonSerializer::Serialize(Object, Writer);
	return Json;
}

FString FShuttlegentWebJson::ToJson(const TArray<TSharedPtr<FJsonValue>>& Values)
{
	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
	FJsonSerializer::Serialize(Values, Writer);
	return Json;
}
