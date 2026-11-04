#include "FShuttlegentModelProbe.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "FShuttlegentModelRoutes.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentSettings.h"

namespace FShuttlegentModelProbePrivate
{
	/** 地址去掉首尾空白与末尾斜杠，与 SDK 拼接路径前的处理一致。 */
	FString TrimBaseUrl(const FString& BaseUrl)
	{
		FString Base = BaseUrl.TrimStartAndEnd();
		while (Base.EndsWith(ShuttlegentProbeConstant::UrlSlash))
		{
			Base.LeftChopInline(FCString::Strlen(ShuttlegentProbeConstant::UrlSlash));
		}
		return Base;
	}

	/** 最小请求体：一条用户消息，输出上限取接口允许的最小值。 */
	FString MakeBody(const FShuttlegentModelProbeTarget& Target)
	{
		const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(ShuttlegentProbeConstant::FieldModel, Target.ModelId);

		if (!Target.bBuiltinDeepSeek && Target.Api == EShuttlegentProviderApi::OpenAiResponses)
		{
			Body->SetNumberField(ShuttlegentProbeConstant::FieldMaxOutputTokens, ShuttlegentProbeConstant::MinResponsesOutputTokens);
			Body->SetStringField(ShuttlegentProbeConstant::FieldInput, ShuttlegentProbeConstant::PingText);
		}
		else
		{
			const TSharedRef<FJsonObject> Message = MakeShared<FJsonObject>();
			Message->SetStringField(ShuttlegentProbeConstant::FieldRole, ShuttlegentProbeConstant::RoleUser);
			Message->SetStringField(ShuttlegentProbeConstant::FieldContent, ShuttlegentProbeConstant::PingText);
			Body->SetNumberField(ShuttlegentProbeConstant::FieldMaxTokens, ShuttlegentProbeConstant::MinOutputTokens);
			Body->SetArrayField(ShuttlegentProbeConstant::FieldMessages, { MakeShared<FJsonValueObject>(Message) });
		}

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Body, Writer);
		return Json;
	}

	bool UsesAnthropicHeaders(const FShuttlegentModelProbeTarget& Target)
	{
		return Target.bBuiltinDeepSeek || Target.Api == EShuttlegentProviderApi::AnthropicMessages;
	}

	/** 由状态码与响应分类。 */
	EShuttlegentModelProbeOutcome Classify(int32 StatusCode, const FString& ContentType, const FString& Body)
	{
		if (StatusCode >= ShuttlegentProbeConstant::StatusOkMin && StatusCode <= ShuttlegentProbeConstant::StatusOkMax) return EShuttlegentModelProbeOutcome::Connected;
		if (StatusCode == ShuttlegentProbeConstant::StatusTooManyRequests) return EShuttlegentModelProbeOutcome::RateLimited;
		if (StatusCode == ShuttlegentProbeConstant::StatusUnauthorized || StatusCode == ShuttlegentProbeConstant::StatusForbidden) return EShuttlegentModelProbeOutcome::KeyInvalid;

		const bool bHtml = ContentType.Contains(ShuttlegentProbeConstant::HtmlContentType) || Body.TrimStart().StartsWith(ShuttlegentProbeConstant::HtmlLeadingChar);
		if (StatusCode == ShuttlegentProbeConstant::StatusNotFound || bHtml) return EShuttlegentModelProbeOutcome::ProtocolMismatch;
		if (StatusCode == ShuttlegentProbeConstant::StatusBadRequest && Body.Contains(ShuttlegentProbeConstant::ModelKeyword, ESearchCase::IgnoreCase)) return EShuttlegentModelProbeOutcome::WrongModel;
		return EShuttlegentModelProbeOutcome::HttpError;
	}

	/** 没有发请求就能确定的结论：缺少地址、模型 ID 或密钥。 */
	bool FindMissingField(const FShuttlegentModelProbeTarget& Target, EShuttlegentModelProbeOutcome& OutOutcome)
	{
		if (TrimBaseUrl(Target.BaseUrl).IsEmpty())
		{
			OutOutcome = EShuttlegentModelProbeOutcome::MissingBaseUrl;
			return true;
		}
		if (Target.ModelId.TrimStartAndEnd().IsEmpty())
		{
			OutOutcome = EShuttlegentModelProbeOutcome::MissingModel;
			return true;
		}
		if (Target.ApiKey.IsEmpty())
		{
			OutOutcome = EShuttlegentModelProbeOutcome::MissingKey;
			return true;
		}
		return false;
	}
}

