// Copyright 2026 StarGame. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * English source string -> Chinese display string.
 *
 * The table is loaded from the plugin's Content/Dictionary folder. It is only
 * ever used to build transient display strings and search keywords; no FText,
 * asset, FName or serialized property is modified.
 */
class FStarryDictionary
{
public:
	static FStarryDictionary& Get();

	/** Load all dictionary files. Returns false when the main table is missing. */
	bool Load();

	/** Re-read all dictionary files without recreating the asset-name guard. */
	bool Reload();

	/** Write the current source->translation table as JSON. */
	bool Export(const FString& Path) const;

	bool IsEnabled() const { return bEnabled.load(std::memory_order_relaxed); }
	void SetEnabled(bool bInEnabled) { bEnabled.store(bInEnabled, std::memory_order_relaxed); }

	/** Exact, whole-string lookup. English -> Chinese. */
	bool Translate(const FString& Source, FString& OutTranslation) const;

	/** Chinese -> English lookup, used when the display language is English. */
	bool TranslateReverse(const FString& Source, FString& OutTranslation) const;

	/** Direction-aware lookup used by the display hooks. */
	bool TranslateDisplay(const FString& Source, FString& OutTranslation) const;

	/** Line-by-line lookup; leading/trailing whitespace of each line is preserved. */
	bool TranslateMultiline(const FString& Source, FString& OutTranslation) const;

	/** Direction-aware line-by-line lookup used by the display hooks. */
	bool TranslateMultilineDisplay(const FString& Source, FString& OutTranslation) const;

	int32 Num() const { return Entries.Num(); }

	/** True when the string is an asset name and should not be machine-translated. */
	bool IsProtectedName(const FString& Text) const { return IsAssetName(Text); }

	/** Terms that must stay in English (brand / API names). */
	bool IsNoTranslate(const FString& Text) const { return NoTranslate.Contains(Text); }
	const TSet<FString>& GetNoTranslateTerms() const { return NoTranslate; }

	/** Curated fixed translations, also injected into the AI prompt. */
	const TMap<FString, FString>& GetGlossary() const { return Glossary; }

	static bool ContainsNonAscii(const FString& Text);

	/** Remove spaces/underscores/hyphens and lower-case, for "TextureCoordinate" vs "Texture Coordinate". */
	static FString NormalizeKey(const FString& Text);

private:
	bool LoadDictionaries();
	bool LoadFile(const FString& Path, bool bOverride, int32& OutAdded);
	void BuildCompactMap();
	void BuildReverseMap();
	void BuildAssetNameGuard();
	void LoadAssetNames(class IAssetRegistry& AssetRegistry);
	bool IsAssetName(const FString& Text) const;
	bool TranslateComposed(const FString& Source, FString& OutTranslation) const;
	bool TranslateWords(const FString& Source, FString& OutTranslation) const;
	bool TranslateMultilineWith(const FString& Source, FString& OutTranslation, bool bChineseMode) const;
	bool LoadNoTranslate(const FString& Path);
	bool LoadGlossary(const FString& Path);

	TMap<FString, FString> Entries;
	TMap<FString, FString> CompactEntries;
	TMap<FString, FString> ReverseEntries;
	TMap<FString, FString> Glossary;
	TSet<FString> AssetNames;
	TSet<FString> NoTranslate;
	mutable FCriticalSection AssetNameLock;
	std::atomic<bool> bEnabled{ true };
};
