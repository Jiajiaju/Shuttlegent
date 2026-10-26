#pragma once

#include "CoreMinimal.h"

#include "Chat/EShuttlegentChatEntryKind.h"
#include "WriteGate/FShuttlegentWriteResult.h"

/** One row of the chat panel. Widgets read it through attributes, so streamed text needs no rebuild. */
struct FShuttlegentChatEntry
{
	EShuttlegentChatEntryKind Kind = EShuttlegentChatEntryKind::Notice;

	/** Message, prompt, notice or error text. */
	FString Text;

	/** Reasoning text of an assistant message; shown folded. */
	FString Thought;

	/** ACP messageId of an assistant entry. */
	FString MessageId;

	/** ACP toolCallId of a tool call or permission entry. */
	FString ToolCallId;

	/** Tool title, or the tool a permission request is about. */
	FString Title;

	/** Tool call status: pending, in_progress, completed or failed. */
	FString Status;

	/** Extra detail: tool justification for permission requests. */
	FString Detail;

	/** Tool call: rawInput as compact JSON. */
	FString Input;

	/** Tool call: text of the latest result content, including the error text of a failed call. */
	FString Result;

	/** Assistant entry: times of the first and the latest reasoning chunk (UTC); unset without reasoning. */
	FDateTime ThoughtStartUtc {};
	FDateTime ThoughtEndUtc {};

	/** Assistant entry: reasoning is expanded. */
	bool bThoughtExpanded = false;

	/** Permission entry: answered, and the answer. Write unknown entry: bAnswered once the user verified the objects. */
	bool bAnswered = false;
	bool bAllowed = false;

	/** Notice or turn end that reports a failure. */
	bool bError = false;

	/** Write result entry: what the write did, and whether "undo this change" already reverted it. */
	FShuttlegentWriteResult WriteResult;
	bool bUndone = false;

	/** Stable id used to answer a pending permission request. */
	int32 Id = 0;
};
