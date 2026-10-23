#pragma once

#include "CoreMinimal.h"

/** Kind of one row in the chat panel. */
enum class EShuttlegentChatEntryKind : uint8
{
	// A prompt the user sent
	User,

	// Assistant reply text (and its folded reasoning) of one message id
	Assistant,

	// One tool call and its latest status
	ToolCall,

	// A runtime permission request, answered from the panel
	Permission,

	// A turn that did not end normally (cancelled or failed)
	TurnEnd,

	// Connection and session notices
	Notice
};
