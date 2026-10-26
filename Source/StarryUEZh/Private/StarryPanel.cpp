// Copyright 2026 StarGame. All Rights Reserved.

#include "StarryPanel.h"

#include "StarryAI.h"
#include "StarryCredential.h"
#include "StarryDictionary.h"
#include "StarryHooks.h"
#include "StarryUI.h"
#include "StarryUEZh.h"

#include "Dom/JsonObject.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Styling/AppStyle.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

#define LOCTEXT_NAMESPACE "StarryUEZhPanel"

namespace
{
	void ShowNotification(const FText& Text)
	{
		FNotificationInfo Info(Text);
		Info.ExpireDuration = 5.0f;
		FSlateNotificationManager::Get().AddNotification(Info);
	}

	FString HarvestPath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("StarryUEZh"), TEXT("harvest.tsv"));
	}

	FString GapsPath()
	{
		return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("StarryUEZh"), TEXT("gaps.tsv"));
	}

	FString DictionaryDirectory()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("StarryUEZh"));
		return Plugin.IsValid() ? FPaths::Combine(Plugin->GetContentDir(), TEXT("Dictionary")) : FString();
	}

	FString ManualDictionaryPath()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("StarryUEZh"));
		return Plugin.IsValid()
			? FPaths::Combine(Plugin->GetContentDir(), TEXT("Dictionary"), TEXT("manual_zh.json"))
			: FString();
	}

	FString AIDictionaryPath()
	{
		const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("StarryUEZh"));
		return Plugin.IsValid()
			? FPaths::Combine(Plugin->GetContentDir(), TEXT("Dictionary"), TEXT("ai_zh.json"))
			: FString();
	}

	bool IsSafeAITranslation(const FString& Source, const FString& Translation)
	{
		auto CountChar = [](const FString& Text, TCHAR Character)
		{
			int32 Count = 0;
			for (const TCHAR C : Text)
			{
				Count += (C == Character) ? 1 : 0;
			}
			return Count;
		};
		if (CountChar(Source, TEXT('{')) != CountChar(Translation, TEXT('{'))) return false;
		if (CountChar(Source, TEXT('}')) != CountChar(Translation, TEXT('}'))) return false;
		if (CountChar(Source, TEXT('%')) != CountChar(Translation, TEXT('%'))) return false;
		if (CountChar(Source, TEXT('<')) != CountChar(Translation, TEXT('<'))) return false;
		if (CountChar(Source, TEXT('>')) != CountChar(Translation, TEXT('>'))) return false;
		const bool bLeadS = !Source.IsEmpty() && FChar::IsWhitespace(Source[0]);
		const bool bLeadT = !Translation.IsEmpty() && FChar::IsWhitespace(Translation[0]);
		const bool bTailS = !Source.IsEmpty() && FChar::IsWhitespace(Source[Source.Len() - 1]);
		const bool bTailT = !Translation.IsEmpty() && FChar::IsWhitespace(Translation[Translation.Len() - 1]);
		if (bLeadS != bLeadT || bTailS != bTailT)
		{
			return false;
		}
		for (const FString& Term : FStarryDictionary::Get().GetNoTranslateTerms())
		{
			if (Source.Contains(Term) && !Translation.Contains(Term))
			{
				return false;
			}
		}
		return true;
	}

	const TCHAR* AIConfigSection = TEXT("StarryUEZh");
}

