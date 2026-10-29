// Copyright 2026 StarGame. All Rights Reserved.

#include "StarryUI.h"

#include "StarryDictionary.h"
#include "StarryEditorExtenders.h"
#include "StarryHooks.h"
#include "StarryLocale.h"
#include "StarryPanel.h"
#include "StarryToolbarButton.h"
#include "StarryUEZh.h"
#include "Runtime/Launch/Resources/Version.h"

#include "AssetToolsModule.h"
#include "Editor.h"
#include "Framework/Commands/InputChord.h"
#include "Framework/Commands/UICommandInfo.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "LevelEditor.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Misc/ConfigCacheIni.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Containers/Ticker.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "StarryUEZh"

namespace
{
	TSharedPtr<FUICommandList> GCommandList;
	bool bUIRegistered = false;

	const TCHAR* ConfigSection = TEXT("StarryUEZh");
	const TCHAR* ConfigKeyEnabled = TEXT("Enabled");

	class FStarryInputProcessor : public IInputProcessor
	{
	public:
		virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override
		{
		}

		virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
		{
			if (InKeyEvent.GetKey() == EKeys::F1
				&& !InKeyEvent.IsRepeat()
				&& !InKeyEvent.IsControlDown()
				&& !InKeyEvent.IsAltDown()
				&& !InKeyEvent.IsShiftDown()
				&& !InKeyEvent.IsCommandDown())
			{
				FStarryUI::Toggle();
				return true;
			}
			return false;
		}
	};

	TSharedPtr<FStarryInputProcessor> GInputProcessor;

	// UMaterial::GetExpressionCollection only exists from 5.1. 5.0 keeps the
	// expression array as a public member on the material itself.
	int32 NumMaterialExpressions(const UMaterial* Material)
	{
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
		return Material->GetExpressionCollection().Expressions.Num();
#else
		return Material->Expressions.Num();
#endif
	}
}

FStarryCommands::FStarryCommands()
	: TCommands<FStarryCommands>(
		TEXT("StarryUEZh"),
		NSLOCTEXT("StarryUEZh", "StarryUEZhCommands", "星空汉化"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FStarryCommands::RegisterCommands()
{
	UI_COMMAND(
		Toggle,
		"切换中英显示",
		"切换蓝图 / 材质 / Niagara 节点的中文显示（F1）",
		EUserInterfaceActionType::ToggleButton,
		FInputChord(EKeys::F1));
}

void FStarryUI::Register()
{
	if (bUIRegistered)
	{
		return;
	}
	bUIRegistered = true;

	FStarryCommands::Register();

	StarryPanel::Register();
	StarryEditorExtenders::Register();

	GCommandList = MakeShared<FUICommandList>();
	GCommandList->MapAction(
		FStarryCommands::Get().Toggle,
		FExecuteAction::CreateStatic(&FStarryUI::Toggle),
		FCanExecuteAction::CreateStatic([]() { return true; }),
		FIsActionChecked::CreateStatic(&FStarryUI::IsEnabled));

	if (FModuleManager::Get().IsModuleLoaded(TEXT("LevelEditor")))
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::GetModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		TSharedPtr<FUICommandList> GlobalActions = LevelEditorModule.GetGlobalLevelEditorActions();
		if (GlobalActions.IsValid())
		{
			GlobalActions->Append(GCommandList.ToSharedRef());
		}
	}

	// F1 toggles from anywhere in the editor, including graph editors that do not
	// share the level editor command list. The preprocessor runs before widgets,
	// so the toggle works while a material / Niagara / blueprint graph has focus.
	if (!GInputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		GInputProcessor = MakeShared<FStarryInputProcessor>();
		FSlateApplication::Get().RegisterInputPreProcessor(GInputProcessor, 0);
	}

	UToolMenus* Menus = UToolMenus::Get();
	if (Menus)
	{
		// Tools menu entry, useful when the toolbar is hidden.
		UToolMenu* ToolsMenu = Menus->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
		if (ToolsMenu)
		{
			FToolMenuSection& Section = ToolsMenu->FindOrAddSection(TEXT("StarryUEZh"));
			Section.AddMenuEntry(
				TEXT("StarryUEZh.ToggleMenu"),
				LOCTEXT("ToggleMenuLabel", "星空汉化：切换中英显示"),
				LOCTEXT("ToggleMenuTooltip", "切换蓝图 / 材质 / Niagara 节点的中文显示（F1）"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings")),
				FUIAction(
					FExecuteAction::CreateStatic(&FStarryUI::Toggle),
					FCanExecuteAction::CreateStatic([]() { return true; }),
					FIsActionChecked::CreateStatic(&FStarryUI::IsEnabled)),
				EUserInterfaceActionType::ToggleButton);

			Section.AddMenuEntry(
				TEXT("StarryUEZh.OpenPanel"),
				LOCTEXT("OpenPanelLabel", "星空汉化：打开面板"),
				LOCTEXT("OpenPanelTooltip", "扫描词条、重新加载词条、切换中英"),
				FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings")),
				FUIAction(FExecuteAction::CreateStatic([]()
				{
					FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("StarryUEZh")));
				})),
				EUserInterfaceActionType::Button);
		}
	}
}

