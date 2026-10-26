#include "Reference/FShuttlegentReferencePrompt.h"

#include "Reference/FShuttlegentReference.h"
#include "ShuttlegentEditorConstants.h"

FString FShuttlegentReferencePrompt::Compose(const FString& Text, const TArray<FShuttlegentReference>& References)
{
	if (References.IsEmpty()) return Text;

	FString Prompt = Text.TrimEnd() + ShuttlegentEditorReference::BlockSeparator + ShuttlegentEditorReference::BlockHeader;
	for (const FShuttlegentReference& Reference : References)
	{
		const TCHAR* Kind = Reference.Kind == EShuttlegentReferenceKind::Actor ? ShuttlegentEditorReference::KindLabelActor : ShuttlegentEditorReference::KindLabelAsset;
		Prompt += ShuttlegentEditorReference::LineBreak;
		Prompt += FString::Format(ShuttlegentEditorReference::LineFormat, { Kind, Reference.Name, Reference.ClassName, Reference.Path });
	}
	return Prompt;
}

void FShuttlegentReferencePrompt::Split(const FString& Prompt, FString& OutText, TArray<FString>& OutReferenceLines)
{
	OutReferenceLines.Reset();

	const FString Marker = FString(ShuttlegentEditorReference::BlockSeparator) + ShuttlegentEditorReference::BlockHeader;
	const int32 MarkerIndex = Prompt.Find(Marker, ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (MarkerIndex == INDEX_NONE)
	{
		OutText = Prompt;
		return;
	}

	OutText = Prompt.Left(MarkerIndex);
	Prompt.Mid(MarkerIndex + Marker.Len()).ParseIntoArray(OutReferenceLines, ShuttlegentEditorReference::LineBreak, true);
}