void SStarryUEZhPanel::Construct(const FArguments& InArgs)
{
	FString SavedKey;
	FStarryCredential::LoadApiKey(SavedKey);

	// Always offer a usable model list, even before the API answers.
	AIModelOptions.Add(MakeShared<FString>(TEXT("deepseek-flash")));
	AIModelOptions.Add(MakeShared<FString>(TEXT("deepseek-v4-pro")));
	SelectedModel = TEXT("deepseek-flash");

	ChildSlot
	[
		SNew(SBorder)
		.Padding(12.0f)
		.BorderImage(FAppStyle::Get().GetBrush(TEXT("ToolPanel.GroupBorder")))
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("PanelTitle", "星空汉化"))
				.Font(FAppStyle::Get().GetFontStyle(TEXT("HeadingSmall")))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(this, &SStarryUEZhPanel::GetModeText)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(STextBlock)
				.Text(this, &SStarryUEZhPanel::GetStatsText)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush(TEXT("ToolPanel.GroupBorder")))
				.Padding(6.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("UsageTitle", "使用步骤"))
						.Font(FAppStyle::Get().GetFontStyle(TEXT("SmallFont")))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 3.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("UsageSteps",
							"1. 点「扫描词条」，等它扫完。这一步不用打开窗口。\n"
							"2. 打开你想汉化的窗口，把折叠的菜单和下拉框展开一次。\n"
							"3. 回到面板，点「收集未翻译文字」，新出现的词会列在下面。\n"
							"4. 选中一条，填中文，点「保存词条」，马上生效。\n"
							"5. 不想手动翻，就用上面的 AI：填 Key、选模型、点开始翻译。"))
						.AutoWrapText(true)
						.ColorAndOpacity(FLinearColor(0.72f, 0.72f, 0.72f))
					]
				]
			]

			// AI translation controls live at the top, right under the status.
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush(TEXT("ToolPanel.GroupBorder")))
				.Padding(6.0f)
				[
					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("AITitle", "AI 自动翻译（DeepSeek）"))
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Text(LOCTEXT("AIHint", "粘贴 API Key 后会自动获取模型，选好模型后点开始翻译。"))
							.ColorAndOpacity(FLinearColor(0.6f, 0.6f, 0.6f))
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(6.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SHyperlink)
							.Text(LOCTEXT("AIGetKey", "去申请"))
							.OnNavigate(FSimpleDelegate::CreateLambda([]()
							{
								FPlatformProcess::LaunchURL(TEXT("https://platform.deepseek.com/api_keys"), nullptr, nullptr);
							}))
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SAssignNew(AIKeyBox, SEditableTextBox)
							.HintText(LOCTEXT("AIKeyHint", "粘贴 API Key 后会自动获取模型"))
							.IsPassword(true)
							.Text(FText::FromString(SavedKey))
							.OnTextChanged(this, &SStarryUEZhPanel::OnAIKeyChanged)
							.OnTextCommitted(this, &SStarryUEZhPanel::OnAIKeyCommitted)
						]

					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SAssignNew(AIModelCombo, SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&AIModelOptions)
							.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item)
							{
								return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? *Item : FString()));
							})
							.OnSelectionChanged(this, &SStarryUEZhPanel::OnAIModelChanged)
							[
								SNew(STextBlock)
								.Text_Lambda([this]()
								{
									return FText::FromString(SelectedModel.IsEmpty() ? TEXT("先点获取模型") : SelectedModel);
								})
							]
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(6.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("AIStart", "开始翻译"))
.ContentPadding(FMargin(16.0f, 8.0f))
							.OnClicked(this, &SStarryUEZhPanel::OnStartAITranslate)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(6.0f, 0.0f, 0.0f, 0.0f)
						[
							SNew(SButton)
							.Text(LOCTEXT("AIStop", "停止"))
.ContentPadding(FMargin(16.0f, 8.0f))
							.OnClicked(this, &SStarryUEZhPanel::OnStopAITranslate)
						]
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(this, &SStarryUEZhPanel::GetAIStatusText)
						.AutoWrapText(true)
					]

					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 4.0f, 0.0f, 0.0f)
					[
						SNew(SProgressBar)
						.Percent_Lambda([this]()
						{
							return AITotal > 0 ? static_cast<float>(AIDone) / static_cast<float>(AITotal) : 0.0f;
						})
						.Visibility_Lambda([this]()
						{
							return bAIRunning ? EVisibility::Visible : EVisibility::Collapsed;
						})
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ToggleButton", "切换中 / 英"))
.ContentPadding(FMargin(16.0f, 8.0f))
					.ToolTipText(LOCTEXT("ToggleTooltip", "相当于 F1"))
					.OnClicked(this, &SStarryUEZhPanel::OnToggle)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("HarvestButton", "扫描词条"))
.ContentPadding(FMargin(16.0f, 8.0f))
					.ToolTipText(LOCTEXT("HarvestTooltip", "扫描节点、函数、枚举、属性和说明文字"))
					.OnClicked(this, &SStarryUEZhPanel::OnHarvest)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("CollectButton", "收集未翻译文字"))
.ContentPadding(FMargin(16.0f, 8.0f))
					.ToolTipText(LOCTEXT("CollectTooltip", "把刚才在窗口里显示过的未翻译文字收进列表"))
					.OnClicked(this, &SStarryUEZhPanel::OnCollectGaps)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("ExportUntranslated", "导出未翻译"))
					.ToolTipText(LOCTEXT("ExportUntranslatedTip", "把当前未翻译词条导出为 JSON 模板"))
.ContentPadding(FMargin(16.0f, 8.0f))
					.OnClicked(this, &SStarryUEZhPanel::OnExportUntranslated)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 6.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("OpenDictButton", "打开词库目录"))
.ContentPadding(FMargin(16.0f, 8.0f))
					.OnClicked(this, &SStarryUEZhPanel::OnOpenDictionary)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.Text(LOCTEXT("ReloadButton", "重新加载词条"))
