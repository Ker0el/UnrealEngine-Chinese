// Copyright 2026 StarGame. All Rights Reserved.

#include "StarryAI.h"

#include "Dom/JsonObject.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "StarryUEZh.h"
#include "StarryDictionary.h"

namespace
{
	const TCHAR* SystemPrompt =
		TEXT("你是虚幻引擎编辑器的本地化翻译。用户会给出一个 JSON 字符串数组，")
		TEXT("把数组里的每个英文界面词条翻译成简体中文。只输出一个 JSON 对象，")
		TEXT("键是原始英文，值是中文译文。不要解释，不要 Markdown，不要代码块。")
		TEXT("完整句子按中文表达习惯意译，不要逐词直译；同类术语全批保持同一译法。")
		TEXT("Nanite、Lumen、Niagara、PCG、GPU、HDR、LOD 这类专有名词保留原文，")
		TEXT("数字、单位和 %s 这类占位符原样保留。");

	FString StripCodeFence(const FString& Content)
	{
		int32 Start = Content.Find(TEXT("{"));
		int32 End = Content.Find(TEXT("}"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
		if (Start != INDEX_NONE && End != INDEX_NONE && End > Start)
		{
			return Content.Mid(Start, End - Start + 1);
		}
		return Content;
	}
}

void FStarryAI::FetchModels(const FString& ApiKey, FOnStarryAIModels OnDone)
{
	if (ApiKey.IsEmpty())
	{
		OnDone.ExecuteIfBound(false, FStarryAIModelList(), TEXT("API Key 为空"));
		return;
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(TEXT("https://api.deepseek.com/models"));
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
	Request->SetTimeout(30.0f);

	Request->OnProcessRequestComplete().BindLambda(
		[OnDone](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			if (!bConnected || !Response.IsValid())
			{
				OnDone.ExecuteIfBound(false, FStarryAIModelList(), TEXT("网络连接失败，检查网络"));
				return;
			}
			const int32 Status = Response->GetResponseCode();
			const FString Body = Response->GetContentAsString();
			if (Status < 200 || Status >= 300)
			{
				OnDone.ExecuteIfBound(false, FStarryAIModelList(),
					FString::Printf(TEXT("接口返回 %d：%s"), Status, *Body.Left(200)));
				return;
			}

			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Body);
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				OnDone.ExecuteIfBound(false, FStarryAIModelList(), TEXT("返回内容不是 JSON"));
				return;
			}

			const TArray<TSharedPtr<FJsonValue>>* Data = nullptr;
			if (!Root->TryGetArrayField(TEXT("data"), Data) || !Data)
			{
				OnDone.ExecuteIfBound(false, FStarryAIModelList(), TEXT("返回里没有 data"));
				return;
			}

			FStarryAIModelList Models;
			for (const TSharedPtr<FJsonValue>& Value : *Data)
			{
				const TSharedPtr<FJsonObject> Object = Value.IsValid() ? Value->AsObject() : nullptr;
				FString Id;
				if (Object.IsValid() && Object->TryGetStringField(TEXT("id"), Id) && !Id.IsEmpty())
				{
					Models.Add(Id);
				}
			}

			if (Models.Num() == 0)
			{
				OnDone.ExecuteIfBound(false, FStarryAIModelList(), TEXT("没有可用模型"));
				return;
			}
			OnDone.ExecuteIfBound(true, Models, FString());
		});

	if (!Request->ProcessRequest())
	{
		OnDone.ExecuteIfBound(false, FStarryAIModelList(), TEXT("请求发送失败"));
	}
}

void FStarryAI::Translate(
	const FString& Url,
	const FString& ApiKey,
	const FString& Model,
	const TArray<FString>& Sources,
	FOnStarryAITranslated OnDone)
{
	if (Url.IsEmpty() || ApiKey.IsEmpty() || Model.IsEmpty())
	{
		OnDone.ExecuteIfBound(false, TMap<FString, FString>(), TEXT("请先填写 API 地址、API Key 和模型名"));
		return;
	}
	if (Sources.Num() == 0)
	{
		OnDone.ExecuteIfBound(false, TMap<FString, FString>(), TEXT("没有需要翻译的词条"));
		return;
	}

	// User message: a plain JSON array of source strings.
	TArray<TSharedPtr<FJsonValue>> SourceValues;
	SourceValues.Reserve(Sources.Num());
	for (const FString& Source : Sources)
	{
		SourceValues.Add(MakeShared<FJsonValueString>(Source));
	}
	FString SourceJson;
	const TSharedRef<TJsonWriter<>> SourceWriter = TJsonWriterFactory<>::Create(&SourceJson);
	FJsonSerializer::Serialize(SourceValues, SourceWriter);

	TArray<TSharedPtr<FJsonValue>> Messages;

	TSharedPtr<FJsonObject> SystemMessage = MakeShared<FJsonObject>();
	SystemMessage->SetStringField(TEXT("role"), TEXT("system"));

	FString Prompt = SystemPrompt;
	const TMap<FString, FString>& Glossary = FStarryDictionary::Get().GetGlossary();
	if (Glossary.Num() > 0)
	{
		Prompt += TEXT("\n以下固定译法必须原样使用：");
		for (const TPair<FString, FString>& Pair : Glossary)
		{
			Prompt += FString::Printf(TEXT("%s=%s；"), *Pair.Key, *Pair.Value);
		}
	}
	SystemMessage->SetStringField(TEXT("content"), Prompt);
	Messages.Add(MakeShared<FJsonValueObject>(SystemMessage));

	TSharedPtr<FJsonObject> UserMessage = MakeShared<FJsonObject>();
	UserMessage->SetStringField(TEXT("role"), TEXT("user"));
	UserMessage->SetStringField(TEXT("content"), SourceJson);
	Messages.Add(MakeShared<FJsonValueObject>(UserMessage));

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("model"), Model);
	Body->SetNumberField(TEXT("temperature"), 0.2);
	Body->SetArrayField(TEXT("messages"), Messages);

