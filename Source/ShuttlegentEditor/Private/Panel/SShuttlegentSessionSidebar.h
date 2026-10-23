#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class FShuttlegentChatModel;
class SComboButton;
class SEditableTextBox;
class SVerticalBox;
struct FShuttlegentSessionRecord;

/**
 * Session sidebar (248 wide): brand row with a collapse button, "new session", search,
 * the session list grouped by today / yesterday / earlier, and a footer with the archived
 * sessions, the read-only permission preset and the settings button.
 */
class SShuttlegentSessionSidebar : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SShuttlegentSessionSidebar) {}
		SLATE_ARGUMENT(TSharedPtr<FShuttlegentChatModel>, Model)
		SLATE_EVENT(FSimpleDelegate, OnCollapse)
		SLATE_EVENT(FSimpleDelegate, OnSessionChosen)
	SLATE_END_ARGS()

private:
	TSharedPtr<FShuttlegentChatModel> Model;
	FSimpleDelegate OnCollapse;

	/** A session was opened or a new one started; an overlay sidebar closes. */
	FSimpleDelegate OnSessionChosen;

	TSharedPtr<SEditableTextBox> SearchBox;
	TSharedPtr<SVerticalBox> SessionList;
	TSharedPtr<SComboButton> ArchivedButton;
	FString SearchText;

public:
	void Construct(const FArguments& InArgs);
	virtual ~SShuttlegentSessionSidebar() override;

	void FocusSearch();

private:
	TSharedRef<SWidget> BuildFooter();
	TSharedRef<SWidget> BuildArchivedMenu();
	TSharedRef<SWidget> BuildSessionRow(const FShuttlegentSessionRecord& Record);
	void RebuildList();

	FText GetPermissionText() const;
	FText GetArchivedText() const;

	void HandleSessionsChanged();
};
