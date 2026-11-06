#include "FShuttlegentConnectionTester.h"

#include "FShuttlegentEffortCatalog.h"
#include "FShuttlegentModelProbe.h"
#include "FShuttlegentModelRoutes.h"
#include "ShuttlegentSettings.h"

namespace FShuttlegentConnectionTesterPrivate
{
	/**
	 * 把检测结论写回设置并存盘。检测期间条目被删、挪动或又改了地址与密钥时，按指纹找回原条目，找不到就丢弃。
	 * 目录取不到（bEffortsAvailable 为 false）时保留该连接原有的档位，只去掉已不在模型列表中的模型。
	 */
	void Apply(int32 Index, const FString& Fingerprint, const FShuttlegentModelProbeResult& Result, bool bEffortsAvailable, const TArray<FShuttlegentModelEfforts>& Efforts)
	{
		UShuttlegentSettings* Current = GetMutableDefault<UShuttlegentSettings>();
		if (!IsValid(Current)) return;

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
			if (bEffortsAvailable) Tested.ModelEfforts = Efforts;
			else Tested.ModelEfforts.RemoveAll([&Result](const FShuttlegentModelEfforts& Item) { return !Result.ModelIds.Contains(Item.ModelId); });
		}

		Current->SaveConfig();
		FShuttlegentConnectionTester::OnTested().Broadcast(TestedIndex);
	}
}

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

	FShuttlegentModelProbe::Detect(Target, FShuttlegentModelProbe::FOnProbeDone::CreateLambda([Index, Fingerprint, BaseUrl = Target.BaseUrl, bBuiltin = Target.bBuiltin](const FShuttlegentModelProbeResult& Result)
	{
		// 内置连接的档位由运行时自带；其余连通的连接再查模型目录，查完一并写回，运行时只因这次检测重启一次
		if (!Result.bConnected || bBuiltin)
		{
			FShuttlegentConnectionTesterPrivate::Apply(Index, Fingerprint, Result, false, TArray<FShuttlegentModelEfforts>());
			return;
		}

		FShuttlegentEffortCatalog::Resolve(BaseUrl,Result.ModelIds, FShuttlegentEffortCatalog::FOnResolved::CreateLambda([Index, Fingerprint, Result](bool bAvailable, const TArray<FShuttlegentModelEfforts>& Efforts)
		{
			FShuttlegentConnectionTesterPrivate::Apply(Index, Fingerprint, Result, bAvailable, Efforts);
		}));
	}));
}

FShuttlegentConnectionTester::FOnTested& FShuttlegentConnectionTester::OnTested()
{
	static FOnTested Delegate;
	return Delegate;
}
