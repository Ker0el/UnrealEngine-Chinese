// Copyright 2026 StarGame. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

using FStarryAITranslationMap = TMap<FString, FString>;
using FStarryAIModelList = TArray<FString>;

DECLARE_DELEGATE_ThreeParams(FOnStarryAITranslated, bool, const FStarryAITranslationMap&, const FString&);
DECLARE_DELEGATE_ThreeParams(FOnStarryAIModels, bool, const FStarryAIModelList&, const FString&);

/**
 * Sends one batch of UI strings to an OpenAI-compatible chat completions API and
 * returns the English -> Chinese map. The HTTP module delivers the callback on
 * the game thread, so callers can touch Slate state directly.
 */
class FStarryAI
{
public:
	/** Query the provider's model list. */
	static void FetchModels(const FString& ApiKey, FOnStarryAIModels OnDone);

	static void Translate(
		const FString& Url,
		const FString& ApiKey,
		const FString& Model,
		const TArray<FString>& Sources,
		FOnStarryAITranslated OnDone);
};
