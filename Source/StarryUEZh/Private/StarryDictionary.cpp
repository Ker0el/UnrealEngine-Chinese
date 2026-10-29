// Copyright 2026 StarGame. All Rights Reserved.

#include "StarryDictionary.h"

#include "StarryUEZh.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FStarryDictionary& FStarryDictionary::Get()
{
	static FStarryDictionary Instance;
	return Instance;
}

bool FStarryDictionary::ContainsNonAscii(const FString& Text)
{
	for (const TCHAR C : Text)
	{
		if (C > 127)
		{
			return true;
		}
	}
	return false;
}

FString FStarryDictionary::NormalizeKey(const FString& Text)
{
	FString Result;
	Result.Reserve(Text.Len());
	for (const TCHAR C : Text)
	{
		if (C == TEXT(' ') || C == TEXT('_') || C == TEXT('-') || C == TEXT('.') || C == TEXT('/') || C == TEXT('\\'))
		{
			continue;
		}
		Result.AppendChar(FChar::ToLower(C));
	}
	return Result;
}

bool FStarryDictionary::Load()
{
	if (!LoadDictionaries())
	{
		return false;
	}
	BuildAssetNameGuard();
	return true;
}

bool FStarryDictionary::Reload()
{
	return LoadDictionaries();
}

bool FStarryDictionary::LoadDictionaries()
{
	Entries.Reset();
	Glossary.Reset();

	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("StarryUEZh"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogStarryUEZh, Error, TEXT("找不到 StarryUEZh 插件"));
		return false;
	}

	const FString DictionaryDir = FPaths::Combine(Plugin->GetContentDir(), TEXT("Dictionary"));
	const FString MainPath = FPaths::Combine(DictionaryDir, TEXT("zh_cn.json"));
	const FString ExtraPath = FPaths::Combine(DictionaryDir, TEXT("zh_cn_extra.json"));
	const FString MaterialPath = FPaths::Combine(DictionaryDir, TEXT("zh_cn_material.json"));
	const FString NoTranslatePath = FPaths::Combine(DictionaryDir, TEXT("notranslate.json"));
	const FString NiagaraPath = FPaths::Combine(DictionaryDir, TEXT("zh_cn_niagara.json"));
	const FString NiagaraEnginePath = FPaths::Combine(DictionaryDir, TEXT("zh_cn_niagara_engine.json"));
	const FString NiagaraParamsPath = FPaths::Combine(DictionaryDir, TEXT("zh_cn_niagara_params.json"));
	const FString MaterialFuncsPath = FPaths::Combine(DictionaryDir, TEXT("zh_cn_material_funcs.json"));
	const FString MaterialPinsPath = FPaths::Combine(DictionaryDir, TEXT("zh_cn_material_pins.json"));
	const FString BonesPath = FPaths::Combine(DictionaryDir, TEXT("zh_cn_bones.json"));
	const FString GlossaryPath = FPaths::Combine(DictionaryDir, TEXT("glossary.json"));
	const FString ManualPath = FPaths::Combine(DictionaryDir, TEXT("manual_zh.json"));

	int32 Added = 0;
	if (!LoadFile(MainPath, false, Added))
	{
		UE_LOG(LogStarryUEZh, Error, TEXT("词库加载失败：%s"), *MainPath);
		return false;
	}
	UE_LOG(LogStarryUEZh, Log, TEXT("主词库：%d 条（%s）"), Added, *MainPath);

	if (FPaths::FileExists(NoTranslatePath))
	{
		LoadNoTranslate(NoTranslatePath);
	}

	if (FPaths::FileExists(ExtraPath))
	{
		Added = 0;
		if (LoadFile(ExtraPath, false, Added))
		{
			UE_LOG(LogStarryUEZh, Log, TEXT("扩展词库：%d 条（%s）"), Added, *ExtraPath);
		}
	}

	if (FPaths::FileExists(MaterialPath))
	{
		Added = 0;
		if (LoadFile(MaterialPath, false, Added))
		{
			UE_LOG(LogStarryUEZh, Log, TEXT("材质补全词库：%d 条（%s）"), Added, *MaterialPath);
		}
	}

	if (FPaths::FileExists(NiagaraPath))
	{
		Added = 0;
		if (LoadFile(NiagaraPath, false, Added))
		{
			UE_LOG(LogStarryUEZh, Log, TEXT("Niagara 词库：%d 条（%s）"), Added, *NiagaraPath);
		}
	}

	if (FPaths::FileExists(NiagaraEnginePath))
	{
		Added = 0;
		if (LoadFile(NiagaraEnginePath, false, Added))
		{
			UE_LOG(LogStarryUEZh, Log, TEXT("Niagara 引擎属性词库：%d 条（%s）"), Added, *NiagaraEnginePath);
		}
	}

	if (FPaths::FileExists(NiagaraParamsPath))
	{
		Added = 0;
		if (LoadFile(NiagaraParamsPath, false, Added))
		{
			UE_LOG(LogStarryUEZh, Log, TEXT("Niagara 模块参数词库：%d 条（%s）"), Added, *NiagaraParamsPath);
		}
	}

	if (FPaths::FileExists(MaterialFuncsPath))
	{
		Added = 0;
		if (LoadFile(MaterialFuncsPath, false, Added))
		{
			UE_LOG(LogStarryUEZh, Log, TEXT("材质函数词库：%d 条（%s）"), Added, *MaterialFuncsPath);
		}
	}

	if (FPaths::FileExists(MaterialPinsPath))
	{
		Added = 0;
		if (LoadFile(MaterialPinsPath, false, Added))
		{
			UE_LOG(LogStarryUEZh, Log, TEXT("材质函数引脚词库：%d 条（%s）"), Added, *MaterialPinsPath);
		}
	}

	if (FPaths::FileExists(BonesPath))
	{
		Added = 0;
		if (LoadFile(BonesPath, false, Added))
		{
			UE_LOG(LogStarryUEZh, Log, TEXT("骨骼显示名词库：%d 条（%s）"), Added, *BonesPath);
		}
	}

	if (FPaths::FileExists(GlossaryPath))
	{
		if (LoadGlossary(GlossaryPath))
		{
			UE_LOG(LogStarryUEZh, Log, TEXT("固定译法词表：%d 条（%s）"), Glossary.Num(), *GlossaryPath);
		}
	}

	if (FPaths::FileExists(ManualPath))
	{
		Added = 0;
		if (LoadFile(ManualPath, true, Added))
		{
			UE_LOG(LogStarryUEZh, Log, TEXT("手工修正词条：%d 条（%s）"), Added, *ManualPath);
		}
	}

	// Any additional JSON dropped into Content/Dictionary is loaded automatically,
	// so new word tables do not require rebuilding the plugin.
	TSet<FString> KnownFiles;
	KnownFiles.Add(TEXT("zh_cn.json"));
	KnownFiles.Add(TEXT("notranslate.json"));
	KnownFiles.Add(TEXT("zh_cn_extra.json"));
	KnownFiles.Add(TEXT("zh_cn_material.json"));
	KnownFiles.Add(TEXT("zh_cn_niagara.json"));
	KnownFiles.Add(TEXT("zh_cn_niagara_engine.json"));
	KnownFiles.Add(TEXT("zh_cn_niagara_params.json"));
	KnownFiles.Add(TEXT("zh_cn_material_funcs.json"));
	KnownFiles.Add(TEXT("zh_cn_material_pins.json"));
	KnownFiles.Add(TEXT("zh_cn_bones.json"));
	KnownFiles.Add(TEXT("glossary.json"));
	KnownFiles.Add(TEXT("manual_zh.json"));

	TArray<FString> FoundFiles;
	IFileManager::Get().FindFiles(FoundFiles, *(DictionaryDir / TEXT("*.json")), true, false);
	for (const FString& File : FoundFiles)
	{
		if (KnownFiles.Contains(File))
		{
			continue;
		}
		Added = 0;
		const FString Path = FPaths::Combine(DictionaryDir, File);
		if (LoadFile(Path, false, Added))
		{
			UE_LOG(LogStarryUEZh, Log, TEXT("附加词库：%d 条（%s）"), Added, *Path);
		}
	}

	UE_LOG(LogStarryUEZh, Log, TEXT("词库就绪，共 %d 条"), Entries.Num());

	BuildCompactMap();
	BuildReverseMap();
	return true;
}