	FString BodyText;
	const TSharedRef<TJsonWriter<>> BodyWriter = TJsonWriterFactory<>::Create(&BodyText);
	if (!FJsonSerializer::Serialize(Body.ToSharedRef(), BodyWriter))
	{
		OnDone.ExecuteIfBound(false, TMap<FString, FString>(), TEXT("请求体生成失败"));
		return;
	}

	const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *ApiKey));
	Request->SetTimeout(90.0f);
	Request->SetContentAsString(BodyText);

	Request->OnProcessRequestComplete().BindLambda(
		[OnDone](FHttpRequestPtr, FHttpResponsePtr Response, bool bConnected)
		{
			if (!bConnected || !Response.IsValid())
			{
				OnDone.ExecuteIfBound(false, TMap<FString, FString>(), TEXT("网络连接失败，检查 API 地址和网络"));
				return;
			}

			const int32 Status = Response->GetResponseCode();
			const FString ResponseText = Response->GetContentAsString();
			if (Status < 200 || Status >= 300)
			{
				FString Snippet = ResponseText.Left(200);
				OnDone.ExecuteIfBound(false, TMap<FString, FString>(),
					FString::Printf(TEXT("接口返回 %d：%s"), Status, *Snippet));
				return;
			}

			TSharedPtr<FJsonObject> Root;
			const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(ResponseText);
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
			{
				OnDone.ExecuteIfBound(false, TMap<FString, FString>(), TEXT("接口返回的不是 JSON"));
				return;
			}

			const TArray<TSharedPtr<FJsonValue>>* Choices = nullptr;
			if (!Root->TryGetArrayField(TEXT("choices"), Choices) || !Choices || Choices->Num() == 0)
			{
				OnDone.ExecuteIfBound(false, TMap<FString, FString>(), TEXT("接口返回里没有 choices"));
				return;
			}

			const TSharedPtr<FJsonObject> Choice = (*Choices)[0]->AsObject();
			if (!Choice.IsValid())
			{
				OnDone.ExecuteIfBound(false, TMap<FString, FString>(), TEXT("choices 格式不对"));
				return;
			}
			const TSharedPtr<FJsonObject>* MessagePtr = nullptr;
			if (!Choice->TryGetObjectField(TEXT("message"), MessagePtr) || !MessagePtr)
			{
				OnDone.ExecuteIfBound(false, TMap<FString, FString>(), TEXT("返回里没有 message"));
				return;
			}
			FString Content;
			(*MessagePtr)->TryGetStringField(TEXT("content"), Content);
			Content = StripCodeFence(Content);

			TSharedPtr<FJsonObject> TranslationObject;
			const TSharedRef<TJsonReader<TCHAR>> ContentReader = TJsonReaderFactory<TCHAR>::Create(Content);
			if (!FJsonSerializer::Deserialize(ContentReader, TranslationObject) || !TranslationObject.IsValid())
			{
				OnDone.ExecuteIfBound(false, TMap<FString, FString>(), TEXT("模型输出不是 JSON，请重试或换模型"));
				return;
			}

			TMap<FString, FString> Result;
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : TranslationObject->Values)
			{
				FString Translation;
				if (Pair.Value.IsValid() && Pair.Value->TryGetString(Translation) && !Translation.IsEmpty())
				{
					Result.Add(Pair.Key, Translation);
				}
			}

			if (Result.Num() == 0)
			{
				OnDone.ExecuteIfBound(false, TMap<FString, FString>(), TEXT("模型没有返回任何译文"));
				return;
			}
			OnDone.ExecuteIfBound(true, Result, FString());
		});

	if (!Request->ProcessRequest())
	{
		OnDone.ExecuteIfBound(false, TMap<FString, FString>(), TEXT("请求发送失败"));
	}
}