.ContentPadding(FMargin(16.0f, 8.0f))
					.ToolTipText(LOCTEXT("ReloadTooltip", "重新读取 Content/Dictionary 下的 JSON，不用重启编辑器"))
					.OnClicked(this, &SStarryUEZhPanel::OnReloadDictionary)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(this, &SStarryUEZhPanel::GetScanStatsText)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SSearchBox)
					.HintText(LOCTEXT("ScanSearchHint", "搜索原文或译文"))
					.OnTextChanged(this, &SStarryUEZhPanel::OnScanSearchChanged)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SCheckBox)
					.OnCheckStateChanged(this, &SStarryUEZhPanel::OnOnlyUntranslatedChanged)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("OnlyUntranslated", "只看未翻译"))
					]
				]
			]

			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::Get().GetBrush(TEXT("ToolPanel.GroupBorder")))
				.Padding(4.0f)
				[
					SAssignNew(ScanListView, SListView<TSharedPtr<FStarryScanRow>>)
					.ListItemsSource(&VisibleScanRows)
					.OnGenerateRow(this, &SStarryUEZhPanel::OnGenerateScanRow)
					.OnSelectionChanged(this, &SStarryUEZhPanel::OnScanSelectionChanged)
					.SelectionMode(ESelectionMode::Single)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 6.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("EditHint", "选中上面一行会自动填入原文。填好译文后点「保存词条」，立即生效并写入 manual_zh.json。「导出未翻译」会生成 JSON 模板，可以直接交给 AI 填译文，填好放回 Content/Dictionary 后点重新加载词条。"))
				.AutoWrapText(true)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 4.0f, 0.0f, 0.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(0.45f)
				[
					SAssignNew(ScanSourceBox, SEditableTextBox)
					.HintText(LOCTEXT("SourceHint", "原文（英文）"))
				]

				+ SHorizontalBox::Slot()
				.FillWidth(0.45f)
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SAssignNew(ScanTranslationBox, SEditableTextBox)
					.HintText(LOCTEXT("TranslationHint", "译文（中文）"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("SaveEntry", "保存词条"))
.ContentPadding(FMargin(16.0f, 8.0f))
					.OnClicked(this, &SStarryUEZhPanel::OnSaveManualEntry)
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(6.0f, 0.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("DeleteEntry", "删除词条"))
.ContentPadding(FMargin(16.0f, 8.0f))
					.OnClicked(this, &SStarryUEZhPanel::OnDeleteManualEntry)
				]
			]
		]
	];

	LoadScanResults();

	if (SavedKey.StartsWith(TEXT("sk-")) && SavedKey.Len() >= 30)
	{
		TWeakPtr<SStarryUEZhPanel> LocalWeakThis = SharedThis(this);
		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
			[LocalWeakThis](float)
			{
				TSharedPtr<SStarryUEZhPanel> This = LocalWeakThis.Pin();
				if (This.IsValid())
				{
					This->OnFetchModels();
				}
				return false;
			}), 1.0f);
	}
}

FText SStarryUEZhPanel::GetModeText() const
{
	return FStarryDictionary::Get().IsEnabled()
		? LOCTEXT("ModeChinese", "当前显示：中文")
		: LOCTEXT("ModeEnglish", "当前显示：English");
}

FText SStarryUEZhPanel::GetStatsText() const
{
	return FText::FromString(FString::Printf(
		TEXT("词条：%d    Niagara：%s"),
		FStarryDictionary::Get().Num(),
		StarryHooks::HasNiagaraHooks() ? TEXT("已挂载") : TEXT("未挂载")));
}

FReply SStarryUEZhPanel::OnToggle()
{
	FStarryUI::Toggle();
	return FReply::Handled();
}

FReply SStarryUEZhPanel::OnHarvest()
{
	ShowNotification(LOCTEXT("HarvestStart", "正在扫描词条，请稍候…"));
	StarryHooks::HarvestStatic();
	LoadScanResults();
	ShowNotification(FText::FromString(FString::Printf(
		TEXT("扫描完成：共 %d 条，已翻译 %d，未翻译 %d"),
		AllScanRows.Num(), ScanTranslatedCount, AllScanRows.Num() - ScanTranslatedCount)));
	return FReply::Handled();
}

FReply SStarryUEZhPanel::OnCollectGaps()
{
	StarryHooks::DumpGaps();
	LoadScanResults();
	ShowNotification(LOCTEXT("CollectDone", "已收集未翻译文字，列表已刷新"));
	return FReply::Handled();
}

FReply SStarryUEZhPanel::OnExportUntranslated()
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Entries = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> Categories = MakeShared<FJsonObject>();
	int32 Count = 0;
	for (const TSharedPtr<FStarryScanRow>& Row : AllScanRows)
	{
		if (!Row.IsValid() || Row->State != TEXT("未翻译"))
		{
			continue;
		}
		Entries->SetStringField(Row->Source, FString());
		Categories->SetStringField(Row->Source, Row->Category);
		++Count;
	}
	Root->SetObjectField(TEXT("entries"), Entries);
	Root->SetObjectField(TEXT("categories"), Categories);
	Root->SetNumberField(TEXT("count"), Count);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		ShowNotification(LOCTEXT("ExportUntranslatedFailed", "未翻译词条导出失败"));
		return FReply::Handled();
	}

	const FString Directory = DictionaryDirectory();
	if (Directory.IsEmpty())
	{
		ShowNotification(LOCTEXT("ExportUntranslatedFailed", "找不到词库目录"));
		return FReply::Handled();
	}
	IFileManager::Get().MakeDirectory(*Directory, true);
	const FString Path = FPaths::Combine(Directory, TEXT("待翻译.json"));
	if (!FFileHelper::SaveStringToFile(Output, *Path))
	{
		ShowNotification(LOCTEXT("ExportUntranslatedFailed", "未翻译词条导出失败"));
		return FReply::Handled();
	}
	ShowNotification(FText::FromString(FString::Printf(TEXT("未翻译词条已导出：%s（%d 条）"), *Path, Count)));
	UE_LOG(LogStarryUEZh, Log, TEXT("未翻译词条已导出：%s（%d 条）"), *Path, Count);
	return FReply::Handled();
}

FReply SStarryUEZhPanel::OnOpenDictionary()
{
	FStarryUI::OpenDictionaryFolder();
	return FReply::Handled();
}

