#include "FShuttlegentConnectionTester.h"

#include "FShuttlegentModelProbe.h"
#include "FShuttlegentModelRoutes.h"
#include "ShuttlegentSettings.h"

void FShuttlegentConnectionTester::Test(int32 Index)
{
	UShuttlegentSettings* Settings = GetMutableDefault<UShuttlegentSettings>();
	if (!IsValid(Settings) || !Settings->Connections.IsValidIndex(Index)) return;

	FShuttlegentConnection& Connection = Settings->Connections[Index];
	if (Connection.Status == EShuttlegentConnectionStatus::Testing) return;

	FShuttlegentModelProbeTarget Target;
	Target.BaseUrl = Connection.BaseUrl;
	Target.ApiKey = Connection.ApiKey;
	Target.bBuiltin = Connection.bBuiltin;
	const FString Fingerprint = FShuttlegentModelRoutes::MakeFingerprint(Connection);
	Connection.Status = EShuttlegentConnectionStatus::Testing;
	OnTested().Broadcast(Index);

	FShuttlegentModelProbe::Detect(Target, FShuttlegentModelProbe::FOnProbeDone::CreateLambda([Index, Fingerprint](const FShuttlegentModelProbeResult& Result)
	{
		UShuttlegentSettings* Current = GetMutableDefault<UShuttlegentSettings>();
		if (!IsValid(Current)) return;

		// 检测期间条目被删、挪动或又改了地址与密钥：结果不属于它，按指纹找回原条目，找不到就丢弃
		int32 TestedIndex = Index;
		if (!Current->Connections.IsValidIndex(TestedIndex) || FShuttlegentModelRoutes::MakeFingerprint(Current->Connections[TestedIndex]) != Fingerprint)
		{
			TestedIndex = Current->Connections.IndexOfByPredicate([&Fingerprint](const FShuttlegentConnection& Item) { return FShuttlegentModelRoutes::MakeFingerprint(Item) == Fingerprint; });
		}
		if (TestedIndex == INDEX_NONE) return;

		FShuttlegentConnection& Tested = Current->Connections[TestedIndex];
		Tested.Status = Result.bConnected ? EShuttlegentConnectionStatus::Connected : EShuttlegentConnectionStatus::Failed;
		Tested.TestedFingerprint = Fingerprint;
		if (Result.bConnected && !Tested.bBuiltin)
		{
			Tested.Api = Result.Api;
			Tested.ModelIds = Result.ModelIds;
		}
		Current->SaveConfig();
		OnTested().Broadcast(TestedIndex);
	}));
}

FShuttlegentConnectionTester::FOnTested& FShuttlegentConnectionTester::OnTested()
{
	static FOnTested Delegate;
	return Delegate;
}
