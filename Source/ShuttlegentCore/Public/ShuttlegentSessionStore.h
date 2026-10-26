#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

#include "ShuttlegentSessionRecord.h"

/**
 * Session index and transcripts under <Project>/Saved/Shuttlegent/Sessions.
 *
 * The runtime cannot replay a session (no session/load), so every prompt, session/update
 * and turn end is appended here and replayed by the UI when a session is reopened.
 * Game thread only.
 */
class SHUTTLEGENTCORE_API FShuttlegentSessionStore
{
private:
	TArray<FShuttlegentSessionRecord> Records;
	FString Directory;
	bool bLoaded = false;

public:
	FShuttlegentSessionStore();

	/** All known sessions, most recently updated first. */
	const TArray<FShuttlegentSessionRecord>& GetRecords();

	const FShuttlegentSessionRecord* FindRecord(const FString& SessionId);

	/** Creates or updates a record and saves the index. A new record is stamped now and listed first; an existing one keeps its UpdatedAt and place. */
	void UpsertRecord(const FShuttlegentSessionRecord& Record);

	/** The session has new content (a prompt sent, a turn ended): UpdatedAt is set to now and the session is listed first. */
	void TouchRecord(const FString& SessionId);

	/** Removes a session from the index and deletes its transcript. Returns false for an unknown session. */
	bool DeleteRecord(const FString& SessionId);

	/** Sets a user-chosen title: trimmed, not empty, at most ShuttlegentSessionConstant::RenameMaxChars. Keeps UpdatedAt. Returns false when rejected. */
	bool RenameRecord(const FString& SessionId, const FString& Title);

	/** Archives or restores a session in the index. Keeps UpdatedAt. Returns false for an unknown session. */
	bool SetArchived(const FString& SessionId, bool bArchived);

	/** Appends one transcript entry: {"t": ISO-8601, "kind": Kind, "data": Data}. */
	void AppendTranscript(const FString& SessionId, const FString& Kind, const TSharedPtr<FJsonObject>& Data);

	/** Reads a whole transcript back as entry objects, oldest first. */
	TArray<TSharedPtr<FJsonObject>> LoadTranscript(const FString& SessionId) const;

private:
	void EnsureLoaded();
	void SaveIndex() const;
	FString GetTranscriptPath(const FString& SessionId) const;
	FString GetIndexPath() const;
};
