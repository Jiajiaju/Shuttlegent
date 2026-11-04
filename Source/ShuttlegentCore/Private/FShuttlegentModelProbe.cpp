#include "FShuttlegentModelProbe.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "FShuttlegentModelRoutes.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"

namespace FShuttlegentModelProbePrivate
{
	/** 一次 HTTP 往返的结果。 */
	struct FResponse
	{
		bool bResponded = false;
		bool bTimedOut = false;
		int32 StatusCode = 0;
		FString Body;
	};

	using FOnResponse = TFunction<void(const FResponse&)>;

	bool IsOk(const FResponse& Response)
	{
		return Response.bResponded && Response.StatusCode >= ShuttlegentProbeConstant::StatusOkMin && Response.StatusCode <= ShuttlegentProbeConstant::StatusOkMax;
	}

	/** 限流也说明地址、协议、密钥与模型都对。 */
	bool IsReachable(const FResponse& Response)
	{
		return IsOk(Response) || (Response.bResponded && Response.StatusCode == ShuttlegentProbeConstant::StatusTooManyRequests);
	}

	bool IsKeyRejected(const FResponse& Response)
	{
		return Response.bResponded && (Response.StatusCode == ShuttlegentProbeConstant::StatusUnauthorized || Response.StatusCode == ShuttlegentProbeConstant::StatusForbidden);
	}

	/** 写进日志的正文：去掉换行，截断。 */
	FString Excerpt(const FString& Body)
	{
		return Body.Left(ShuttlegentProbeConstant::DetailMaxChars).Replace(TEXT("\r"), TEXT(" ")).Replace(TEXT("\n"), TEXT(" "));
	}

	/** 发一个请求，记一行日志；Body 为空时不带正文。 */
	void Send(const TCHAR* Verb, const FString& Url, const FString& ApiKey, bool bAnthropicHeaders, const FString& Body, FOnResponse OnResponse)
	{
		const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(Url);
		Request->SetVerb(Verb);
		Request->SetTimeout(ShuttlegentProbeConstant::TimeoutSeconds);
		if (bAnthropicHeaders)
		{
			Request->SetHeader(ShuttlegentProbeConstant::HeaderAnthropicKey, ApiKey);
			Request->SetHeader(ShuttlegentProbeConstant::HeaderAnthropicVersion, ShuttlegentProbeConstant::AnthropicVersion);
		}
		else
		{
			Request->SetHeader(ShuttlegentProbeConstant::HeaderAuthorization, FString(ShuttlegentProbeConstant::BearerPrefix) + ApiKey);
		}
		if (!Body.IsEmpty())
		{
			Request->SetHeader(ShuttlegentProbeConstant::HeaderContentType, ShuttlegentProbeConstant::ContentTypeJson);
			Request->SetContentAsString(Body);
		}

		const double StartSeconds = FPlatformTime::Seconds();
		const FString VerbText = Verb;
		Request->OnProcessRequestComplete().BindLambda([OnResponse, StartSeconds, VerbText, Url](FHttpRequestPtr Completed, FHttpResponsePtr HttpResponse, bool bConnected)
		{
			const int32 ElapsedMs = FMath::RoundToInt((FPlatformTime::Seconds() - StartSeconds) * ShuttlegentProbeConstant::MillisecondsPerSecond);
			FResponse Response;
			if (!bConnected || !HttpResponse.IsValid())
			{
				Response.bTimedOut = Completed.IsValid() && Completed->GetFailureReason() == EHttpFailureReason::TimedOut;
				UE_LOG(LogShuttlegent, Log, TEXT("Connection test: %s %s -> no response (%s) in %d ms."), *VerbText, *Url, Response.bTimedOut ? TEXT("timed out") : TEXT("connection failed"), ElapsedMs);
				OnResponse(Response);
				return;
			}

			Response.bResponded = true;
			Response.StatusCode = HttpResponse->GetResponseCode();
			Response.Body = HttpResponse->GetContentAsString();
			if (IsOk(Response))
			{
				UE_LOG(LogShuttlegent, Log, TEXT("Connection test: %s %s -> %d in %d ms."), *VerbText, *Url, Response.StatusCode, ElapsedMs);
			}
			else
			{
				UE_LOG(LogShuttlegent, Log, TEXT("Connection test: %s %s -> %d in %d ms: %s"), *VerbText, *Url, Response.StatusCode, ElapsedMs, *Excerpt(Response.Body));
			}
			OnResponse(Response);
		});
		Request->ProcessRequest();
	}

