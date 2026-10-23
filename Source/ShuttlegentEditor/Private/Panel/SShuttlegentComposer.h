#pragma once

#include "CoreMinimal.h"
#include "Types/SlateStructs.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/SCompoundWidget.h"

#include "Chat/FShuttlegentConfigChoice.h"

class FShuttlegentChatModel;
class SEditableTextBox;
class SShuttlegentElevationCard;

/**
 * Composer at the bottom of the conversation column: bordered single-line input, toolbar with
 * model, reasoning effort, usage and send / stop, and the hint line. While a runtime permission
 * request waits, the escalation card takes its place.
 */
class SShuttlegentComposer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShuttlegentComposer) {}
		SLATE_ARGUMENT(TSharedPtr<FShuttlegentChatModel>, Model)
		SLATE_EVENT(FSimpleDelegate, OnSubmitted)
	SLATE_END_ARGS()

private:
	using FChoicePtr = TSharedPtr<FShuttlegentConfigChoice>;

	TSharedPtr<FShuttlegentChatModel> Model;
	FSimpleDelegate OnSubmitted;
	TSharedPtr<SEditableTextBox> InputBox;
	TSharedPtr<SShuttlegentElevationCard> ElevationCard;
	TSharedPtr<SComboBox<FChoicePtr>> ModelCombo;
	TSharedPtr<SComboBox<FChoicePtr>> ReasoningCombo;
	TArray<FChoicePtr> ModelChoices;
	TArray<FChoicePtr> ReasoningChoices;
	FString CurrentModelValue;
	FString CurrentReasoningValue;

	/** Permission entry the card shows; 0 when the composer is shown. */
	int32 ShownPermissionId = 0;

	/** Width of the composer area at the last tick; sizes the column. */
	float ViewWidth = 0.0f;

	bool bHasDraft = false;

public:
	void Construct(const FArguments& InArgs);
	virtual ~SShuttlegentComposer() override;

	void FocusInput();

	//~ Begin SWidget Interface.
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget Interface.

private:
	TSharedRef<SWidget> BuildInput();
	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildConfigCombo(TSharedPtr<SComboBox<FChoicePtr>>& OutCombo, TArray<FChoicePtr>& Choices, const FString& ConfigId, FString* CurrentValue);

	void RefreshConfigChoices();
	void SubmitInput();
	void OnInputCommitted(const FText& Text, ETextCommit::Type CommitType);

	FOptionalSize GetColumnWidth() const;
	FText GetUsageText() const;
	FText GetUsageToolTip() const;
	bool IsPrompting() const;

	void HandleStatusChanged();
};
