// Copyright 2026 StarGame. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/** Stores the DeepSeek API key in the Windows Credential Manager. */
class FStarryCredential
{
public:
	static bool SaveApiKey(const FString& Key);
	static bool LoadApiKey(FString& OutKey);
	static void DeleteApiKey();
};
