// Copyright 2026 StarGame. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace StarryHooks
{
	/** Install every available display/search hook. Safe to call once. */
	bool Install();

	/** Remove all installed hooks. */
	void Uninstall();

	/** Ask all Slate text blocks to rebuild their layouts on the next pass. */
	void RequestRefresh();

	/** Whether the Niagara hooks were installed. */
	bool HasNiagaraHooks();

	/** Debug counters for the plain-text marshaller hook. */
	void GetSetTextStats(int64& OutCalls, int64& OutHits);

	/** Write all un-translated display strings seen so far to Saved/StarryUEZh/gaps.tsv. */
	void DumpGaps();

	/** Scan material expressions, UFunctions, enums and properties; write harvest.tsv. */
	void HarvestStatic();
}
