#include "FShuttlegentSessionTitler.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformTime.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#include "FShuttlegentModelProbe.h"
#include "FShuttlegentModelRoutes.h"
#include "ShuttlegentConstants.h"
#include "ShuttlegentLog.h"
#include "ShuttlegentSettings.h"

namespace FShuttlegentSessionTitlerPrivate
{
	/** 会话 "model" 取值（["路由", "模型 ID"]）对应的路由与模型；对不上时为默认模型。没有已连通的连接时返回 false。 */
	bool FindModel(const UShuttlegentSettings& Settings, const FString& ModelValue, FShuttlegentModelRoute& OutRoute, FString& OutModelId)
	{
		TArray<TSharedPtr<FJsonValue>> Parts;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ModelValue);
		if (FJsonSerializer::Deserialize(Reader, Parts) && Parts.Num() >= 2 && Parts[0].IsValid() && Parts[1].IsValid())
		{
			const FString RouteName = Parts[0]->AsString();
			const FString ModelId = Parts[1]->AsString();
			for (const FShuttlegentModelRoute& Route : FShuttlegentModelRoutes::Build(Settings))
			{
				if (Route.Route != RouteName || !Route.ModelIds.Contains(ModelId)) continue;

				OutRoute = Route;
				OutModelId = ModelId;
				return true;
			}
		}
		return FShuttlegentModelRoutes::FindDefaultModel(Settings, OutRoute, OutModelId);
	}

	/** 请求体。Chat Completions 不设输出上限：新的 OpenAI 模型不接受 max_tokens，兼容服务又未必认 max_completion_tokens，提示词已要求不超过 16 字。 */
	FString MakeBody(const FShuttlegentModelRoute& Target, const FString& ModelId, const FString& Prompt)
	{
		const TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(ShuttlegentProbeConstant::FieldModel, ModelId);

		if (!Target.bBuiltin && Target.Api == EShuttlegentProviderApi::OpenAiResponses)
		{
			Body->SetNumberField(ShuttlegentProbeConstant::FieldMaxOutputTokens, ShuttlegentTitleConstant::MaxOutputTokens);
			Body->SetStringField(ShuttlegentProbeConstant::FieldInput, Prompt);
		}
		else
		{
			const TSharedRef<FJsonObject> Message = MakeShared<FJsonObject>();
			Message->SetStringField(ShuttlegentProbeConstant::FieldRole, ShuttlegentProbeConstant::RoleUser);
			Message->SetStringField(ShuttlegentProbeConstant::FieldContent, Prompt);
			if (Target.bBuiltin || Target.Api == EShuttlegentProviderApi::AnthropicMessages) Body->SetNumberField(ShuttlegentProbeConstant::FieldMaxTokens, ShuttlegentTitleConstant::MaxOutputTokens);
			Body->SetArrayField(ShuttlegentProbeConstant::FieldMessages, { MakeShared<FJsonValueObject>(Message) });
		}

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Body, Writer);
		return Json;
	}

	/** 数组中第一个 type 为 Type 的块的 text；没有时为空。 */
	FString FindTypedText(const TArray<TSharedPtr<FJsonValue>>& Blocks, const TCHAR* Type)
	{
		for (const TSharedPtr<FJsonValue>& Value : Blocks)
		{
			const TSharedPtr<FJsonObject> Block = Value.IsValid() ? Value->AsObject() : nullptr;
			FString BlockType;
			FString Text;
			if (Block.IsValid() && Block->TryGetStringField(ShuttlegentTitleConstant::FieldType, BlockType) && BlockType == Type && Block->TryGetStringField(ShuttlegentTitleConstant::FieldText, Text)) return Text;
		}
		return FString();
	}

	/** 三种协议响应中的回答文字：Messages 的 content[]，Chat Completions 的 choices[0].message.content，Responses 的 output[].content[]。 */
	FString ParseText(const FString& Body)
	{
		TSharedPtr<FJsonObject> Root;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Body);
		if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return FString();

		const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
		if (Root->TryGetArrayField(ShuttlegentProbeConstant::FieldContent, Content)) return FindTypedText(*Content, ShuttlegentTitleConstant::TypeText);

		const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
		if (Root->TryGetArrayField(ShuttlegentTitleConstant::FieldChoices, Choices) && !Choices->IsEmpty() && (*Choices)[0].IsValid())
		{
			const TSharedPtr<FJsonObject> Choice = (*Choices)[0]->AsObject();
			const TSharedPtr<FJsonObject>* Message = nullptr;
			FString Text;
			if (Choice.IsValid() && Choice->TryGetObjectField(ShuttlegentTitleConstant::FieldMessage, Message) && (*Message)->TryGetStringField(ShuttlegentProbeConstant::FieldContent, Text)) return Text;
			return FString();
		}

		const TArray<TSharedPtr<FJsonValue>>* Output = nullptr;
		if (!Root->TryGetArrayField(ShuttlegentTitleConstant::FieldOutput, Output)) return FString();

		for (const TSharedPtr<FJsonValue>& Value : *Output)
		{
			const TSharedPtr<FJsonObject> Item = Value.IsValid() ? Value->AsObject() : nullptr;
			const TArray<TSharedPtr<FJsonValue>>* ItemContent = nullptr;
			if (!Item.IsValid() || !Item->TryGetArrayField(ShuttlegentProbeConstant::FieldContent, ItemContent)) continue;

			const FString Text = FindTypedText(*ItemContent, ShuttlegentTitleConstant::TypeOutputText);
			if (!Text.IsEmpty()) return Text;
		}
		return FString();
	}

	/** 只取第一行，去掉首尾的引号与标点，超长截断。 */
	FString CleanTitle(const FString& Text)
	{
		TArray<FString> Lines;
		Text.ParseIntoArrayLines(Lines);
		FString Title = Lines.IsEmpty() ? FString() : Lines[0].TrimStartAndEnd();
		while (!Title.IsEmpty() && FCString::Strchr(ShuttlegentTitleConstant::TrimChars, Title[0]) != nullptr)
		{
			Title.RightChopInline(1);
		}
		while (!Title.IsEmpty() && FCString::Strchr(ShuttlegentTitleConstant::TrimChars, Title[Title.Len() - 1]) != nullptr)
		{
			Title.LeftChopInline(1);
		}
		Title.TrimStartAndEndInline();
		return Title.Left(ShuttlegentSessionConstant::TitleMaxChars);
	}
}