	FString ToJson(const TSharedRef<FJsonObject>& Object)
	{
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Object, Writer);
		return Json;
	}

	/** 试发的对话请求体：一条 ping，输出上限取 TokenField 字段；TokenField 为空时不限。 */
	FString MakeChatBody(EShuttlegentProviderApi Api, const FString& ModelId, const TCHAR* TokenField, int32 Tokens)
	{
		const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(ShuttlegentProbeConstant::FieldModel, ModelId);
		if (TokenField != nullptr) Body->SetNumberField(TokenField, Tokens);
		if (Api == EShuttlegentProviderApi::OpenAiResponses)
		{
			Body->SetStringField(ShuttlegentProbeConstant::FieldInput, ShuttlegentProbeConstant::PingText);
			return ToJson(Body);
		}

		const TSharedRef<FJsonObject> Message = MakeShared<FJsonObject>();
		Message->SetStringField(ShuttlegentProbeConstant::FieldRole, ShuttlegentProbeConstant::RoleUser);
		Message->SetStringField(ShuttlegentProbeConstant::FieldContent, ShuttlegentProbeConstant::PingText);
		Body->SetArrayField(ShuttlegentProbeConstant::FieldMessages, { MakeShared<FJsonValueObject>(Message) });
		return ToJson(Body);
	}

	bool IsChatModel(const FString& ModelId)
	{
		for (const TCHAR* Keyword : ShuttlegentProbeConstant::NonChatModelKeywords)
		{
			if (ModelId.Contains(Keyword, ESearchCase::IgnoreCase)) return false;
		}
		return true;
	}

	/** 模型列表响应 { data: [{ id, created | created_at }] } 中可用于对话的模型，最新的在前；不是这种形状时返回 false。 */
	bool ParseModelList(const FString& Body, TArray<FString>& OutModelIds)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		const TArray<TSharedPtr<FJsonValue>>* Data = nullptr;
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid() || !Root->TryGetArrayField(ShuttlegentProbeConstant::FieldData, Data)) return false;

		TArray<TPair<int64, FString>> Models;
		for (const TSharedPtr<FJsonValue>& Value : *Data)
		{
			const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
			FString Id;
			if (!Item.IsValid() || !Item->TryGetStringField(ShuttlegentProbeConstant::FieldId, Id) || Id.IsEmpty() || !IsChatModel(Id)) continue;

			int64 Created = 0;
			FString CreatedAt;
			FDateTime At;
			if (!Item->TryGetNumberField(ShuttlegentProbeConstant::FieldCreated, Created) && Item->TryGetStringField(ShuttlegentProbeConstant::FieldCreatedAt, CreatedAt) && FDateTime::ParseIso8601(*CreatedAt, At))
			{
				Created = At.ToUnixTimestamp();
			}
			Models.Emplace(Created, Id);
		}

		Models.Sort([](const TPair<int64, FString>& Left, const TPair<int64, FString>& Right) { return Left.Key != Right.Key ? Left.Key > Right.Key : Left.Value < Right.Value; });
		OutModelIds.Reset();
		for (const TPair<int64, FString>& Model : Models)
		{
			OutModelIds.AddUnique(Model.Value);
		}
		return true;
	}

	/** 一次检测的状态，在各请求的回调之间传递。 */
	struct FDetection : TSharedFromThis<FDetection>
	{
		FShuttlegentModelProbeTarget Target;
		FString BaseUrl;
		FShuttlegentModelProbe::FOnProbeDone OnDone;
		FShuttlegentModelProbeResult Result;
		double StartSeconds = 0.0;

		void Finish(bool bConnected, const FString& Reason)
		{
			Result.bConnected = bConnected;
			const int32 ElapsedMs = FMath::RoundToInt((FPlatformTime::Seconds() - StartSeconds) * ShuttlegentProbeConstant::MillisecondsPerSecond);
			if (bConnected)
			{
				UE_LOG(LogShuttlegent, Log, TEXT("Connection test %s: connected, %s, %d model(s), first %s, in %d ms."), *BaseUrl, FShuttlegentModelRoutes::GetApiName(Result.Api), Result.ModelIds.Num(), Result.ModelIds.IsEmpty() ? TEXT("-") : *Result.ModelIds[0], ElapsedMs);
			}
			else
			{
				UE_LOG(LogShuttlegent, Warning, TEXT("Connection test %s: failed, %s"), *BaseUrl, *Reason);
			}
			OnDone.ExecuteIfBound(Result);
		}

		void Run()
		{
			StartSeconds = FPlatformTime::Seconds();
			if (BaseUrl.IsEmpty()) { Finish(false, TEXT("no base URL.")); return; }
			if (Target.ApiKey.IsEmpty()) { Finish(false, TEXT("no API key.")); return; }

			if (Target.bBuiltin) { TestBuiltin(); return; }
			ListOpenAiModels();
		}

		/** 内置 DeepSeek：协议与模型固定，只用第一个模型试发一次。 */
		void TestBuiltin()
		{
			Result.Api = EShuttlegentProviderApi::AnthropicMessages;
			for (const TCHAR* ModelId : ShuttlegentBuiltinModel::Ids)
			{
				Result.ModelIds.Add(ModelId);
			}

			const TSharedRef<FDetection> Self = AsShared();
			const FString Url = FShuttlegentModelProbe::GetChatUrl(BaseUrl, EShuttlegentProviderApi::AnthropicMessages, true);
			Send(ShuttlegentProbeConstant::Verb, Url, Target.ApiKey, true, MakeChatBody(EShuttlegentProviderApi::AnthropicMessages, Result.ModelIds[0], ShuttlegentProbeConstant::FieldMaxTokens, ShuttlegentProbeConstant::MinOutputTokens), [Self](const FResponse& Response)
			{
				if (IsReachable(Response)) Self->Finish(true, FString());
				else Self->Finish(false, Self->Describe(Response));
			});
		}

		void ListOpenAiModels()
		{
			const TSharedRef<FDetection> Self = AsShared();
			Send(ShuttlegentProbeConstant::VerbGet, BaseUrl + ShuttlegentProbeConstant::OpenAiModelsPath, Target.ApiKey, false, FString(), [Self](const FResponse& Response)
			{
				if (!Response.bResponded) { Self->Finish(false, Self->Describe(Response)); return; }
				if (IsKeyRejected(Response)) { Self->Finish(false, Self->Describe(Response)); return; }
				if (IsOk(Response) && ParseModelList(Response.Body, Self->Result.ModelIds))
				{
					if (Self->Result.ModelIds.IsEmpty()) { Self->Finish(false, TEXT("the model list has no chat models.")); return; }
					Self->TryResponses();
					return;
				}
				Self->ListAnthropicModels();
			});
		}

		void ListAnthropicModels()
		{
			const TSharedRef<FDetection> Self = AsShared();
			Send(ShuttlegentProbeConstant::VerbGet, BaseUrl + ShuttlegentProbeConstant::AnthropicModelsPath, Target.ApiKey, true, FString(), [Self](const FResponse& Response)
			{
				if (IsOk(Response) && ParseModelList(Response.Body, Self->Result.ModelIds))
				{
					if (Self->Result.ModelIds.IsEmpty()) { Self->Finish(false, TEXT("the model list has no chat models.")); return; }
					Self->Result.Api = EShuttlegentProviderApi::AnthropicMessages;
					Self->Finish(true, FString());
					return;
				}
				Self->Finish(false, FString::Printf(TEXT("neither %s (OpenAI) nor %s (Anthropic) returned a model list; %s"), ShuttlegentProbeConstant::OpenAiModelsPath, ShuttlegentProbeConstant::AnthropicModelsPath, *Self->Describe(Response)));
			});
		}

		void TryResponses()
		{
			const TSharedRef<FDetection> Self = AsShared();
			const FString Body = MakeChatBody(EShuttlegentProviderApi::OpenAiResponses, Result.ModelIds[0], ShuttlegentProbeConstant::FieldMaxOutputTokens, ShuttlegentProbeConstant::DetectOutputTokens);
			Send(ShuttlegentProbeConstant::Verb, FShuttlegentModelProbe::GetChatUrl(BaseUrl, EShuttlegentProviderApi::OpenAiResponses, false), Target.ApiKey, false, Body, [Self](const FResponse& Response)
			{
				if (IsReachable(Response))
				{
					Self->Result.Api = EShuttlegentProviderApi::OpenAiResponses;
					Self->Finish(true, FString());
					return;
				}
				Self->TryCompletions(ShuttlegentProbeConstant::FieldMaxTokens);
			});
		}

		/** Chat Completions：先用 max_tokens，服务方要求 max_completion_tokens 时换它再试一次。 */
		void TryCompletions(const TCHAR* TokenField)
		{
			const TSharedRef<FDetection> Self = AsShared();
			const FString Body = MakeChatBody(EShuttlegentProviderApi::OpenAiCompletions, Result.ModelIds[0], TokenField, ShuttlegentProbeConstant::DetectOutputTokens);
			Send(ShuttlegentProbeConstant::Verb, FShuttlegentModelProbe::GetChatUrl(BaseUrl, EShuttlegentProviderApi::OpenAiCompletions, false), Target.ApiKey, false, Body, [Self, TokenField](const FResponse& Response)
			{
				if (IsReachable(Response))
				{
					Self->Result.Api = EShuttlegentProviderApi::OpenAiCompletions;
					Self->Finish(true, FString());
					return;
				}
				const bool bWantsNewField = FCString::Strcmp(TokenField, ShuttlegentProbeConstant::FieldMaxCompletionTokens) != 0 && Response.Body.Contains(ShuttlegentProbeConstant::FieldMaxCompletionTokens);
				if (bWantsNewField) { Self->TryCompletions(ShuttlegentProbeConstant::FieldMaxCompletionTokens); return; }
				Self->Finish(false, FString::Printf(TEXT("the model list works, but neither /responses nor /chat/completions accepted %s; %s"), *Self->Result.ModelIds[0], *Self->Describe(Response)));
			});
		}

		FString Describe(const FResponse& Response) const
		{
			if (!Response.bResponded) return Response.bTimedOut ? TEXT("request timed out.") : TEXT("host unreachable (DNS or connection refused).");
			if (IsKeyRejected(Response)) return FString::Printf(TEXT("API key rejected (HTTP %d)."), Response.StatusCode);
			return FString::Printf(TEXT("HTTP %d: %s"), Response.StatusCode, *Excerpt(Response.Body));
		}
	};
}