FReply SStarryUEZhPanel::OnReloadDictionary()
{
	FStarryUI::ReloadDictionary();
	ShowNotification(FText::FromString(FString::Printf(TEXT("词条已重新加载：%d 条"), FStarryDictionary::Get().Num())));
	LoadScanResults();
	return FReply::Handled();
}

void SStarryUEZhPanel::LoadScanResults()
{
	AllScanRows.Reset();
	ScanTranslatedCount = 0;

	TSet<FString> Seen;
	auto AddRow = [this, &Seen](const FString& Category, const FString& Source)
	{
		if (Source.IsEmpty() || Seen.Contains(Source))
		{
			return;
		}
		Seen.Add(Source);

		TSharedPtr<FStarryScanRow> Row = MakeShared<FStarryScanRow>();
		Row->Category = Category;
		Row->Source = Source;

		FString Translation;
		if (FStarryDictionary::ContainsNonAscii(Source))
		{
			Row->State = TEXT("已是中文");
			Row->Translation = Source;
			++ScanTranslatedCount;
		}
		else if (FStarryDictionary::Get().Translate(Source, Translation))
		{
			Row->State = TEXT("已翻译");
			Row->Translation = Translation;
			++ScanTranslatedCount;
		}
		else
		{
			Row->State = TEXT("未翻译");
			Row->Translation = TEXT("（未翻译）");
		}
		AllScanRows.Add(Row);
	};

	FString Content;
	if (FFileHelper::LoadFileToString(Content, *HarvestPath()))
	{
		TArray<FString> Lines;
		Content.ParseIntoArrayLines(Lines, false);
		for (const FString& Line : Lines)
		{
			if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
			{
				continue;
			}
			FString Category;
			FString Source;
			if (Line.Split(TEXT("\t"), &Category, &Source))
			{
				AddRow(Category, Source);
			}
		}
	}

	// Strings that were actually rendered but missed the dictionary.
	FString Gaps;
	if (FFileHelper::LoadFileToString(Gaps, *GapsPath()))
	{
		TArray<FString> Lines;
		Gaps.ParseIntoArrayLines(Lines, false);
		for (const FString& Line : Lines)
		{
			if (Line.IsEmpty() || Line.StartsWith(TEXT("#")))
			{
				continue;
			}
			FString Count;
			FString Rest;
			if (Line.Split(TEXT("\t"), &Count, &Rest))
			{
				FString Source = Rest;
				FString Window;
				FString Category = TEXT("运行时缺口");
				if (Rest.Split(TEXT("\t"), &Source, &Window) && !Window.IsEmpty())
				{
					Category += TEXT(" @ ") + Window;
				}
				// Older builds recorded the panel's own category labels and text it
				// drew; skip those so stale gap files do not flood the list.
				if (Window.Contains(TEXT("星空汉化"))
					|| Source.StartsWith(TEXT("UEnum:"))
					|| Source.StartsWith(TEXT("UClass:"))
					|| Source.StartsWith(TEXT("UFunctionTooltip:"))
					|| Source.StartsWith(TEXT("PropertyTooltip:"))
					|| Source.StartsWith(TEXT("NiagaraDescription:"))
					|| Source.StartsWith(TEXT("MaterialDescription:"))
					|| Source.StartsWith(TEXT("PluginDescription:"))
					|| Source.StartsWith(TEXT("AssetDescription:")))
				{
					continue;
				}
				AddRow(Category, Source);
			}
		}
	}

	RefreshScanList();
}

void SStarryUEZhPanel::RefreshScanList()
{
	VisibleScanRows.Reset();
	const bool bWantsText = !ScanSearchText.IsEmpty();
	for (const TSharedPtr<FStarryScanRow>& Row : AllScanRows)
	{
		if (bOnlyUntranslated && !bAIRunning && Row->State != TEXT("未翻译"))
		{
			continue;
		}
		if (bWantsText
			&& !Row->Source.Contains(ScanSearchText, ESearchCase::IgnoreCase)
			&& !Row->Translation.Contains(ScanSearchText, ESearchCase::IgnoreCase))
		{
			continue;
		}
		VisibleScanRows.Add(Row);
	}

	if (ScanListView.IsValid())
	{
		ScanListView->RequestListRefresh();
	}
}

void SStarryUEZhPanel::OnScanSearchChanged(const FText& NewText)
{
	ScanSearchText = NewText.ToString();
	RefreshScanList();
}

void SStarryUEZhPanel::OnOnlyUntranslatedChanged(ECheckBoxState NewState)
{
	bOnlyUntranslated = NewState == ECheckBoxState::Checked;
	RefreshScanList();
}

TSharedRef<ITableRow> SStarryUEZhPanel::OnGenerateScanRow(
	TSharedPtr<FStarryScanRow> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	const bool bTranslated = Item->State != TEXT("未翻译");
	const FLinearColor TranslationColor = bTranslated
		? FLinearColor(0.78f, 0.78f, 0.78f)
		: FLinearColor(1.0f, 0.58f, 0.22f);
	// Keep the source column in English: the leading zero-width space makes
	// the display hook treat this text as user content and skip translation.
	const FString DisplaySource = FString::Chr(0x200B) + Item->Source;

	return SNew(STableRow<TSharedPtr<FStarryScanRow>>, OwnerTable)
		.Padding(FMargin(2.0f, 1.0f))
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Category))
				.MinDesiredWidth(130.0f)
				.ColorAndOpacity(FLinearColor(0.55f, 0.55f, 0.55f))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.45f)
			.Padding(6.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(DisplaySource))
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.45f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Item->Translation))
				.ColorAndOpacity(TranslationColor)
			]
		];
}