void FStarryUI::Unregister()
{
	if (!bUIRegistered)
	{
		return;
	}
	bUIRegistered = false;

	if (GInputProcessor.IsValid())
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().UnregisterInputPreProcessor(GInputProcessor);
		}
		GInputProcessor.Reset();
	}

	StarryEditorExtenders::Unregister();
	StarryPanel::Unregister();
	FStarryCommands::Unregister();
	GCommandList.Reset();
}

void FStarryUI::SetEnabled(bool bEnabled)
{
	FStarryDictionary::Get().SetEnabled(bEnabled);
	StarryLocale::Apply(bEnabled);
	StarryHooks::RequestRefresh();

	GConfig->SetBool(ConfigSection, ConfigKeyEnabled, bEnabled, GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);

	UE_LOG(LogStarryUEZh, Log, TEXT("显示模式：%s"), bEnabled ? TEXT("中文") : TEXT("English"));
}

void FStarryUI::Toggle()
{
	SetEnabled(!FStarryDictionary::Get().IsEnabled());
}

bool FStarryUI::IsEnabled()
{
	return FStarryDictionary::Get().IsEnabled();
}

void FStarryUI::ReloadDictionary()
{
	if (FStarryDictionary::Get().Reload())
	{
		StarryHooks::RequestRefresh();
		UE_LOG(LogStarryUEZh, Log, TEXT("词条已重新加载，共 %d 条"), FStarryDictionary::Get().Num());
	}
	else
	{
		UE_LOG(LogStarryUEZh, Error, TEXT("词条重新加载失败"));
	}
}

void FStarryUI::ExportDictionary()
{
	const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("StarryUEZh"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Path = FPaths::Combine(Directory, TEXT("dictionary_export.json"));
	if (FStarryDictionary::Get().Export(Path))
	{
		UE_LOG(LogStarryUEZh, Log, TEXT("词条已导出：%s"), *Path);
	}
	else
	{
		UE_LOG(LogStarryUEZh, Error, TEXT("词条导出失败：%s"), *Path);
	}
}

void FStarryUI::OpenDictionaryFolder()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("StarryUEZh"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogStarryUEZh, Warning, TEXT("找不到 StarryUEZh 插件，无法打开词库目录"));
		return;
	}

	FString Directory = FPaths::ConvertRelativePathToFull(FPaths::Combine(Plugin->GetContentDir(), TEXT("Dictionary")));
	Directory.ReplaceInline(TEXT("/"), TEXT("\\"));
	FPlatformApplicationMisc::ClipboardCopy(*Directory);
	UE_LOG(LogStarryUEZh, Log, TEXT("词库目录：%s"), *Directory);

	FNotificationInfo Info(FText::FromString(FString::Printf(
		TEXT("词库目录（路径已复制到剪贴板）：\n%s"), *Directory)));
	Info.ExpireDuration = 8.0f;
	FSlateNotificationManager::Get().AddNotification(Info);

	FPlatformProcess::ExploreFolder(*Directory);
}

