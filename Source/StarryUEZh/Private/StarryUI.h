// Copyright 2026 StarGame. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FStarryUI
{
public:
	static void Register();
	static void Unregister();

	static void SetEnabled(bool bEnabled);
	static void Toggle();
	static bool IsEnabled();
	static void ReloadDictionary();
	static void ExportDictionary();
	/** Open Content/Dictionary in Explorer and copy its path to the clipboard. */
	static void OpenDictionaryFolder();
	static FText GetButtonLabel();
	static FText GetTooltip();

	/** Debug helper: create a material with a few expressions and open it. */
	static void OpenTestMaterial();

	/** Debug helper: close and reopen the test material editor. */
	static void ReopenTestMaterial();
};

class FStarryCommands : public TCommands<FStarryCommands>
{
public:
	FStarryCommands();

	virtual void RegisterCommands() override;

	TSharedPtr<FUICommandInfo> Toggle;
};