FText SStarryUEZhPanel::GetScanStatsText() const
{
	return FText::FromString(FString::Printf(
		TEXT("扫描结果：%d 条，已翻译 %d，未翻译 %d，当前显示 %d 条"),
		AllScanRows.Num(), ScanTranslatedCount, AllScanRows.Num() - ScanTranslatedCount,
		VisibleScanRows.Num()));
}

void SStarryUEZhPanel::OnScanSelectionChanged(TSharedPtr<FStarryScanRow> Item, ESelectInfo::Type SelectInfo)
{
	if (!Item.IsValid())
	{
		return;
	}
	if (ScanSourceBox.IsValid())
	{
		ScanSourceBox->SetText(FText::FromString(FString::Chr(0x200B) + Item->Source));
	}
	if (ScanTranslationBox.IsValid())
	{
		ScanTranslationBox->SetText(Item->State == TEXT("未翻译")
			? FText::GetEmpty()
			: FText::FromString(Item->Translation));
	}
}

bool SStarryUEZhPanel::WriteManualEntry(const FString& Source, const FString& Translation)
{
	const FString Path = ManualDictionaryPath();
	if (Path.IsEmpty())
	{
		return false;
	}

	FString Json;
	FFileHelper::LoadFileToString(Json, *Path);
	TSharedPtr<FJsonObject> Root;
	if (!Json.IsEmpty())
	{
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Root);
	}
	if (!Root.IsValid())
	{
		Root = MakeShared<FJsonObject>();
	}

	const TSharedPtr<FJsonObject>* Existing = nullptr;
	TSharedPtr<FJsonObject> Entries;
	if (Root->TryGetObjectField(TEXT("entries"), Existing) && Existing)
	{
		Entries = *Existing;
	}
	else
	{
		Entries = MakeShared<FJsonObject>();
		Root->SetObjectField(TEXT("entries"), Entries);
	}
	Entries->SetStringField(Source, Translation);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		return false;
	}
	return FFileHelper::SaveStringToFile(Output, *Path);
}

bool SStarryUEZhPanel::RemoveManualEntry(const FString& Source)
{
	const FString Path = ManualDictionaryPath();
	if (Path.IsEmpty())
	{
		return false;
	}

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path) || Json.IsEmpty())
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TSharedPtr<FJsonObject>* Existing = nullptr;
	if (!Root->TryGetObjectField(TEXT("entries"), Existing) || !Existing)
	{
		return false;
	}
	(*Existing)->RemoveField(Source);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		return false;
	}
	return FFileHelper::SaveStringToFile(Output, *Path);
}

FReply SStarryUEZhPanel::OnSaveManualEntry()
{
	if (!ScanSourceBox.IsValid() || !ScanTranslationBox.IsValid())
	{
		return FReply::Handled();
	}

	FString Source = ScanSourceBox->GetText().ToString().TrimStartAndEnd();
	Source.RemoveFromStart(FString::Chr(0x200B));
	const FString Translation = ScanTranslationBox->GetText().ToString().TrimStartAndEnd();
	if (Source.IsEmpty() || Translation.IsEmpty())
	{
		ShowNotification(LOCTEXT("EntryEmpty", "原文和译文都不能为空"));
		return FReply::Handled();
	}
	if (!FStarryDictionary::ContainsNonAscii(Translation))
	{
		ShowNotification(LOCTEXT("EntryNotChinese", "译文需要是中文"));
		return FReply::Handled();
	}

	if (WriteManualEntry(Source, Translation))
	{
		FStarryUI::ReloadDictionary();
		LoadScanResults();
		ShowNotification(FText::FromString(FString::Printf(
			TEXT("已保存并生效：%s → %s"), *Source, *Translation)));
	}
	else
	{
		ShowNotification(LOCTEXT("EntrySaveFailed", "保存失败，词库目录可能不可写"));
	}
	return FReply::Handled();
}

FReply SStarryUEZhPanel::OnDeleteManualEntry()
{
	if (!ScanSourceBox.IsValid())
	{
		return FReply::Handled();
	}

	FString Source = ScanSourceBox->GetText().ToString().TrimStartAndEnd();
	Source.RemoveFromStart(FString::Chr(0x200B));
	if (Source.IsEmpty())
	{
		ShowNotification(LOCTEXT("EntryNoSource", "先选中一行或填入原文"));
		return FReply::Handled();
	}

	if (RemoveManualEntry(Source))
	{
		FStarryUI::ReloadDictionary();
		LoadScanResults();
		ShowNotification(FText::FromString(FString::Printf(
			TEXT("已从 manual_zh.json 删除：%s"), *Source)));
	}
	else
	{
		ShowNotification(LOCTEXT("EntryDeleteFailed", "删除失败，词库目录可能不可写"));
	}
	return FReply::Handled();
}