FText FStarryUI::GetButtonLabel()
{
	return FText::FromString(FStarryDictionary::Get().IsEnabled() ? TEXT("切换中文（F1）") : TEXT("切换英文（F1）"));
}

FText FStarryUI::GetTooltip()
{
	return FStarryDictionary::Get().IsEnabled()
		? LOCTEXT("TooltipOn", "当前：中文显示。点击或按 F1 切回英文。")
		: LOCTEXT("TooltipOff", "当前：英文显示。点击或按 F1 切到中文。");
}

void FStarryUI::OpenTestMaterial()
{
	UMaterial* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/NewMaterial.NewMaterial"));
	if (!Material)
	{
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		Material = Cast<UMaterial>(AssetToolsModule.Get().CreateAsset(
			TEXT("NewMaterial"), TEXT("/Game"), UMaterial::StaticClass(), nullptr));
	}

	if (!Material)
	{
		UE_LOG(LogStarryUEZh, Error, TEXT("测试材质创建失败"));
		return;
	}

	if (NumMaterialExpressions(Material) == 0)
	{
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureCoordinate::StaticClass(), -760, -180);
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionTextureSample::StaticClass(), -520, -160);
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionConstant3Vector::StaticClass(), -520, 120);
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionComponentMask::StaticClass(), -520, 320);
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionAdd::StaticClass(), -240, 20);
		UMaterialEditingLibrary::CreateMaterialExpression(Material, UMaterialExpressionMultiply::StaticClass(), -240, 240);
		Material->PostEditChange();
	}

	if (GEditor)
	{
		UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
		if (AssetEditorSubsystem)
		{
			AssetEditorSubsystem->OpenEditorForAsset(Material);
			UE_LOG(LogStarryUEZh, Log, TEXT("已打开测试材质：%s，表达式 %d 个"),
				*Material->GetPathName(), NumMaterialExpressions(Material));
			return;
		}
	}
	UE_LOG(LogStarryUEZh, Error, TEXT("无法打开测试材质"));
}

void FStarryUI::ReopenTestMaterial()
{
	UMaterial* Material = LoadObject<UMaterial>(nullptr, TEXT("/Game/NewMaterial.NewMaterial"));
	if (!Material || !GEditor)
	{
		return;
	}
	UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
	if (!AssetEditorSubsystem)
	{
		return;
	}
	AssetEditorSubsystem->CloseAllEditorsForAsset(Material);
	AssetEditorSubsystem->OpenEditorForAsset(Material);
	UE_LOG(LogStarryUEZh, Log, TEXT("已重新打开测试材质，用于重建节点标签"));
}

static FAutoConsoleCommand GStarryToggleCommand(
	TEXT("StarryZh.Toggle"),
	TEXT("切换星空汉化的中文/英文显示"),
	FConsoleCommandDelegate::CreateStatic(&FStarryUI::Toggle));

static FAutoConsoleCommand GStarryOnCommand(
	TEXT("StarryZh.On"),
	TEXT("打开星空汉化"),
	FConsoleCommandDelegate::CreateStatic([]() { FStarryUI::SetEnabled(true); }));

static FAutoConsoleCommand GStarryOffCommand(
	TEXT("StarryZh.Off"),
	TEXT("关闭星空汉化"),
	FConsoleCommandDelegate::CreateStatic([]() { FStarryUI::SetEnabled(false); }));

