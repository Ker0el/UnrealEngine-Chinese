// Copyright 2026 StarGame. All Rights Reserved.

#include "StarryToolbarButton.h"

#include "StarryDictionary.h"
#include "StarryHooks.h"
#include "StarryUI.h"
#include "StarryUEZh.h"

#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/AppStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "StarryToolbarButton"

void SStarryToolbarButton::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SButton)
		.ToolTipText(LOCTEXT("Tooltip", "左键切换中文 / 英文，右键打开词条菜单"))
		.OnClicked(FOnClicked::CreateLambda([]()
		{
			FStarryUI::Toggle();
			return FReply::Handled();
		}))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.Image(FAppStyle::Get().GetBrush(TEXT("Icons.Star")))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SStarryToolbarButton::GetLabel)
			]
		]
	];
}

FReply SStarryToolbarButton::OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		ShowMenu();
		return FReply::Handled();
	}
	return FReply::Unhandled();
}

FText SStarryToolbarButton::GetLabel() const
{
	return FStarryUI::GetButtonLabel();
}

void SStarryToolbarButton::ShowMenu()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Reload", "重新加载词条"),
		LOCTEXT("ReloadTip", "重新读取 Content/Dictionary，不用重启编辑器"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")),
		FUIAction(FExecuteAction::CreateStatic(&FStarryUI::ReloadDictionary)));

	MenuBuilder.AddMenuEntry(
		LOCTEXT("Harvest", "扫描词条"),
		LOCTEXT("HarvestTip", "扫描材质、函数、枚举和属性，结果写到 Saved/StarryUEZh/harvest.tsv"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Refresh")),
		FUIAction(FExecuteAction::CreateStatic(&StarryHooks::HarvestStatic)));

	MenuBuilder.AddMenuSeparator();

	MenuBuilder.AddMenuEntry(
		LOCTEXT("OpenPanel", "打开汉化面板"),
		LOCTEXT("OpenPanelTip", "打开星空 UE 汉化面板"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Settings")),
		FUIAction(FExecuteAction::CreateLambda([]()
		{
			FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("StarryUEZh")));
		})));

	const TSharedRef<SWidget> MenuContent = MenuBuilder.MakeWidget();
	FSlateApplication::Get().PushMenu(
		AsShared(),
		FWidgetPath(),
		MenuContent,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
}

#undef LOCTEXT_NAMESPACE