FText SStarryUEZhPanel::GetAIStatusText() const
{
	return FText::FromString(AIStatus.IsEmpty() ? TEXT("就绪") : AIStatus);
}

FReply SStarryUEZhPanel::OnStartAITranslate()
{
	if (bAIRunning)
	{
		ShowNotification(LOCTEXT("AIBusy", "正在翻译中，请等待或点停止"));
		return FReply::Handled();
	}
	if (!AIKeyBox.IsValid())
	{
		return FReply::Handled();
	}

	AIRequestKey = AIKeyBox->GetText().ToString();
	AIRequestKey.ReplaceInline(TEXT("\r"), TEXT(""));
	AIRequestKey.ReplaceInline(TEXT("\n"), TEXT(""));
	AIRequestKey.TrimStartAndEndInline();
	AIRequestKey.RemoveFromStart(TEXT("Bearer "));
	if (AIRequestKey.IsEmpty())
	{
		ShowNotification(LOCTEXT("AINeedsKey", "先粘贴 DeepSeek API Key"));
		return FReply::Handled();
	}
	if (SelectedModel.IsEmpty())
	{
		ShowNotification(LOCTEXT("AIFetchingModels", "正在获取模型，稍等再点开始翻译"));
		OnFetchModels();
		return FReply::Handled();
	}

	FStarryCredential::SaveApiKey(AIRequestKey);
	GConfig->SetString(AIConfigSection, TEXT("AIKey"), TEXT(""), GEditorPerProjectIni);
	GConfig->SetString(AIConfigSection, TEXT("AIModel"), *SelectedModel, GEditorPerProjectIni);
	GConfig->SetString(AIConfigSection, TEXT("AIUrl"), TEXT("https://api.deepseek.com/v1/chat/completions"), GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);

	AIRequestUrl = TEXT("https://api.deepseek.com/v1/chat/completions");
	AIRequestModel = SelectedModel;
	bAIRunning = true;
	BeginAITranslation();
	return FReply::Handled();
}

FReply SStarryUEZhPanel::OnFetchModels()
{
	if (!AIKeyBox.IsValid())
	{
		return FReply::Handled();
	}

	FString Key = AIKeyBox->GetText().ToString();
	Key.ReplaceInline(TEXT("\r"), TEXT(""));
	Key.ReplaceInline(TEXT("\n"), TEXT(""));
	Key.TrimStartAndEndInline();
	Key.RemoveFromStart(TEXT("Bearer "));
	if (Key.IsEmpty())
	{
		ShowNotification(LOCTEXT("AINeedsKey", "先粘贴 DeepSeek API Key"));
		return FReply::Handled();
	}

	FStarryCredential::SaveApiKey(Key);
	GConfig->SetString(AIConfigSection, TEXT("AIKey"), TEXT(""), GEditorPerProjectIni);
	GConfig->Flush(false, GEditorPerProjectIni);

	AIStatus = TEXT("正在获取模型…");
	UE_LOG(LogStarryUEZh, Log, TEXT("请求 DeepSeek 模型列表"));
	TWeakPtr<SStarryUEZhPanel> LocalWeakThis = SharedThis(this);
	FStarryAI::FetchModels(Key, FOnStarryAIModels::CreateLambda(
		[LocalWeakThis](bool bOk, const FStarryAIModelList& Models, const FString& Error)
		{
			TSharedPtr<SStarryUEZhPanel> This = LocalWeakThis.Pin();
			if (!This.IsValid())
			{
				return;
			}
			if (!bOk)
			{
				UE_LOG(LogStarryUEZh, Warning, TEXT("获取模型失败：%s"), *Error);
				This->AIStatus = FString::Printf(TEXT("获取模型失败：%s"), *Error);
				ShowNotification(FText::FromString(This->AIStatus));
				return;
			}

			This->AIModelOptions.Reset();
			for (const FString& Id : Models)
			{
				This->AIModelOptions.Add(MakeShared<FString>(Id));
			}

			FString Pick = Models[0];
			for (const FString& Id : Models)
			{
				if (Id.Contains(TEXT("chat")))
				{
					Pick = Id;
					break;
				}
			}
			This->SelectedModel = Pick;

			if (This->AIModelCombo.IsValid())
			{
				This->AIModelCombo->RefreshOptions();
				for (const TSharedPtr<FString>& Option : This->AIModelOptions)
				{
					if (Option.IsValid() && *Option == Pick)
					{
						This->AIModelCombo->SetSelectedItem(Option);
						break;
					}
				}
			}

			UE_LOG(LogStarryUEZh, Log, TEXT("获取到 %d 个模型，当前：%s"), Models.Num(), *Pick);
			This->AIStatus = FString::Printf(TEXT("已获取 %d 个模型，当前：%s"), Models.Num(), *Pick);
		}));
	return FReply::Handled();
}

void SStarryUEZhPanel::OnAIModelChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo)
{
	if (Item.IsValid())
	{
		SelectedModel = *Item;
	}
}

