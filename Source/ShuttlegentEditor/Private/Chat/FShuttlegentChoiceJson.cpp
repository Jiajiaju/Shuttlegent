#include "Chat/FShuttlegentChoiceJson.h"

#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "ShuttlegentEditorConstants.h"

namespace FShuttlegentChoiceJsonPrivate
{
	TArray<TSharedPtr<FJsonValue>> MakeStrings(const TArray<FString>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Items;
		for (const FString& Value : Values)
		{
			Items.Add(MakeShared<FJsonValueString>(Value));
		}
		return Items;
	}

	/** selected、notes、overall 三个字段；只保留 Request 中存在的选项 id。 */
	void ReadAnswer(const FJsonObject& Object, const FShuttlegentChoiceRequest& Request, FShuttlegentChoiceAnswer& OutAnswer)
	{
		const auto HasOption = [&Request](const FString& Id) { return Request.Options.ContainsByPredicate([&Id](const FShuttlegentChoiceOption& Option) { return Option.Id == Id; }); };

		TArray<FString> Selected;
		Object.TryGetStringArrayField(ShuttlegentEditorWebField::ChoiceSelected, Selected);
		for (const FShuttlegentChoiceOption& Option : Request.Options)
		{
			if (Selected.Contains(Option.Id)) OutAnswer.Selected.Add(Option.Id);
		}

		const TSharedPtr<FJsonObject>* Notes = nullptr;
		if (Object.TryGetObjectField(ShuttlegentEditorWebField::ChoiceNotes, Notes) && Notes != nullptr)
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Notes)->Values)
			{
				FString Note;
				if (!HasOption(Pair.Key) || !Pair.Value.IsValid() || !Pair.Value->TryGetString(Note)) continue;

				Note.TrimStartAndEndInline();
				if (!Note.IsEmpty()) OutAnswer.ItemNotes.Add(Pair.Key, Note);
			}
		}

		Object.TryGetStringField(ShuttlegentEditorWebField::ChoiceOverall, OutAnswer.OverallNote);
		OutAnswer.OverallNote.TrimStartAndEndInline();
	}
}

TSharedRef<FJsonObject> FShuttlegentChoiceJson::ToJson(const FShuttlegentChoiceRequest& Request, const FShuttlegentChoiceAnswer& Answer, const FString& Status)
{
	const TSharedRef<FJsonObject> Object = MakeShared<FJsonObject>();
	Object->SetStringField(ShuttlegentEditorWebField::ChoiceKey, Request.ChoiceKey);
	Object->SetStringField(ShuttlegentEditorWebField::Question, Request.Question);
	Object->SetBoolField(ShuttlegentEditorWebField::ChoiceMultiSelect, Request.bMultiSelect);
	Object->SetBoolField(ShuttlegentEditorWebField::ChoiceItemNotes, Request.bItemNotes);
	Object->SetBoolField(ShuttlegentEditorWebField::ChoiceOverallNote, Request.bOverallNote);
	Object->SetStringField(ShuttlegentEditorWebField::Status, Status);

	TArray<TSharedPtr<FJsonValue>> Options;
	for (const FShuttlegentChoiceOption& Option : Request.Options)
	{
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(ShuttlegentEditorWebField::Id, Option.Id);
		Item->SetStringField(ShuttlegentEditorWebField::Title, Option.Title);
		Item->SetStringField(ShuttlegentEditorWebField::ChoiceOptionDescription, Option.Description);
		Item->SetStringField(ShuttlegentEditorWebField::ChoiceOptionImage, Option.Image);
		Options.Add(MakeShared<FJsonValueObject>(Item));
	}
	Object->SetArrayField(ShuttlegentEditorWebField::Options, Options);

	const TSharedRef<FJsonObject> Notes = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Answer.ItemNotes)
	{
		Notes->SetStringField(Pair.Key, Pair.Value);
	}
	Object->SetArrayField(ShuttlegentEditorWebField::ChoiceSelected, FShuttlegentChoiceJsonPrivate::MakeStrings(Answer.Selected));
	Object->SetObjectField(ShuttlegentEditorWebField::ChoiceNotes, Notes);
	Object->SetStringField(ShuttlegentEditorWebField::ChoiceOverall, Answer.OverallNote);
	return Object;
}

bool FShuttlegentChoiceJson::FromJson(const FJsonObject& Object, FShuttlegentChoiceRequest& OutRequest, FShuttlegentChoiceAnswer& OutAnswer, FString& OutStatus)
{
	Object.TryGetStringField(ShuttlegentEditorWebField::ChoiceKey, OutRequest.ChoiceKey);
	Object.TryGetStringField(ShuttlegentEditorWebField::Question, OutRequest.Question);
	Object.TryGetBoolField(ShuttlegentEditorWebField::ChoiceMultiSelect, OutRequest.bMultiSelect);
	Object.TryGetBoolField(ShuttlegentEditorWebField::ChoiceItemNotes, OutRequest.bItemNotes);
	Object.TryGetBoolField(ShuttlegentEditorWebField::ChoiceOverallNote, OutRequest.bOverallNote);
	Object.TryGetStringField(ShuttlegentEditorWebField::Status, OutStatus);

	const TArray<TSharedPtr<FJsonValue>>* Options = nullptr;
	if (Object.TryGetArrayField(ShuttlegentEditorWebField::Options, Options) && Options != nullptr)
	{
		for (const TSharedPtr<FJsonValue>& Value : *Options)
		{
			const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Item.IsValid()) continue;

			FShuttlegentChoiceOption Option;
			Item->TryGetStringField(ShuttlegentEditorWebField::Id, Option.Id);
			Item->TryGetStringField(ShuttlegentEditorWebField::Title, Option.Title);
			Item->TryGetStringField(ShuttlegentEditorWebField::ChoiceOptionDescription, Option.Description);
			Item->TryGetStringField(ShuttlegentEditorWebField::ChoiceOptionImage, Option.Image);
			OutRequest.Options.Add(MoveTemp(Option));
		}
	}
	if (OutRequest.ChoiceKey.IsEmpty() || OutRequest.Options.IsEmpty()) return false;

	FShuttlegentChoiceJsonPrivate::ReadAnswer(Object, OutRequest, OutAnswer);
	return true;
}

bool FShuttlegentChoiceJson::ParseAnswer(const FString& Json, const FShuttlegentChoiceRequest& Request, FShuttlegentChoiceAnswer& OutAnswer)
{
	TSharedPtr<FJsonObject> Object;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid()) return false;

	FShuttlegentChoiceJsonPrivate::ReadAnswer(*Object, Request, OutAnswer);
	if (!Request.bMultiSelect && OutAnswer.Selected.Num() > 1) OutAnswer.Selected.SetNum(1);
	if (!Request.bItemNotes) OutAnswer.ItemNotes.Reset();
	if (!Request.bOverallNote) OutAnswer.OverallNote.Reset();
	return true;
}
