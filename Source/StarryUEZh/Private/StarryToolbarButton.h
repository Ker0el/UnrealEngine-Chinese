// Copyright 2026 StarGame. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SStarryToolbarButton : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SStarryToolbarButton) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FReply OnPreviewMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

private:
	void ShowMenu();
	FText GetLabel() const;
};
