#include "Endpoint/FShuttlegentToolCallScope.h"

FString FShuttlegentToolCallScope::CurrentKey;

FShuttlegentToolCallScope::FShuttlegentToolCallScope(const FString& ConversationKey)
	: PreviousKey(CurrentKey)
{
	check(IsInGameThread());
	CurrentKey = ConversationKey;
}

FShuttlegentToolCallScope::~FShuttlegentToolCallScope()
{
	CurrentKey = PreviousKey;
}