void FShuttlegentModelProbe::Detect(const FShuttlegentModelProbeTarget& Target, FOnProbeDone OnDone)
{
	const TSharedRef<FShuttlegentModelProbePrivate::FDetection> Detection = MakeShared<FShuttlegentModelProbePrivate::FDetection>();
	Detection->Target = Target;
	Detection->Target.ApiKey = Target.ApiKey.TrimStartAndEnd();
	Detection->BaseUrl = FShuttlegentModelRoutes::TrimBaseUrl(Target.BaseUrl);
	Detection->OnDone = OnDone;
	Detection->Run();
}

FString FShuttlegentModelProbe::GetChatUrl(const FString& BaseUrl, EShuttlegentProviderApi Api, bool bBuiltin)
{
	const FString Base = FShuttlegentModelRoutes::TrimBaseUrl(BaseUrl);
	if (Base.IsEmpty()) return FString();

	// 内置 DeepSeek 由 llm-deepseek 发请求：地址已以 /v1 结尾时不再补（messages-api.ts 的 messagesApiRoot）
	if (bBuiltin)
	{
		const FString Root = Base.EndsWith(ShuttlegentProbeConstant::VersionSegment) ? Base : Base + ShuttlegentProbeConstant::VersionSegment;
		return Root + ShuttlegentProbeConstant::BuiltinMessagesPath;
	}

	switch (Api)
	{
	case EShuttlegentProviderApi::AnthropicMessages:
		{
			return Base + ShuttlegentProbeConstant::AnthropicMessagesPath;
		}
	case EShuttlegentProviderApi::OpenAiResponses:
		{
			return Base + ShuttlegentProbeConstant::OpenAiResponsesPath;
		}
	case EShuttlegentProviderApi::OpenAiCompletions:
	default:
		{
			return Base + ShuttlegentProbeConstant::OpenAiCompletionsPath;
		}
	}
}