void FShuttlegentSessionTitler::Generate(const FString& ModelValue, const FString& UserText, const FString& AnswerText, FOnTitled OnDone)
{
	const UShuttlegentSettings* Settings = GetDefault<UShuttlegentSettings>();
	if (!IsValid(Settings))
	{
		OnDone.ExecuteIfBound(FString());
		return;
	}

	FShuttlegentModelRoute Target;
	FString ModelId;
	const bool bFound = FShuttlegentSessionTitlerPrivate::FindModel(*Settings, ModelValue, Target, ModelId);
	const FString Url = bFound ? FShuttlegentModelProbe::GetChatUrl(Target.BaseUrl, Target.Api, Target.bBuiltin) : FString();
	if (Url.IsEmpty() || Target.ApiKey.IsEmpty() || ModelId.IsEmpty())
	{
		UE_LOG(LogShuttlegent, Warning, TEXT("Session title: no connected model; the title stays as it is."));
		OnDone.ExecuteIfBound(FString());
		return;
	}

	const FString Prompt = FString::Format(ShuttlegentTitleConstant::PromptFormat, { UserText.Left(ShuttlegentTitleConstant::ExcerptMaxChars), AnswerText.Left(ShuttlegentTitleConstant::ExcerptMaxChars) });
	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(ShuttlegentProbeConstant::Verb);
	Request->SetTimeout(ShuttlegentTitleConstant::TimeoutSeconds);
	Request->SetHeader(ShuttlegentProbeConstant::HeaderContentType, ShuttlegentProbeConstant::ContentTypeJson);
	if (Target.bBuiltin || Target.Api == EShuttlegentProviderApi::AnthropicMessages)
	{
		Request->SetHeader(ShuttlegentProbeConstant::HeaderAnthropicKey, Target.ApiKey);
		Request->SetHeader(ShuttlegentProbeConstant::HeaderAnthropicVersion, ShuttlegentProbeConstant::AnthropicVersion);
	}
	else
	{
		Request->SetHeader(ShuttlegentProbeConstant::HeaderAuthorization, FString(ShuttlegentProbeConstant::BearerPrefix) + Target.ApiKey);
	}
	Request->SetContentAsString(FShuttlegentSessionTitlerPrivate::MakeBody(Target, ModelId, Prompt));

	const double StartSeconds = FPlatformTime::Seconds();
	Request->OnProcessRequestComplete().BindLambda([Url, OnDone, StartSeconds](FHttpRequestPtr CompletedRequest, FHttpResponsePtr Response, bool bConnected)
	{
		const int32 ElapsedMs = FMath::RoundToInt((FPlatformTime::Seconds() - StartSeconds) * 1000.0);
		const int32 StatusCode = Response.IsValid() ? Response->GetResponseCode() : 0;
		const bool bOk = bConnected && Response.IsValid() && StatusCode >= ShuttlegentProbeConstant::StatusOkMin && StatusCode <= ShuttlegentProbeConstant::StatusOkMax;
		const FString Title = bOk ? FShuttlegentSessionTitlerPrivate::CleanTitle(FShuttlegentSessionTitlerPrivate::ParseText(Response->GetContentAsString())) : FString();
		if (Title.IsEmpty())
		{
			UE_LOG(LogShuttlegent, Warning, TEXT("Session title: POST %s -> %d in %d ms, no title; the title stays as it is."), *Url, StatusCode, ElapsedMs);
		}
		else
		{
			UE_LOG(LogShuttlegent, Log, TEXT("Session title: POST %s -> %d in %d ms."), *Url, StatusCode, ElapsedMs);
		}
		OnDone.ExecuteIfBound(Title);
	});
	Request->ProcessRequest();
}