void FStarryDictionary::BuildCompactMap()
{
	CompactEntries.Reset();
	CompactEntries.Reserve(Entries.Num());
	for (const TPair<FString, FString>& Pair : Entries)
	{
		const FString Key = NormalizeKey(Pair.Key);
		if (Key.Len() >= 2)
		{
			const bool bCanonical = Pair.Key.Contains(TEXT(" "));
			if (FString* Existing = CompactEntries.Find(Key))
			{
				// Prefer the spaced canonical source ("Texture Coordinate") over
				// the class-style source ("TextureCoordinate").
				if (bCanonical)
				{
					*Existing = Pair.Value;
				}
			}
			else
			{
				CompactEntries.Add(Key, Pair.Value);
			}
		}
	}
	UE_LOG(LogStarryUEZh, Log, TEXT("紧凑索引：%d 条"), CompactEntries.Num());
}

void FStarryDictionary::BuildReverseMap()
{
	ReverseEntries.Reset();
	auto IsBetterEnglish = [](const FString& Candidate, const FString& Current)
	{
		const bool bCandidateSpaced = Candidate.Contains(TEXT(" "));
		const bool bCurrentSpaced = Current.Contains(TEXT(" "));
		if (bCandidateSpaced != bCurrentSpaced)
		{
			return bCandidateSpaced;
		}
		return Candidate.Len() < Current.Len();
	};

	for (const TPair<FString, FString>& Pair : Entries)
	{
		if (!ContainsNonAscii(Pair.Value) || Pair.Value.IsEmpty() || Pair.Key.IsEmpty())
		{
			continue;
		}
		if (FString* Existing = ReverseEntries.Find(Pair.Value))
		{
			if (IsBetterEnglish(Pair.Key, *Existing))
			{
				*Existing = Pair.Key;
			}
		}
		else
		{
			ReverseEntries.Add(Pair.Value, Pair.Key);
		}
	}
	UE_LOG(LogStarryUEZh, Log, TEXT("中文反查索引：%d 条"), ReverseEntries.Num());
}

