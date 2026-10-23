#pragma once

#include "CoreMinimal.h"

#include "Chat/EShuttlegentChatEntryKind.h"

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

	/** Assistant entry: reasoning is expanded. */
	bool bThoughtExpanded = false;

	/** Permission entry: answered, and the answer. */
	bool bAnswered = false;
	bool bAllowed = false;

	/** Notice or turn end that reports a failure. */
	bool bError = false;

	/** Stable id used to answer a pending permission request. */
	int32 Id = 0;
};
