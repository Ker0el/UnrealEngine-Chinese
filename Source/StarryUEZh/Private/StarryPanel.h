// Copyright 2026 StarGame. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "Widgets/SCompoundWidget.h"

class SDockTab;
class FSpawnTabArgs;
class ITableRow;
class STableViewBase;

/** One row in the scan results list. */
struct FStarryScanRow
{
	FString Category;
	FString Source;
	FString Translation;
	FString State;
};

class SStarryUEZhPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStarryUEZhPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	FReply OnToggle();
	FReply OnHarvest();
	FReply OnCollectGaps();
	FReply OnExportUntranslated();
	FReply OnOpenDictionary();
	FReply OnReloadDictionary();
	FReply OnSaveManualEntry();
	FReply OnDeleteManualEntry();
	FReply OnStartAITranslate();
	FReply OnStopAITranslate();
	FReply OnFetchModels();
	void OnAIKeyChanged(const FText& NewText);
	void OnAIKeyCommitted(const FText& NewText, ETextCommit::Type CommitType);

	FText GetModeText() const;
	FText GetStatsText() const;
	FText GetScanStatsText() const;
	FText GetAIStatusText() const;

	void LoadScanResults();
	void RefreshScanList();
	void OnScanSearchChanged(const FText& NewText);
	void OnOnlyUntranslatedChanged(ECheckBoxState NewState);
	void OnScanSelectionChanged(TSharedPtr<FStarryScanRow> Item, ESelectInfo::Type SelectInfo);
	void OnAIModelChanged(TSharedPtr<FString> Item, ESelectInfo::Type SelectInfo);
	void BeginAITranslation();
	void StartNextAIBatch();
	void FinishAITranslation(bool bSuccess, const FString& Error);
	TSharedRef<ITableRow> OnGenerateScanRow(TSharedPtr<FStarryScanRow> Item, const TSharedRef<STableViewBase>& OwnerTable);

	bool WriteManualEntry(const FString& Source, const FString& Translation);
	bool RemoveManualEntry(const FString& Source);
	bool SaveAITranslations();

	TArray<TSharedPtr<FStarryScanRow>> AllScanRows;
	TArray<TSharedPtr<FStarryScanRow>> VisibleScanRows;
	TSharedPtr<class SListView<TSharedPtr<FStarryScanRow>>> ScanListView;
	TSharedPtr<class SEditableTextBox> ScanSourceBox;
	TSharedPtr<class SEditableTextBox> ScanTranslationBox;
	FString ScanSearchText;
	bool bOnlyUntranslated = false;
	int32 ScanTranslatedCount = 0;

	// AI translation
	TSharedPtr<class SEditableTextBox> AIKeyBox;
	int32 AIKeyFetchGeneration = 0;
	TArray<TSharedPtr<FString>> AIModelOptions;
	TSharedPtr<class SComboBox<TSharedPtr<FString>>> AIModelCombo;
	FString SelectedModel;
	TArray<FString> AIPendingSources;
	TMap<FString, FString> AIResults;
	int32 AITotal = 0;
	int32 AIDone = 0;
	int32 AISkipped = 0;
	int32 AIRetry = 0;
	int32 AIFailed = 0;
	bool bAIRunning = false;
	bool bAICancel = false;
	FString AIStatus;
	FString AIRequestUrl;
	FString AIRequestKey;
	FString AIRequestModel;
};

namespace StarryPanel
{
	void Register();
	void Unregister();
	TSharedRef<SDockTab> Spawn(const FSpawnTabArgs& Args);
}