void FStarryDictionary::LoadAssetNames(IAssetRegistry& AssetRegistry)
{
	TArray<FAssetData> Assets;
	AssetRegistry.GetAllAssets(Assets, false);
	TSet<FString> NewNames;
	NewNames.Reserve(Assets.Num());
	for (const FAssetData& Asset : Assets)
	{
		NewNames.Add(Asset.AssetName.ToString());
	}
	{
		FScopeLock Lock(&AssetNameLock);
		AssetNames = MoveTemp(NewNames);
		UE_LOG(LogStarryUEZh, Log, TEXT("资产名保护：%d 个名字不再翻译"), AssetNames.Num());
	}
}

bool FStarryDictionary::IsAssetName(const FString& Text) const
{
	FScopeLock Lock(&AssetNameLock);
	return AssetNames.Contains(Text);
}

bool FStarryDictionary::TranslateWords(const FString& Source, FString& OutTranslation) const
{
	TArray<FString> Words;
	Source.ParseIntoArray(Words, TEXT(" "), true);

	TArray<FString> Tokens;
	for (const FString& Word : Words)
	{
		FString Current;
		for (int32 Index = 0; Index < Word.Len(); ++Index)
		{
			const TCHAR C = Word[Index];
			const TCHAR Prev = Index > 0 ? Word[Index - 1] : 0;
			const TCHAR Next = Index + 1 < Word.Len() ? Word[Index + 1] : 0;
			const bool bCamelBoundary = Index > 0 && FChar::IsUpper(C)
				&& (FChar::IsLower(Prev) || FChar::IsDigit(Prev)
					|| (FChar::IsUpper(Prev) && FChar::IsLower(Next)));
			const bool bDigitBoundary = Index > 0 && FChar::IsDigit(C) && !FChar::IsDigit(Prev);
			if ((bCamelBoundary || bDigitBoundary) && !Current.IsEmpty())
			{
				Tokens.Add(Current);
				Current.Reset();
			}
			Current.AppendChar(C);
		}
		if (!Current.IsEmpty())
		{
			Tokens.Add(Current);
		}
	}

	bool bAny = false;
	FString Result;
	for (const FString& Token : Tokens)
	{
		if (!Token.IsEmpty() && FChar::IsDigit(Token[0]))
		{
			Result += Token;
			bAny = true;
			continue;
		}
		const FString* Found = Entries.Find(Token);
		const FString Piece = Found ? *Found : Token;
		if (Found)
		{
			bAny = true;
		}
		if (!Result.IsEmpty() && !Piece.IsEmpty() && Piece[0] < 128
			&& !FChar::IsDigit(Piece[0]) && Result[Result.Len() - 1] >= 128)
		{
			Result += TEXT(" ");
		}
		Result += Piece;
	}

	if (bAny)
	{
		OutTranslation = Result;
		return true;
	}
	return false;
}