void SStarryUEZhPanel::OnAIKeyChanged(const FText& NewText)
{
	const FString Key = NewText.ToString().TrimStartAndEnd();
	++AIKeyFetchGeneration;
	if (!Key.StartsWith(TEXT("sk-")) || Key.Len() < 30)
	{
		return;
	}

	UE_LOG(LogStarryUEZh, Log, TEXT("检测到 API Key（长度 %d），准备获取模型"), Key.Len());

	// Fill the dropdown immediately so the panel stays usable even if the
	// request is slow or blocked.
	if (AIModelOptions.Num() == 0)
	{
		AIModelOptions.Add(MakeShared<FString>(TEXT("deepseek-flash")));
		AIModelOptions.Add(MakeShared<FString>(TEXT("deepseek-v4-pro")));
		SelectedModel = TEXT("deepseek-flash");
		if (AIModelCombo.IsValid())
		{
			AIModelCombo->RefreshOptions();
			AIModelCombo->SetSelectedItem(AIModelOptions[0]);
		}
		AIStatus = TEXT("已填入默认模型，正在从接口刷新…");
	}

	const int32 Generation = AIKeyFetchGeneration;
	TWeakPtr<SStarryUEZhPanel> LocalWeakThis = SharedThis(this);
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda(
		[LocalWeakThis, Generation](float)
		{
			TSharedPtr<SStarryUEZhPanel> This = LocalWeakThis.Pin();
			if (!This.IsValid() || This->AIKeyFetchGeneration != Generation)
			{
				return false;
			}
			This->OnFetchModels();
			return false;
		}), 0.8f);
}

void SStarryUEZhPanel::OnAIKeyCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	const FString Key = NewText.ToString().TrimStartAndEnd();
	if (!Key.StartsWith(TEXT("sk-")) || Key.Len() < 30)
	{
		return;
	}
	++AIKeyFetchGeneration;
	OnFetchModels();
}

void SStarryUEZhPanel::BeginAITranslation()
{
	if (AllScanRows.Num() == 0)
	{
		StarryHooks::HarvestStatic();
		LoadScanResults();
	}

	TSet<FString> Seen;
	AIPendingSources.Reset();
	for (const TSharedPtr<FStarryScanRow>& Row : AllScanRows)
	{
		if (!Row.IsValid() || Row->State != TEXT("未翻译"))
		{
			continue;
		}
		const FString& Source = Row->Source;
		if (Source.Len() < 2 || Source.Len() > 400)
		{
			continue;
		}
		if (Source.Contains(TEXT("/")) || Source.Contains(TEXT("\\")))
		{
			continue;
		}
		if (Source.StartsWith(TEXT("b")) && Source.Len() > 1 && FChar::IsUpper(Source[1]))
		{
			continue;
		}
		if (FStarryDictionary::Get().IsProtectedName(Source)
			|| FStarryDictionary::Get().IsNoTranslate(Source))
		{
			continue;
		}
		if (Seen.Contains(Source))
		{
			continue;
		}
		Seen.Add(Source);
		AIPendingSources.Add(Source);
	}

	if (AIPendingSources.Num() == 0)
	{
		bAIRunning = false;
		AIStatus = TEXT("没有可翻译的词条");
		ShowNotification(LOCTEXT("AINothingToDo", "没有可翻译的未翻译词条"));
		return;
	}
	AIResults.Reset();
	AISkipped = 0;
	AIRetry = 0;
	AIFailed = 0;
	AITotal = AIPendingSources.Num();
	AIDone = 0;
	bAICancel = false;
	AIStatus = FString::Printf(TEXT("正在翻译 0/%d"), AITotal);
	StartNextAIBatch();
}

FReply SStarryUEZhPanel::OnStopAITranslate()
{
	if (bAIRunning)
	{
		bAICancel = true;
		AIStatus = TEXT("正在停止…");
	}
	return FReply::Handled();
}