FShuttlegentModelProbeTarget FShuttlegentModelProbe::MakeTarget(const UShuttlegentSettings& Settings, int32 ModelIndex)
{
	FShuttlegentModelProbeTarget Target;
	if (!Settings.Models.IsValidIndex(ModelIndex)) return Target;

	// 与 FShuttlegentModelRoutes 对该模型的取值一致：运行时只收到个人设置中的密钥，留空时沿用同地址同协议的模型的密钥
	const FShuttlegentModelEntry& Entry = Settings.Models[ModelIndex];
	const FString BaseUrl = Entry.BaseUrl.TrimStartAndEnd();
	int32 KeySource = INDEX_NONE;
	Target.Label = Entry.Name;
	Target.ModelId = Entry.ModelId.TrimStartAndEnd();
	Target.BaseUrl = Entry.bBuiltin && BaseUrl.IsEmpty() ? FString(ShuttlegentProviderConstant::DeepSeekBaseUrl) : BaseUrl;
	Target.Api = Entry.Api;
	Target.bBuiltinDeepSeek = Entry.bBuiltin;
	Target.ApiKey = FShuttlegentModelRoutes::ResolveKey(Settings, ModelIndex, KeySource);
	return Target;
}

FString FShuttlegentModelProbe::GetRequestUrl(const FShuttlegentModelProbeTarget& Target)
{
	const FString Base = FShuttlegentModelProbePrivate::TrimBaseUrl(Target.BaseUrl);
	if (Base.IsEmpty()) return FString();

	if (Target.bBuiltinDeepSeek)
	{
		const FString Root = Base.EndsWith(ShuttlegentProbeConstant::VersionSegment) ? Base : Base + ShuttlegentProbeConstant::VersionSegment;
		return Root + ShuttlegentProbeConstant::BuiltinMessagesPath;
	}

	switch (Target.Api)
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

void FShuttlegentModelProbe::Probe(const FShuttlegentModelProbeTarget& Target, FOnProbeDone OnDone)
{
	FShuttlegentModelProbeResult Result;
	Result.Label = Target.Label;
	Result.ModelId = Target.ModelId.TrimStartAndEnd();

	EShuttlegentModelProbeOutcome Missing = EShuttlegentModelProbeOutcome::Unreachable;
	if (FShuttlegentModelProbePrivate::FindMissingField(Target, Missing))
	{
		Result.Outcome = Missing;
		OnDone.ExecuteIfBound(Result);
		return;
	}

	Result.Url = GetRequestUrl(Target);
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Result.Url);
	Request->SetVerb(ShuttlegentProbeConstant::Verb);
	Request->SetTimeout(ShuttlegentProbeConstant::TimeoutSeconds);
	Request->SetHeader(ShuttlegentProbeConstant::HeaderContentType, ShuttlegentProbeConstant::ContentTypeJson);
	if (FShuttlegentModelProbePrivate::UsesAnthropicHeaders(Target))
	{
		Request->SetHeader(ShuttlegentProbeConstant::HeaderAnthropicKey, Target.ApiKey);
		Request->SetHeader(ShuttlegentProbeConstant::HeaderAnthropicVersion, ShuttlegentProbeConstant::AnthropicVersion);
	}
	else
	{
		Request->SetHeader(ShuttlegentProbeConstant::HeaderAuthorization, FString(ShuttlegentProbeConstant::BearerPrefix) + Target.ApiKey);
	}
	Request->SetContentAsString(FShuttlegentModelProbePrivate::MakeBody(Target));

	const double StartSeconds = FPlatformTime::Seconds();
	Request->OnProcessRequestComplete().BindLambda([Result, OnDone, StartSeconds](FHttpRequestPtr CompletedRequest, FHttpResponsePtr Response, bool bConnected) mutable
	{
		Result.ElapsedMs = FMath::RoundToInt((FPlatformTime::Seconds() - StartSeconds) * ShuttlegentProbeConstant::MillisecondsPerSecond);

		if (!bConnected || !Response.IsValid())
		{
			Result.Outcome = EShuttlegentModelProbeOutcome::Unreachable;
			Result.bTimedOut = CompletedRequest.IsValid() && CompletedRequest->GetFailureReason() == EHttpFailureReason::TimedOut;
			UE_LOG(LogShuttlegent, Log, TEXT("Model probe: POST %s -> no response (%s) in %d ms."), *Result.Url, Result.bTimedOut ? TEXT("timed out") : TEXT("connection failed"), Result.ElapsedMs);
			OnDone.ExecuteIfBound(Result);
			return;
		}

		const FString Body = Response->GetContentAsString();
		Result.StatusCode = Response->GetResponseCode();
		Result.Outcome = FShuttlegentModelProbePrivate::Classify(Result.StatusCode, Response->GetContentType(), Body);
		Result.Detail = Body.Left(ShuttlegentProbeConstant::DetailMaxChars);
		UE_LOG(LogShuttlegent, Log, TEXT("Model probe: POST %s -> %d in %d ms."), *Result.Url, Result.StatusCode, Result.ElapsedMs);
		OnDone.ExecuteIfBound(Result);
	});
	Request->ProcessRequest();
}