bool FStarryDictionary::TranslateComposed(const FString& Source, FString& OutTranslation) const
{
	if (Source.StartsWith(TEXT("MF_")))
	{
		FString Rest = Source.Mid(3);
		TArray<FString> Parts;
		Rest.ParseIntoArray(Parts, TEXT("_"), true);
		TArray<FString> Translated;
		bool bAny = false;
		for (const FString& Part : Parts)
		{
			FString PartTranslation;
			if (TranslateWords(Part, PartTranslation))
			{
				Translated.Add(PartTranslation);
				bAny = true;
			}
			else
			{
				Translated.Add(Part);
			}
		}
		if (bAny)
		{
			OutTranslation = FString::Join(Translated, TEXT("_"));
			return true;
		}
		return false;
	}

	if (Source.StartsWith(TEXT("CoordinateBasis: ")))
	{
		const FString Rest = Source.Mid(17);
		const int32 SuffixIndex = Rest.Find(TEXT(" ("), ESearchCase::CaseSensitive, ESearchDir::FromStart);
		const FString Method = SuffixIndex > 0 ? Rest.Left(SuffixIndex) : Rest;
		FString Suffix = SuffixIndex > 0 ? Rest.Mid(SuffixIndex + 2) : FString();
		Suffix.RemoveFromEnd(TEXT(")"));

		FString MethodTranslation;
		FString SuffixTranslation;
		bool bAny = TranslateWords(Method, MethodTranslation);
		if (TranslateWords(Suffix, SuffixTranslation))
		{
			bAny = true;
		}
		if (!bAny)
		{
			return false;
		}

		OutTranslation = TEXT("坐标基准：") + (MethodTranslation.IsEmpty() ? Method : MethodTranslation);
		if (!Suffix.IsEmpty())
		{
			OutTranslation += TEXT("（") + (SuffixTranslation.IsEmpty() ? Suffix : SuffixTranslation) + TEXT("）");
		}
		return true;
	}

	return false;
}