void SStarryUEZhPanel::StartNextAIBatch()
{
	if (bAICancel)
	{
		FinishAITranslation(false, TEXT("已停止"));
		return;
	}
	if (AIPendingSources.Num() == 0)
	{
		FinishAITranslation(true, FString());
		return;
	}

	const int32 Count = FMath::Min(20, AIPendingSources.Num());
	TArray<FString> Batch;
	Batch.Reserve(Count);
	for (int32 Index = 0; Index < Count; ++Index)
	{
		Batch.Add(AIPendingSources[Index]);
	}
	AIPendingSources.RemoveAt(0, Count);
	AIStatus = FString::Printf(TEXT("正在翻译 %d/%d，等待接口返回…"), AIDone, AITotal);

	TWeakPtr<SStarryUEZhPanel> LocalWeakThis = SharedThis(this);
	FStarryAI::Translate(AIRequestUrl, AIRequestKey, AIRequestModel, Batch,
		FOnStarryAITranslated::CreateLambda(
			[LocalWeakThis, Count, Batch](bool bOk, const TMap<FString, FString>& Translations, const FString& Error)
			{
				TSharedPtr<SStarryUEZhPanel> This = LocalWeakThis.Pin();
				if (!This.IsValid() || !This->bAIRunning)
				{
					return;
				}
				if (!bOk)
				{
					if (This->bAICancel)
					{
						This->FinishAITranslation(false, TEXT("已停止"));
						return;
					}
					// Retry the same batch twice before giving up on it, so one
					// timeout or rate limit does not abort a long run.
					if (++This->AIRetry <= 2)
					{
						This->AIPendingSources.Insert(Batch, 0);
						This->AIStatus = FString::Printf(TEXT("接口失败，%d 秒后重试（第 %d 次）：%s"),
							2, This->AIRetry, *Error.Left(60));
						TWeakPtr<SStarryUEZhPanel> RetryWeak = LocalWeakThis;
						FTSTicker::GetCoreTicker().AddTicker(
							FTickerDelegate::CreateLambda([RetryWeak](float)
							{
								TSharedPtr<SStarryUEZhPanel> Panel = RetryWeak.Pin();
								if (!Panel.IsValid() || !Panel->bAIRunning)
								{
									return false;
								}
								if (Panel->bAICancel)
								{
									Panel->FinishAITranslation(false, TEXT("已停止"));
									return false;
								}
								Panel->StartNextAIBatch();
								return false;
							}), 2.0f);
						return;
					}
					This->AIRetry = 0;
					This->AIFailed += Count;
					This->AIDone += Count;
					This->AIStatus = FString::Printf(TEXT("本批失败，跳过 %d 条，继续下一批…"), Count);
					This->StartNextAIBatch();
					return;
				}
				This->AIRetry = 0;
				for (const TPair<FString, FString>& Pair : Translations)
				{
					if (!IsSafeAITranslation(Pair.Key, Pair.Value))
					{
						++This->AISkipped;
						continue;
					}
					This->AIResults.Add(Pair.Key, Pair.Value);
					for (const TSharedPtr<FStarryScanRow>& Row : This->AllScanRows)
					{
						if (Row.IsValid() && Row->State == TEXT("未翻译") && Row->Source == Pair.Key)
						{
							Row->Translation = Pair.Value;
							Row->State = TEXT("已翻译");
							++This->ScanTranslatedCount;
						}
					}
				}
				This->AIDone += Count;
				This->SaveAITranslations();
				This->RefreshScanList();
				This->AIStatus = FString::Printf(TEXT("正在翻译 %d/%d，本批返回 %d 条，格式跳过 %d 条"), This->AIDone, This->AITotal, Translations.Num(), This->AISkipped);
				This->StartNextAIBatch();
			}));
}

void SStarryUEZhPanel::FinishAITranslation(bool bSuccess, const FString& Error)
{
	bAIRunning = false;
	if (bAICancel)
	{
		bAICancel = false;
		AIStatus = TEXT("已停止");
		ShowNotification(FText::FromString(FString::Printf(TEXT("已停止，已翻译 %d 条"), AIResults.Num())));
		return;
	}

	if (AIResults.Num() > 0 && SaveAITranslations())
	{
		FStarryUI::ReloadDictionary();
		LoadScanResults();
	}

	if (bSuccess)
	{
		AIStatus = AIFailed > 0
			? FString::Printf(TEXT("完成：写入 %d 条，%d 条接口失败，%d 条格式跳过"), AIResults.Num(), AIFailed, AISkipped)
			: FString::Printf(TEXT("完成：写入 %d 条，格式跳过 %d 条"), AIResults.Num(), AISkipped);
		ShowNotification(FText::FromString(AIStatus));
	}
	else
	{
		UE_LOG(LogStarryUEZh, Warning, TEXT("AI 翻译失败：%s"), *Error);
		AIStatus = FString::Printf(TEXT("失败：%s（已保存 %d 条）"), *Error, AIResults.Num());
		ShowNotification(FText::FromString(AIStatus));
	}
}

bool SStarryUEZhPanel::SaveAITranslations()
{
	const FString Path = AIDictionaryPath();
	if (Path.IsEmpty() || AIResults.Num() == 0)
	{
		return false;
	}

	FString Json;
	FFileHelper::LoadFileToString(Json, *Path);
	TSharedPtr<FJsonObject> Root;
	if (!Json.IsEmpty())
	{
		const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Root);
	}
	if (!Root.IsValid())
	{
		Root = MakeShared<FJsonObject>();
	}

	const TSharedPtr<FJsonObject>* Existing = nullptr;
	TSharedPtr<FJsonObject> Entries;
	if (Root->TryGetObjectField(TEXT("entries"), Existing) && Existing)
	{
		Entries = *Existing;
	}
	else
	{
		Entries = MakeShared<FJsonObject>();
		Root->SetObjectField(TEXT("entries"), Entries);
	}

	for (const TPair<FString, FString>& Pair : AIResults)
	{
		Entries->SetStringField(Pair.Key, Pair.Value);
	}

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		return false;
	}
	return FFileHelper::SaveStringToFile(Output, *Path);
}

namespace StarryPanel
{
	static const FName TabId(TEXT("StarryUEZh"));

	void Register()
	{
		FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TabId, FOnSpawnTab::CreateStatic(&StarryPanel::Spawn))
			.SetDisplayName(LOCTEXT("TabTitle", "星空汉化"))
			.SetMenuType(ETabSpawnerMenuType::Hidden);
	}

	void Unregister()
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
	}

	TSharedRef<SDockTab> Spawn(const FSpawnTabArgs& Args)
	{
		return SNew(SDockTab)
			.TabRole(ETabRole::NomadTab)
			[
				SNew(SStarryUEZhPanel)
			];
	}
}

#undef LOCTEXT_NAMESPACE
