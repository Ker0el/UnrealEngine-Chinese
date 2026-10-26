// Copyright 2026 StarGame. All Rights Reserved.

#include "StarryEditorExtenders.h"

#include "StarryToolbarButton.h"
#include "StarryUI.h"

#include "BlueprintEditorModule.h"
#include "Editor.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "LevelEditor.h"
#include "MaterialEditorModule.h"
#include "NiagaraEditorModule.h"
#include "Styling/AppStyle.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "ToolMenus.h"

#define LOCTEXT_NAMESPACE "StarryEditorExtenders"

namespace
{
	struct FInstalledExtender
	{
		TSharedPtr<FExtensibilityManager> Manager;
		TSharedPtr<FExtender> Extender;
	};

	TArray<FInstalledExtender> GInstalled;
	FDelegateHandle GAssetOpenedHandle;

	void AddToManager(const TSharedPtr<FExtensibilityManager>& Manager, const TSharedPtr<FExtender>& Extender)
	{
		if (Manager.IsValid() && Extender.IsValid())
		{
			Manager->AddExtender(Extender);
			GInstalled.Add({ Manager, Extender });
		}
	}

	TSharedRef<FExtender> MakeMenuExtender()
	{
		TSharedRef<FExtender> Extender = MakeShared<FExtender>();
		Extender->AddMenuExtension(TEXT("Window"), EExtensionHook::After, nullptr,
			FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& Builder)
			{
				Builder.AddMenuEntry(
					LOCTEXT("OpenPanelMenu", "星空汉化：打开面板"),
					LOCTEXT("OpenPanelMenuTip", "扫描词条、重新加载词条、切换中英"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings")),
					FUIAction(FExecuteAction::CreateLambda([]()
					{
						FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("StarryUEZh")));
					})));

				Builder.AddMenuEntry(
					LOCTEXT("ToggleMenu", "星空汉化：切换中 / 英"),
					LOCTEXT("ToggleMenuTip", "相当于 F1"),
					FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Star")),
					FUIAction(
						FExecuteAction::CreateStatic(&FStarryUI::Toggle),
						FCanExecuteAction::CreateStatic([]() { return true; }),
						FIsActionChecked::CreateStatic(&FStarryUI::IsEnabled)),
					NAME_None,
					EUserInterfaceActionType::ToggleButton);
			}));
		return Extender;
	}

	TSharedRef<FExtender> MakeLevelEditorExtender()
	{
		// Append after the Play section so the button sits right next to the
		// play controls in the viewport toolbar.
		TSharedRef<FExtender> Extender = MakeShared<FExtender>();
		Extender->AddToolBarExtension(TEXT("Play"), EExtensionHook::After, nullptr,
			FToolBarExtensionDelegate::CreateLambda([](FToolBarBuilder& Builder)
			{
				Builder.AddWidget(SNew(SStarryToolbarButton), NAME_None, true, HAlign_Fill);
			}));
		return Extender;
	}
}

namespace StarryEditorExtenders
{
	void Register()
	{
		if (FModuleManager::Get().ModuleExists(TEXT("LevelEditor")))
		{
			FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
			PRAGMA_DISABLE_DEPRECATION_WARNINGS
			AddToManager(LevelEditorModule.GetToolBarExtensibilityManager(), MakeLevelEditorExtender());
			PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}

		if (FModuleManager::Get().ModuleExists(TEXT("MaterialEditor")))
		{
			IMaterialEditorModule& MaterialModule = FModuleManager::LoadModuleChecked<IMaterialEditorModule>(TEXT("MaterialEditor"));
			AddToManager(MaterialModule.GetMenuExtensibilityManager(), MakeMenuExtender());
		}

		if (FModuleManager::Get().ModuleExists(TEXT("NiagaraEditor")))
		{
			FNiagaraEditorModule& NiagaraModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>(TEXT("NiagaraEditor"));
			AddToManager(NiagaraModule.GetMenuExtensibilityManager(), MakeMenuExtender());
		}

		if (FModuleManager::Get().ModuleExists(TEXT("Kismet")))
		{
			FBlueprintEditorModule& BlueprintModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>(TEXT("Kismet"));
			AddToManager(BlueprintModule.GetMenuExtensibilityManager(), MakeMenuExtender());
		}

		// Every asset editor builds its own toolbar menu. Add our section at the
		// end of that menu when the editor opens, then rebuild its toolbar, so the
		// button sits on the right in material, Niagara, blueprint, animation,
		// MetaSound and any other asset editor.
		if (GEditor)
		{
			if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				GAssetOpenedHandle = AssetEditorSubsystem->OnAssetOpenedInEditor().AddLambda(
					[](UObject* Asset, IAssetEditorInstance* Instance)
					{
						if (!Instance)
						{
							return;
						}
						FAssetEditorToolkit* Toolkit = static_cast<FAssetEditorToolkit*>(Instance);
						UToolMenus* Menus = UToolMenus::Get();
						if (!Toolkit || !Menus)
						{
							return;
						}
						const FName ToolbarName = Toolkit->GetToolMenuToolbarName();
						UToolMenu* Menu = Menus->FindMenu(ToolbarName);
						if (!Menu || !Menu->IsRegistered() || Menu->FindSection(TEXT("StarryUEZh")))
						{
							return;
						}
						UToolMenu* Extended = Menus->ExtendMenu(ToolbarName);
						if (!Extended)
						{
							return;
						}
						FToolMenuSection& Section = Extended->FindOrAddSection(TEXT("StarryUEZh"));
						// The tooltip argument was added in 5.2.
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 2
						Section.AddEntry(FToolMenuEntry::InitWidget(
							TEXT("StarryUEZh.AssetEditorButton"),
							SNew(SStarryToolbarButton),
							LOCTEXT("StarryToolbarLabel", "星空汉化"),
							false,
							true,
							true,
							LOCTEXT("StarryToolbarTip", "左键切换中文 / 英文，右键打开词条菜单")));
#else
						Section.AddEntry(FToolMenuEntry::InitWidget(
							TEXT("StarryUEZh.AssetEditorButton"),
							SNew(SStarryToolbarButton),
							LOCTEXT("StarryToolbarLabel", "星空汉化"),
							false,
							true,
							true));
#endif
						Toolkit->RegenerateMenusAndToolbars();
					});
			}
		}
	}

	void Unregister()
	{
		if (GEditor && GAssetOpenedHandle.IsValid())
		{
			if (UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>())
			{
				AssetEditorSubsystem->OnAssetOpenedInEditor().Remove(GAssetOpenedHandle);
			}
		}
		GAssetOpenedHandle.Reset();

		for (const FInstalledExtender& Installed : GInstalled)
		{
			if (Installed.Manager.IsValid() && Installed.Extender.IsValid())
			{
				Installed.Manager->RemoveExtender(Installed.Extender);
			}
		}
		GInstalled.Reset();
	}
}

#undef LOCTEXT_NAMESPACE