void FStarryDictionary::BuildAssetNameGuard()
{
	AssetNames.Reset();
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("AssetRegistry")))
	{
		UE_LOG(LogStarryUEZh, Log, TEXT("AssetRegistry 未加载，跳过资产名保护"));
		return;
	}

	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	LoadAssetNames(AssetRegistry);

	AssetRegistry.OnAssetAdded().AddLambda([this](const FAssetData& Asset)
	{
		FScopeLock Lock(&AssetNameLock);
		AssetNames.Add(Asset.AssetName.ToString());
	});
	AssetRegistry.OnAssetRenamed().AddLambda([this](const FAssetData& Asset, const FString& OldName)
	{
		FScopeLock Lock(&AssetNameLock);
		AssetNames.Add(Asset.AssetName.ToString());
		AssetNames.Add(OldName);
	});

	if (AssetRegistry.IsLoadingAssets())
	{
		AssetRegistry.OnFilesLoaded().AddLambda([this]()
		{
			if (IAssetRegistry* Registry = IAssetRegistry::Get())
			{
				LoadAssetNames(*Registry);
			}
		});
		UE_LOG(LogStarryUEZh, Log, TEXT("AssetRegistry 仍在扫描，加载完成后会再刷新一次资产名保护"));
	}
}

bool FStarryDictionary::LoadNoTranslate(const FString& Path)
{
	NoTranslate.Reset();

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Terms = nullptr;
	if (!Root->TryGetArrayField(TEXT("terms"), Terms) || !Terms)
	{
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Value : *Terms)
	{
		FString Term;
		if (Value.IsValid() && Value->TryGetString(Term) && !Term.IsEmpty())
		{
			NoTranslate.Add(Term);
		}
	}
	UE_LOG(LogStarryUEZh, Log, TEXT("不翻译词表：%d 条（%s）"), NoTranslate.Num(), *Path);
	return true;
}

bool FStarryDictionary::LoadGlossary(const FString& Path)
{
	Glossary.Reset();

	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogStarryUEZh, Warning, TEXT("固定译法词表解析失败：%s"), *Path);
		return false;
	}

	const TSharedPtr<FJsonObject>* Terms = nullptr;
	if (!Root->TryGetObjectField(TEXT("terms"), Terms) || !Terms)
	{
		return false;
	}

	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Terms)->Values)
	{
		FString Translation;
		if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(Translation) || Translation.IsEmpty())
		{
			continue;
		}
		Glossary.Add(Pair.Key, Translation);
		// The curated table is authoritative for its terms: it replaces anything
		// a generated table produced. manual_zh.json still loads after it and wins.
		Entries.Add(Pair.Key, MoveTemp(Translation));
	}
	return true;
}

bool FStarryDictionary::Export(const FString& Path) const
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	TSharedPtr<FJsonObject> EntriesObject = MakeShared<FJsonObject>();
	for (const TPair<FString, FString>& Pair : Entries)
	{
		EntriesObject->SetStringField(Pair.Key, Pair.Value);
	}
	Root->SetObjectField(TEXT("entries"), EntriesObject);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
	{
		return false;
	}
	return FFileHelper::SaveStringToFile(Output, *Path);
}

bool FStarryDictionary::LoadFile(const FString& Path, bool bOverride, int32& OutAdded)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *Path))
	{
		return false;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<TCHAR>> Reader = TJsonReaderFactory<TCHAR>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogStarryUEZh, Warning, TEXT("JSON 解析失败：%s"), *Path);
		return false;
	}

	const TSharedPtr<FJsonObject>* EntriesObject = nullptr;
	if (!Root->TryGetObjectField(TEXT("entries"), EntriesObject) || !EntriesObject)
	{
		UE_LOG(LogStarryUEZh, Warning, TEXT("缺少 entries 字段：%s"), *Path);
		return false;
	}

	const int32 Before = Entries.Num();
	for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*EntriesObject)->Values)
	{
		FString Translation;
		if (!Pair.Value.IsValid() || !Pair.Value->TryGetString(Translation) || Translation.IsEmpty())
		{
			continue;
		}
		if (bOverride)
		{
			Entries.Add(Pair.Key, MoveTemp(Translation));
		}
		else if (!Entries.Contains(Pair.Key))
		{
			// Later tables only fill gaps; the first file that defines a key wins.
			Entries.Add(Pair.Key, MoveTemp(Translation));
		}
	}
	OutAdded = Entries.Num() - Before;
	return true;
}

