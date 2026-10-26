// Copyright 2026 StarGame. All Rights Reserved.

#include "StarryUEZh.h"

#include "StarryDictionary.h"
#include "StarryHooks.h"
#include "StarryUI.h"

#include "Containers/Ticker.h"
#include "Framework/Docking/TabManager.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogStarryUEZh);

namespace
{
	FString GetDictionaryDirectory()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("StarryUEZh"));
		return Plugin.IsValid() ? FPaths::Combine(Plugin->GetContentDir(), TEXT("Dictionary")) : FString();
	}

	FDateTime GetNewestDictionaryWrite(const FString& Directory)
	{
		FDateTime Newest = FDateTime::MinValue();
		if (Directory.IsEmpty())
		{
			return Newest;
		}
		TArray<FString> Files;
		IFileManager::Get().FindFiles(Files, *(Directory / TEXT("*.json")), true, false);
		for (const FString& File : Files)
		{
			const FDateTime Time = IFileManager::Get().GetTimeStamp(*(Directory / File));
			if (Time > Newest)
			{
				Newest = Time;
			}
		}
		return Newest;
	}
}

void FStarryUEZhModule::StartupModule()
{
	UE_LOG(LogStarryUEZh, Log, TEXT("星空 UE 汉化启动"));

	const bool bLoaded = FStarryDictionary::Get().Load();

	bool bEnabled = true;
	if (!GConfig->GetBool(TEXT("StarryUEZh"), TEXT("Enabled"), bEnabled, GEditorPerProjectIni))
	{
		bEnabled = true;
	}
	FStarryDictionary::Get().SetEnabled(bEnabled);

	if (bLoaded)
	{
		StarryHooks::Install();
		FStarryUI::Register();

		// Keep the untranslated list on disk without asking the user to click
		// anything, so the word tables can be refreshed from the real gaps.
		GapsTickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			StarryHooks::DumpGaps();
			return true;
		}), 180.0f);

		// Watch Content/Dictionary so edits made outside the editor show up
		// without a manual reload. Wait 1.5 s after the last write so a file
		// that is still being saved is not read half-way.
		const FString WatchDirectory = GetDictionaryDirectory();
		FDateTime LastWrite = GetNewestDictionaryWrite(WatchDirectory);
		double LastChangeAt = 0.0;
		DictionaryTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WatchDirectory, LastWrite, LastChangeAt](float) mutable
		{
			const FDateTime Newest = GetNewestDictionaryWrite(WatchDirectory);
			if (Newest > LastWrite)
			{
				LastWrite = Newest;
				LastChangeAt = FPlatformTime::Seconds();
				return true;
			}
			if (LastChangeAt > 0.0 && FPlatformTime::Seconds() - LastChangeAt >= 1.5)
			{
				LastChangeAt = 0.0;
				FStarryUI::ReloadDictionary();
			}
			return true;
		}), 1.0f);

		if (FParse::Param(FCommandLine::Get(), TEXT("StarryPanel")))
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("StarryUEZh")));
				return false;
			}), 6.0f);
		}

		if (FParse::Param(FCommandLine::Get(), TEXT("StarryTestMaterial"))
			|| FParse::Param(FCommandLine::Get(), TEXT("StarryAutoDiag")))
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				FStarryUI::OpenTestMaterial();
				return false;
			}), 8.0f);
		}

		if (FParse::Param(FCommandLine::Get(), TEXT("StarryAutoDiag")))
		{
			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				FStarryUI::SetEnabled(false);
				return false;
			}), 12.0f);

			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				FStarryUI::ReopenTestMaterial();
				return false;
			}), 16.0f);

			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				FStarryUI::SetEnabled(true);
				return false;
			}), 40.0f);

			FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
			{
				StarryHooks::DumpGaps();
				StarryHooks::HarvestStatic();
				return false;
			}), 45.0f);
		}
	}
	else
	{
		UE_LOG(LogStarryUEZh, Error, TEXT("词库不可用，插件保持关闭"));
	}

	UE_LOG(LogStarryUEZh, Log, TEXT("启动完成：词条 %d，显示 %s"), FStarryDictionary::Get().Num(),
		bEnabled ? TEXT("中文") : TEXT("English"));
}

void FStarryUEZhModule::ShutdownModule()
{
	if (GapsTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(GapsTickerHandle);
		GapsTickerHandle.Reset();
	}
	if (DictionaryTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(DictionaryTickerHandle);
		DictionaryTickerHandle.Reset();
	}
	StarryHooks::DumpGaps();
	FStarryUI::Unregister();
	StarryHooks::Uninstall();
	UE_LOG(LogStarryUEZh, Log, TEXT("星空 UE 汉化已卸载"));
}

IMPLEMENT_MODULE(FStarryUEZhModule, StarryUEZh)
