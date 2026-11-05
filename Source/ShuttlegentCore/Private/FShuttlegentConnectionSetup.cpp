#include "FShuttlegentConnectionSetup.h"

#include "FShuttlegentConnectionTester.h"
#include "FShuttlegentModelRoutes.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentSettings.h"

int32 FShuttlegentConnectionSetup::Connect(bool bBuiltin, const FString& BaseUrl, const FString& ApiKey)
{
	UShuttlegentSettings* Settings = GetMutableDefault<UShuttlegentSettings>();
	const FString Key = ApiKey.TrimStartAndEnd();
	const FString Url = FShuttlegentModelRoutes::TrimBaseUrl(BaseUrl);
	if (!IsValid(Settings) || Key.IsEmpty() || (!bBuiltin && Url.IsEmpty())) return INDEX_NONE;

	Settings->EnsureBuiltins();

	// 内置连接固定在最前；其余按地址（不分大小写）找已有的那条，同一服务不重复添加
	int32 Index = 0;
	if (!bBuiltin)
	{
		Index = Settings->Connections.IndexOfByPredicate([&Url](const FShuttlegentConnection& Connection)
		{
			return !Connection.bBuiltin && FShuttlegentModelRoutes::TrimBaseUrl(Connection.BaseUrl).Equals(Url, ESearchCase::IgnoreCase);
		});
		if (Index == INDEX_NONE)
		{
			FShuttlegentConnection Added;
			Added.BaseUrl = Url;
			Index = Settings->Connections.Add(Added);
		}
	}

	FShuttlegentConnection& Connection = Settings->Connections[Index];
	Connection.ApiKey = Key;
	if (Connection.Status == EShuttlegentConnectionStatus::Testing) Connection.Status = EShuttlegentConnectionStatus::Untested;
	Settings->SaveConfig();

	UE_LOG(LogShuttlegent, Log, TEXT("Connection set up from the panel: %s."), bBuiltin ? TEXT("built-in DeepSeek") : *Url);
	FShuttlegentConnectionTester::Test(Index);
	return Index;
}