bool FStarryDictionary::Translate(const FString& Source, FString& OutTranslation) const
{
	if (Source.Len() < 2 || Source.Len() > 400)
	{
		return false;
	}
	// English -> Chinese only. Anything that already contains a non-ASCII
	// character is either already translated or user content.
	if (ContainsNonAscii(Source))
	{
		return false;
	}

	// Brand / API names stay in English.
	if (NoTranslate.Contains(Source))
	{
		return false;
	}

	// An explicit dictionary entry always wins, even when the string is also an
	// asset name (material function node titles such as MF_Blend_CircleMask).
	if (const FString* Found = Entries.Find(Source))
	{
		OutTranslation = *Found;
		return true;
	}
	// Asset-name protection stays ahead of the compact index; explicit entries
	// such as AmbientOcclusion are handled by the exact lookup above.

	// Material function asset node titles and material function enum values are
	// often asset names, so translate them compositionally before the asset guard.
	if ((Source.StartsWith(TEXT("MF_")) || Source.StartsWith(TEXT("CoordinateBasis: ")))
		&& TranslateComposed(Source, OutTranslation))
	{
		return true;
	}

	if (IsAssetName(Source))
	{
		return false;
	}

	FString Core = Source;
	FString Prefix;
	if (!Core.IsEmpty() && Core[0] == TEXT('&'))
	{
		Prefix = TEXT("&");
		Core.RightChopInline(1);
	}

	int32 Start = 0;
	while (Start < Core.Len() && (Core[Start] == TEXT(' ') || Core[Start] == TEXT('\t')))
	{
		++Start;
	}
	int32 End = Core.Len();
	while (End > Start && (Core[End - 1] == TEXT(' ') || Core[End - 1] == TEXT('\t')))
	{
		--End;
	}
	const FString Lead = Core.Left(Start);
	const FString Trail = Core.Mid(End);
	const FString Trimmed = Core.Mid(Start, End - Start);

	// Exact trimmed entries also bypass the asset-name guard.
	if (const FString* Found = Entries.Find(Trimmed))
	{
		OutTranslation = Lead + Prefix + *Found + Trail;
		return true;
	}
	if (IsAssetName(Trimmed))
	{
		return false;
	}
	if (NoTranslate.Contains(Trimmed))
	{
		return false;
	}

	// Tooltip captions are often "Property - Object" ("Material Random - Material Node").
	// Translate both sides independently and keep the separator.
	if (Trimmed.Contains(TEXT(" - ")))
	{
		TArray<FString> Parts;
		Trimmed.ParseIntoArray(Parts, TEXT(" - "), false);
		if (Parts.Num() == 2)
		{
			FString LeftTranslation;
			FString RightTranslation;
			const bool bLeft = Translate(Parts[0], LeftTranslation);
			const bool bRight = Translate(Parts[1], RightTranslation);
			if (bLeft || bRight)
			{
				OutTranslation = Lead + Prefix
					+ (bLeft ? LeftTranslation : Parts[0])
					+ TEXT(" - ")
					+ (bRight ? RightTranslation : Parts[1])
					+ Trail;
				return true;
			}
		}
	}

	// Axis / channel labels such as "Random X/Y".
	if (Trimmed.Contains(TEXT("/")))
	{
		TArray<FString> Parts;
		Trimmed.ParseIntoArray(Parts, TEXT("/"), false);
		if (Parts.Num() >= 2 && Parts.Num() <= 4)
		{
			bool bAny = false;
			TArray<FString> Pieces;
			for (const FString& Part : Parts)
			{
				FString PartTranslation;
				if (Translate(Part, PartTranslation))
				{
					Pieces.Add(PartTranslation);
					bAny = true;
				}
				else
				{
					Pieces.Add(Part);
				}
			}
			if (bAny)
			{
				OutTranslation = Lead + Prefix + FString::Join(Pieces, TEXT("/")) + Trail;
				return true;
			}
		}
	}

	auto Lookup = [this](const FString& Key) -> const FString*
	{
		if (const FString* Found = Entries.Find(Key))
		{
			return Found;
		}
		return CompactEntries.Find(NormalizeKey(Key));
	};

	if (Trimmed.Len() >= 2)
	{
		const FString* Found = Lookup(Trimmed);
		FString Suffix;
		if (!Found)
		{
			// Common display suffixes.
			if (Trimmed.EndsWith(TEXT("...")))
			{
				Found = Lookup(Trimmed.LeftChop(3).TrimEnd());
				Suffix = TEXT("...");
			}
			else if (Trimmed.EndsWith(TEXT("…")))
			{
				Found = Lookup(Trimmed.LeftChop(1).TrimEnd());
				Suffix = TEXT("…");
			}
		}

		if (Found)
		{
			OutTranslation = Lead + Prefix + *Found + Suffix + Trail;
			return true;
		}

		// Material / graph captions often append a live value or parameter name:
		//   "Constant3Vector (0,0,0)"
		//   "Texture Sample 'Tex' (DefaultTexture)"
		//   "Component Mask [R,G,B]"
		// Translate the stable head and keep the dynamic tail untouched.
		static const TCHAR* Separators[] = { TEXT(" ("), TEXT(" '"), TEXT(" ["), TEXT(" <"), TEXT("("), TEXT("[") };
		for (const TCHAR* Separator : Separators)
		{
			// Try the right-most suffix first: "Render Shadows (Occlusion Mapping) (SB)"
			// must translate the full head, not just "Render Shadows".
			int32 SearchFrom = INDEX_NONE;
			while (true)
			{
				const int32 Index = Trimmed.Find(Separator, ESearchCase::CaseSensitive, ESearchDir::FromEnd, SearchFrom);
				if (Index <= 1)
				{
					break;
				}
				const FString Head = Trimmed.Left(Index).TrimEnd();
				const FString Tail = Trimmed.Mid(Index);
				if (const FString* HeadTranslation = Lookup(Head))
				{
					// Translate the parameter name inside "Head (Parameter)" too,
					// so "Static Switch (Write Material Random)" becomes fully Chinese.
					FString RenderedTail = Tail;
					if (Tail.StartsWith(TEXT(" (")) && Tail.EndsWith(TEXT(")")))
					{
						const FString Inner = Tail.Mid(2, Tail.Len() - 3);
						FString InnerTranslation;
						if (Translate(Inner, InnerTranslation))
						{
							RenderedTail = TEXT("（") + InnerTranslation + TEXT("）");
						}
					}
					OutTranslation = Lead + Prefix + *HeadTranslation + RenderedTail + Trail;
					return true;
				}
				SearchFrom = Index - 1;
			}
		}

		// Space-qualified variants: VertexNormalWS / ObjectOrientationOS.
		auto TranslateSpaceVariant = [&](const TCHAR* SuffixText, const TCHAR* SuffixTranslation) -> bool
		{
			if (!Trimmed.EndsWith(SuffixText))
			{
				return false;
			}
			const FString Head = Trimmed.LeftChop(FCString::Strlen(SuffixText)).TrimEnd();
			if (const FString* HeadTranslation = Lookup(Head))
			{
				OutTranslation = Lead + Prefix + *HeadTranslation + SuffixTranslation + Trail;
				return true;
			}
			return false;
		};

		if (TranslateSpaceVariant(TEXT("WS"), TEXT("（世界空间）"))
			|| TranslateSpaceVariant(TEXT("OS"), TEXT("（对象空间）")))
		{
			return true;
		}
	}

	return false;
}