static FAutoConsoleCommand GStarryStatusCommand(
	TEXT("StarryZh.Status"),
	TEXT("显示星空汉化状态"),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		UE_LOG(LogStarryUEZh, Log, TEXT("状态：%s，词条：%d，Niagara 钩子：%s"),
			FStarryDictionary::Get().IsEnabled() ? TEXT("中文") : TEXT("English"),
			FStarryDictionary::Get().Num(),
			StarryHooks::HasNiagaraHooks() ? TEXT("已挂载") : TEXT("未挂载"));
	}));

static FAutoConsoleCommand GStarryOpenTestCommand(
	TEXT("StarryZh.OpenTestMaterial"),
	TEXT("创建并打开一个带节点的测试材质"),
	FConsoleCommandDelegate::CreateStatic(&FStarryUI::OpenTestMaterial));

static FAutoConsoleCommand GStarryReopenTestCommand(
	TEXT("StarryZh.ReopenTestMaterial"),
	TEXT("重新打开测试材质，重建节点标签"),
	FConsoleCommandDelegate::CreateStatic(&FStarryUI::ReopenTestMaterial));

static FAutoConsoleCommand GStarryReloadCommand(
	TEXT("StarryZh.Reload"),
	TEXT("重新加载 Content/Dictionary 下的词条，不用重启编辑器"),
	FConsoleCommandDelegate::CreateStatic(&FStarryUI::ReloadDictionary));

static FAutoConsoleCommand GStarryExportCommand(
	TEXT("StarryZh.ExportDictionary"),
	TEXT("导出当前词条到 Saved/StarryUEZh/dictionary_export.json"),
	FConsoleCommandDelegate::CreateStatic(&FStarryUI::ExportDictionary));

static FAutoConsoleCommand GStarryDumpGapsCommand(
	TEXT("StarryZh.DumpGaps"),
	TEXT("把运行时没命中的显示字符串写到 Saved/StarryUEZh/gaps.tsv"),
	FConsoleCommandDelegate::CreateStatic(&StarryHooks::DumpGaps));

static FAutoConsoleCommand GStarryHarvestCommand(
	TEXT("StarryZh.Harvest"),
	TEXT("扫描材质表达式 / 函数 / 枚举 / 属性，写到 Saved/StarryUEZh/harvest.tsv"),
	FConsoleCommandDelegate::CreateStatic(&StarryHooks::HarvestStatic));

static FAutoConsoleCommand GStarryDiagCommand(
	TEXT("StarryZh.Diag"),
	TEXT("输出星空汉化的诊断信息"),
	FConsoleCommandDelegate::CreateStatic([]()
	{
		int64 Calls = 0;
		int64 Hits = 0;
		StarryHooks::GetSetTextStats(Calls, Hits);
		UE_LOG(LogStarryUEZh, Log, TEXT("诊断：SetText 调用 %lld 次，命中 %lld 次，词条 %d"),
			Calls, Hits, FStarryDictionary::Get().Num());

		const TCHAR* Probes[] = { TEXT("Texture Sample"), TEXT("Add"), TEXT("Multiply"), TEXT("Material"), TEXT("Constant3Vector"), TEXT("TexCoord[0]"), TEXT("TexCoord[1]"), TEXT("TexCoord[2]"), TEXT("TexCoord[3]"), TEXT("TexCoord[4]"), TEXT("TexCoord[5]"), TEXT("TexCoord[6]"), TEXT("TexCoord[7]"), TEXT("TexCoord[8]"), TEXT("TexCoord[9]"), TEXT("Flatten Normal") };
		for (const TCHAR* Probe : Probes)
		{
			FString Translated;
			const bool bHit = FStarryDictionary::Get().Translate(Probe, Translated);
			UE_LOG(LogStarryUEZh, Log, TEXT("  词库测试 '%s' -> 命中=%d '%s'"), Probe, bHit ? 1 : 0, *Translated);
		}
	}));

#undef LOCTEXT_NAMESPACE
