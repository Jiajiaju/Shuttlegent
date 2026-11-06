#pragma once

#include "CoreMinimal.h"

#include "EShuttlegentConnectionStatus.generated.h"

/** Result of the latest test of a model connection. */
UENUM()
enum class EShuttlegentConnectionStatus : uint8
{
	// Not tested yet, or the address or key changed since the last test
	Untested,
	// Test in progress
	Testing,
	// Connected: the protocol and model list were detected
	Connected,
	// Not connected: see the Output Log for the reason
	Failed
};