bool FStarryDictionary::TranslateReverse(const FString& Source, FString& OutTranslation) const
{
	if (Source.Len() < 1 || Source.Len() > 400)
	{
		return false;
	}
	if (!ContainsNonAscii(Source))
	{
		return false;
	}
	if (IsAssetName(Source))
	{
		return false;
	}

	FString Core = Source;
	int32 Start = 0;
	while (Start < Core.Len() && (Core[Start] == TEXT(' ') || Core[Start] == TEXT('\t')))
	{
		++Start;
	}
	int32 End = Core.Len();
	while (End > Start && (Core[End - 1] == TEXT(' ') || Core[End - 1] == TEXT('\t')))
	{
		--End;
	}
	const FString Lead = Core.Left(Start);
	const FString Trail = Core.Mid(End);
	const FString Trimmed = Core.Mid(Start, End - Start);

	if (IsAssetName(Trimmed))
	{
		return false;
	}

	if (const FString* Found = ReverseEntries.Find(Trimmed))
	{
		OutTranslation = Lead + *Found + Trail;
		return true;
	}

	// Chinese captions with a value suffix: "常量（0,0,0）".
	const int32 SuffixIndex = Trimmed.Find(TEXT("（"), ESearchCase::CaseSensitive, ESearchDir::FromStart);
	if (SuffixIndex > 1)
	{
		const FString Head = Trimmed.Left(SuffixIndex).TrimEnd();
		if (const FString* HeadTranslation = ReverseEntries.Find(Head))
		{
			OutTranslation = Lead + *HeadTranslation + Trimmed.Mid(SuffixIndex) + Trail;
			return true;
		}
	}

	return false;
}

