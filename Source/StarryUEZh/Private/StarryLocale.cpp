// Copyright 2026 StarGame. All Rights Reserved.

#include "StarryLocale.h"

#include "StarryUEZh.h"

#include "HAL/FileManager.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"

namespace
{
	void LoadRoot(const FString& LocalizationRoot, const TCHAR* Culture, int32 Priority,
		FTextLocalizationResource& Resource, int32& OutFiles)
	{
		if (!IFileManager::Get().DirectoryExists(*LocalizationRoot))
		{
			return;
		}

		TArray<FString> TargetNames;
		const FString Pattern = FPaths::Combine(LocalizationRoot, TEXT("*"));
		IFileManager::Get().FindFiles(TargetNames, *Pattern, false, true);
		for (const FString& TargetName : TargetNames)
		{
			const FString LocResPath = FPaths::Combine(LocalizationRoot, TargetName, Culture, TargetName + TEXT(".locres"));
			if (!IFileManager::Get().FileExists(*LocResPath))
			{
				continue;
			}
			if (Resource.LoadFromFile(LocResPath, Priority))
			{
				++OutFiles;
			}
		}
	}
}

void StarryLocale::Apply(bool bChinese)
{
	const TCHAR* Culture = bChinese ? TEXT("zh-Hans") : TEXT("en");

	FTextLocalizationResource Resource;
	int32 LoadedFiles = 0;

	// Engine first, project next, plugins last. Smaller priority numbers win,
	// so a project/plugin resource can override an engine entry with the same key.
	LoadRoot(FPaths::Combine(FPaths::EngineContentDir(), TEXT("Localization")), Culture, -10000, Resource, LoadedFiles);
	LoadRoot(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Localization")), Culture, -11000, Resource, LoadedFiles);

	for (const TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetEnabledPlugins())
	{
		LoadRoot(FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/Localization")), Culture, -12000, Resource, LoadedFiles);
	}

	if (Resource.IsEmpty())
	{
		UE_LOG(LogStarryUEZh, Warning, TEXT("语言切换失败：没有找到 %s 的 locres"), Culture);
		return;
	}

	FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
	UE_LOG(LogStarryUEZh, Log, TEXT("语言资源已切换：%s，文件 %d 个，条目 %d 条"),
		Culture, LoadedFiles, Resource.Entries.Num());
}