bool FStarryDictionary::TranslateDisplay(const FString& Source, FString& OutTranslation) const
{
	// English mode is handled by loading the official en locres; no reverse
	// rewriting is needed (and reverse rewriting produced wrong menu names).
	return bEnabled.load(std::memory_order_relaxed)
		? Translate(Source, OutTranslation)
		: false;
}

bool FStarryDictionary::TranslateMultiline(const FString& Source, FString& OutTranslation) const
{
	return TranslateMultilineWith(Source, OutTranslation, true);
}

bool FStarryDictionary::TranslateMultilineDisplay(const FString& Source, FString& OutTranslation) const
{
	return bEnabled.load(std::memory_order_relaxed)
		? TranslateMultilineWith(Source, OutTranslation, true)
		: false;
}

bool FStarryDictionary::TranslateMultilineWith(const FString& Source, FString& OutTranslation, bool bChineseMode) const
{
	if (Source.IsEmpty())
	{
		return false;
	}

	TArray<FString> Lines;
	Source.ParseIntoArray(Lines, TEXT("\n"), false);
	bool bAny = false;
	for (FString& Line : Lines)
	{
		// Preserve CR and indentation.
		FString Body = Line;
		FString Suffix;
		if (Body.EndsWith(TEXT("\r")))
		{
			Suffix = TEXT("\r");
			Body.LeftChopInline(1);
		}

		int32 Start = 0;
		while (Start < Body.Len() && (Body[Start] == TEXT(' ') || Body[Start] == TEXT('\t')))
		{
			++Start;
		}
		int32 End = Body.Len();
		while (End > Start && (Body[End - 1] == TEXT(' ') || Body[End - 1] == TEXT('\t')))
		{
			--End;
		}

		const FString Core = Body.Mid(Start, End - Start);
		FString Translated;
		const bool bHit = bChineseMode ? Translate(Core, Translated) : TranslateReverse(Core, Translated);
		if (bHit)
		{
			Line = Body.Left(Start) + Translated + Body.Mid(End) + Suffix;
			bAny = true;
		}
	}

	if (bAny)
	{
		OutTranslation = FString::Join(Lines, TEXT("\n"));
	}
	return bAny;
}
