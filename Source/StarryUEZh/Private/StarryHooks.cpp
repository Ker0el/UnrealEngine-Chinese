// Copyright 2026 StarGame. All Rights Reserved.

#include "StarryHooks.h"

#include "StarryDictionary.h"
#include "StarryUEZh.h"

#include "Containers/Ticker.h"
#include "AnimStateNode.h"
#include "AnimStateNodeBase.h"
#include "AnimStateTransitionNode.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/Skeleton.h"
#include "AnimationStateMachineGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Text/PlainTextLayoutMarshaller.h"
#include "Framework/Text/SlateTextLayout.h"
#include "Framework/Text/TextLayout.h"
#include "Fonts/FontCache.h"
#include "Rendering/SlateRenderer.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformProcess.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunction.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "K2Node_CallFunction.h"
#include "NiagaraActions.h"
#include "ObjectTools.h"
#include "Rendering/DrawElements.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"
#include "Widgets/Text/SlateTextBlockLayout.h"

#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"

// Detours uses the Windows SDK architecture macros; MSVC itself only defines
// _M_X64 / _M_AMD64, so define the one Detours looks for.
#if PLATFORM_WINDOWS && PLATFORM_64BITS && !defined(_AMD64_)
#define _AMD64_
#endif

THIRD_PARTY_INCLUDES_START
#include <detours/detours.h>
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
	// ---------------------------------------------------------------------
	// Mangled exports (UE 5.8, Win64 editor).
	// ---------------------------------------------------------------------
	const TCHAR* Export_SetText =
		TEXT("?SetText@FPlainTextLayoutMarshaller@@UEAAXAEBVFString@@AEAVFTextLayout@@@Z");
	// 5.0 - 5.2 only have the ComputeDesiredSize overload that also takes the
	// text block style. The two argument form appears in 5.3.
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 3
	const TCHAR* Export_ComputeDesiredSize =
		TEXT("?ComputeDesiredSize@FSlateTextBlockLayout@@QEAA?AU?$TVector2@N@Math@UE@@AEBUFWidgetDesiredSizeArgs@1@M@Z");
#else
	const TCHAR* Export_ComputeDesiredSize =
		TEXT("?ComputeDesiredSize@FSlateTextBlockLayout@@QEAA?AU?$TVector2@N@Math@UE@@AEBUFWidgetDesiredSizeArgs@1@MAEBUFTextBlockStyle@@@Z");
#endif
	// 5.2 moved the return type to FDeprecateSlateVector2D and dropped the
	// virtual qualifier. 5.0 / 5.1 still return FVector2D from a virtual
	// function, so the mangled name differs in both the qualifier and the type.
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 2
	const TCHAR* Export_MeasureText =
		TEXT("?Measure@FSlateFontMeasure@@QEBA?AUFDeprecateSlateVector2D@@AEBVFText@@AEBUFSlateFontInfo@@M@Z");
#else
	const TCHAR* Export_MeasureText =
		TEXT("?Measure@FSlateFontMeasure@@UEBA?AU?$TVector2@N@Math@UE@@AEBVFText@@AEBUFSlateFontInfo@@M@Z");
#endif
	const TCHAR* Export_MakeText =
		TEXT("?MakeText@FSlateDrawElement@@SAXAEAVFSlateWindowElementList@@IAEBUFPaintGeometry@@AEBVFString@@AEBUFSlateFontInfo@@W4ESlateDrawEffect@@AEBUFLinearColor@@@Z");
	const TCHAR* Export_UpdateSearchData =
		TEXT("?UpdateSearchData@FEdGraphSchemaAction@@QEAAXVFText@@000@Z");
	const TCHAR* Export_NiagaraMatch =
		TEXT("?DoesItemMatchFilterText@FNiagaraEditorUtilities@@YA_NAEBVFText@@AEBV?$TSharedPtr@UFNiagaraMenuAction_Generic@@$00@@@Z");
	const TCHAR* Export_NiagaraWeight =
		TEXT("?GetWeightForItem@FNiagaraEditorUtilities@@YAHAEBV?$TSharedPtr@UFNiagaraMenuAction_Generic@@$00@@VFName@@AEBV?$TArray@VFString@@V?$TSizedDefaultAllocator@$0CA@@@@@@Z");
	// UE 5.4 / 5.5 have no search profile argument at all.
	const TCHAR* Export_NiagaraWeightLegacy =
		TEXT("?GetWeightForItem@FNiagaraEditorUtilities@@YAHAEBV?$TSharedPtr@UFNiagaraMenuAction_Generic@@$00@@AEBV?$TArray@VFString@@V?$TSizedDefaultAllocator@$0CA@@@@@@Z");

	const TCHAR* Export_RichSetText =
		TEXT("?SetText@FRichTextLayoutMarshaller@@UEAAXAEBVFString@@AEAVFTextLayout@@@Z");

	using FSetTextFn = void (*)(void*, const FString&, FTextLayout&);
	// 5.0 / 5.1 measure into FVector2D, 5.2 and newer into
	// FDeprecateSlateVector2D (aliased as FDeprecateVector2DResult).
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 2
	using FMeasureResult = UE::Slate::FDeprecateVector2DResult;
#else
	using FMeasureResult = FVector2D;
#endif
	// MSVC passes the hidden return buffer for a member function in RDX:
	//   RCX = this, RDX = return buffer, R8 = first arg, ...
	// A function that returns a class by value must also hand that buffer back
	// in RAX. Callers such as SSlowTaskWidget::GetMainTextFont() read the result
	// through that register, so a void hook leaves them with a stale pointer and
	// they crash on the next dereference.
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 3
	using FComputeDesiredSizeFn = FVector2D* (*)(FSlateTextBlockLayout*, FVector2D*, const FSlateTextBlockLayout::FWidgetDesiredSizeArgs&, float);
#else
	using FComputeDesiredSizeFn = FVector2D* (*)(FSlateTextBlockLayout*, FVector2D*, const FSlateTextBlockLayout::FWidgetDesiredSizeArgs&, float, const FTextBlockStyle&);
#endif
	using FMeasureTextFn = FMeasureResult* (*)(const FSlateFontMeasure*, FMeasureResult*, const FText&, const FSlateFontInfo&, float);
	using FMakeTextFn = void (*)(FSlateWindowElementList&, uint32, const FPaintGeometry&, const FString&, const FSlateFontInfo&, ESlateDrawEffect, const FLinearColor&);
	using FUpdateSearchDataFn = void (*)(FEdGraphSchemaAction*, FText, FText, FText, FText);
	using FNiagaraMatchFn = bool (*)(const FText&, const TSharedPtr<FNiagaraMenuAction_Generic>&);
	using FNiagaraWeightFn = int32 (*)(const TSharedPtr<FNiagaraMenuAction_Generic>&, FName, const TArray<FString>&);
	using FNiagaraWeightLegacyFn = int32 (*)(const TSharedPtr<FNiagaraMenuAction_Generic>&, const TArray<FString>&);

	FSetTextFn Real_RichSetText = nullptr;
	FSetTextFn Real_SetText = nullptr;
	FComputeDesiredSizeFn Real_ComputeDesiredSize = nullptr;
	FMeasureTextFn Real_MeasureText = nullptr;
	FMakeTextFn Real_MakeText = nullptr;
	FUpdateSearchDataFn Real_UpdateSearchData = nullptr;
	FNiagaraMatchFn Real_NiagaraMatch = nullptr;
	FNiagaraWeightFn Real_NiagaraWeight = nullptr;
	FNiagaraWeightLegacyFn Real_NiagaraWeightLegacy = nullptr;

	std::atomic<bool> bRefreshPending{ false };
	bool bNiagaraHooksInstalled = false;

	// Diagnostics for the material/graph title investigation.
	int64 GSetTextCalls = 0;
	int64 GSetTextHits = 0;

	struct FGapRecord
	{
		int32 Count = 0;
		FString Window;
	};

	TMap<FString, FGapRecord> GGaps;
	FCriticalSection GGapLock;

	bool LooksLikeUiString(const FString& Text)
	{
		if (Text.Len() < 2 || Text.Len() > 120)
		{
			return false;
		}
		if (Text.Contains(TEXT("\n")) || Text.Contains(TEXT("\r")) || Text.Contains(TEXT("\t")))
		{
			return false;
		}
		if (Text.Contains(TEXT("/")) || Text.Contains(TEXT("\\")) || Text.Contains(TEXT("://")))
		{
			return false;
		}
		bool bHasAlpha = false;
		for (const TCHAR C : Text)
		{
			if (C > 127)
			{
				return false;
			}
			bHasAlpha |= FChar::IsAlpha(C);
			if (!FChar::IsPrint(C))
			{
				return false;
			}
		}
		return bHasAlpha;
	}

	// ---------------------------------------------------------------------
	// CJK font fallback
	//
	// Translated strings are drawn with the widget's own font. Game fonts and
	// custom UMG fonts often have no CJK glyphs, so translated text turns into
	// tofu boxes. Add DroidSansFallback as a CJK sub-font of the composite font
	// (in memory only) so layout shaping and drawing both find glyphs.
	// ---------------------------------------------------------------------

	bool TextHasCjk(const FString& Text)
	{
		for (const TCHAR C : Text)
		{
			if ((C >= 0x2E80 && C <= 0x33FF)
				|| (C >= 0x3400 && C <= 0x4DBF)
				|| (C >= 0x4E00 && C <= 0x9FFF)
				|| (C >= 0xF900 && C <= 0xFAFF)
				|| (C >= 0xFE30 && C <= 0xFE4F)
				|| (C >= 0xFF00 && C <= 0xFFEF))
			{
				return true;
			}
		}
		return false;
	}

	bool CompositeHasCjkRange(const FCompositeFont& Composite)
	{
		for (const FCompositeSubFont& SubFont : Composite.SubTypefaces)
		{
			for (const FInt32Range& Range : SubFont.CharacterRanges)
			{
				if (Range.Contains(0x4E00) || Range.Contains(0x4E2D) || Range.Contains(0x8BBE))
				{
					return true;
				}
			}
		}
		return false;
	}

	void InjectCjkFallback(FCompositeFont& Composite)
	{
		FCompositeSubFont& SubFont = Composite.SubTypefaces.AddDefaulted_GetRef();
		SubFont.Typeface.AppendFont(
			TEXT("Regular"),
			FPaths::EngineContentDir() / TEXT("Slate/Fonts/DroidSansFallback.ttf"),
			EFontHinting::Default,
			EFontLoadingPolicy::LazyLoad);
		SubFont.CharacterRanges.Add(FInt32Range(0x2E80, 0x33FF));
		SubFont.CharacterRanges.Add(FInt32Range(0x3400, 0x4DBF));
		SubFont.CharacterRanges.Add(FInt32Range(0x4E00, 0x9FFF));
		SubFont.CharacterRanges.Add(FInt32Range(0xF900, 0xFAFF));
		SubFont.CharacterRanges.Add(FInt32Range(0xFE30, 0xFE4F));
		SubFont.CharacterRanges.Add(FInt32Range(0xFF00, 0xFFEF));
#if WITH_EDITORONLY_DATA
		Composite.MakeDirty();
#endif
		UE_LOG(LogStarryUEZh, Log, TEXT("字体缺少中文字形，已挂载回退字体 DroidSansFallback"));
	}

	/** Returns true when a fallback was added and widgets should relayout. */
	bool EnsureCjkFallback(const FSlateFontInfo& FontInfo)
	{
		const FCompositeFont* Composite = FontInfo.GetCompositeFont();
		if (!Composite || !FSlateApplicationBase::IsInitialized())
		{
			return false;
		}

		static TSet<const FCompositeFont*> Checked;
		if (Checked.Contains(Composite))
		{
			return false;
		}
		Checked.Add(Composite);

		if (CompositeHasCjkRange(*Composite))
		{
			return false;
		}

		FSlateRenderer* Renderer = FSlateApplicationBase::Get().GetRenderer();
		if (!Renderer)
		{
			return false;
		}

		FCharacterList& Characters = Renderer->GetFontCache()->GetCharacterList(FontInfo, 1.0f);
		static const TCHAR Probes[] = { 0x4E2D, 0x6587, 0x8BBE };
		for (const TCHAR Probe : Probes)
		{
			const FCharacterEntry& Entry = Characters.GetCharacter(Probe, EFontFallback::FF_NoFallback);
			// A missing glyph resolves to the invalid-substitute character, so the
			// returned entry must be for the probed character itself.
			if (Entry.Character == Probe && Entry.Valid && Entry.GlyphIndex != 0
				&& Entry.FallbackLevel == EFontFallback::FF_NoFallback)
			{
				return false;
			}
		}

		InjectCjkFallback(const_cast<FCompositeFont&>(*Composite));
		Renderer->GetFontCache()->RequestFlushCache(TEXT("StarryUEZh CJK fallback"));
		return true;
	}

	/**
	 * The marshaller's SetText runs before the layout shapes the text, so
	 * injecting here makes the cached layout use the fallback glyph metrics.
	 */
	void EnsureCjkFallbackForLayout(const FString& Text, const FTextLayout& Layout)
	{
		if (!TextHasCjk(Text))
		{
			return;
		}

		// Every Slate text block marshaller is paired with an FSlateTextLayout.
		const FSlateTextLayout& SlateLayout = static_cast<const FSlateTextLayout&>(Layout);
		if (EnsureCjkFallback(SlateLayout.GetDefaultTextStyle().Font))
		{
			// The layout was shaped with the old font; rebuild it once the
			// font cache flush has been applied.
			StarryHooks::RequestRefresh();
		}
	}

	void AddGap(const FString& Text)
	{
		if (!LooksLikeUiString(Text))
		{
			return;
		}

		// Scan categories (UEnum:..., NiagaraDescription:...) are labels this
		// plugin renders in its own panel, not UI copy. They would otherwise
		// flood the gap report whenever the panel is in front.
		static const TCHAR* CategoryPrefixes[] = {
			TEXT("UEnum:"), TEXT("UClass:"), TEXT("UFunctionTooltip:"),
			TEXT("PropertyTooltip:"), TEXT("NiagaraDescription:"),
			TEXT("MaterialDescription:"), TEXT("PluginDescription:"),
			TEXT("AssetDescription:")
		};
		for (const TCHAR* Prefix : CategoryPrefixes)
		{
			if (Text.StartsWith(Prefix, ESearchCase::CaseSensitive))
			{
				return;
			}
		}

		TCHAR Title[256] = { 0 };
		::GetWindowTextW(::GetForegroundWindow(), Title, UE_ARRAY_COUNT(Title));
		const FString Window(Title);

		// Never report text drawn by our own panel: it shows source strings
		// verbatim, so every row would look like a missing translation.
		if (Window.Contains(TEXT("星空汉化")))
		{
			return;
		}
		if (FStarryDictionary::Get().IsProtectedName(Text))
		{
			return;
		}

		FScopeLock Lock(&GGapLock);
		FGapRecord& Record = GGaps.FindOrAdd(Text);
		++Record.Count;
		if (!Window.IsEmpty())
		{
			Record.Window = Window;
		}
	}

	struct FHookRecord
	{
		void** RealSlot = nullptr;
		void* Hook = nullptr;
	};
	TArray<FHookRecord> InstalledHooks;

	void* FindExport(const TCHAR* ModuleName, const TCHAR* ExportName)
	{
		FString ModulePath = FModuleManager::Get().GetModuleFilename(FName(ModuleName));
		void* Handle = nullptr;
		if (!ModulePath.IsEmpty())
		{
			Handle = FPlatformProcess::GetDllHandle(*ModulePath);
		}
		if (!Handle)
		{
			const FString Fallback = FString::Printf(TEXT("UnrealEditor-%s.dll"), ModuleName);
			Handle = FPlatformProcess::GetDllHandle(*Fallback);
		}
		if (!Handle)
		{
			return nullptr;
		}
		return FPlatformProcess::GetDllExport(Handle, ExportName);
	}

	// IAssetRegistry::GetAssetsByClass takes a class path in 5.1 and newer, and
	// a plain class name before that. UClass has no GetClassPathName in 5.0.
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 1
	using FAssetClassQuery = FTopLevelAssetPath;
	FORCEINLINE FAssetClassQuery MakeAssetClassQuery(const UClass* Class)
	{
		return Class->GetClassPathName();
	}
#else
	using FAssetClassQuery = FName;
	FORCEINLINE FAssetClassQuery MakeAssetClassQuery(const UClass* Class)
	{
		return Class->GetFName();
	}
#endif

	bool InstallOne(const TCHAR* ModuleName, const TCHAR* ExportName, void* Hook, void** RealSlot, const TCHAR* Label)
	{
		void* Target = FindExport(ModuleName, ExportName);
		if (!Target)
		{
			UE_LOG(LogStarryUEZh, Warning, TEXT("未找到导出：%s（%s）"), Label, ModuleName);
			return false;
		}

		*RealSlot = Target;
		if (DetourTransactionBegin() != NO_ERROR)
		{
			UE_LOG(LogStarryUEZh, Warning, TEXT("DetourTransactionBegin 失败：%s"), Label);
			*RealSlot = nullptr;
			return false;
		}
		DetourUpdateThread(GetCurrentThread());

		const LONG AttachResult = DetourAttach(reinterpret_cast<PVOID*>(RealSlot), Hook);
		if (AttachResult != NO_ERROR)
		{
			DetourTransactionAbort();
			*RealSlot = nullptr;
			UE_LOG(LogStarryUEZh, Warning, TEXT("DetourAttach 失败：%s（%d）"), Label, AttachResult);
			return false;
		}

		const LONG CommitResult = DetourTransactionCommit();
		if (CommitResult != NO_ERROR)
		{
			*RealSlot = nullptr;
			UE_LOG(LogStarryUEZh, Warning, TEXT("DetourTransactionCommit 失败：%s（%d）"), Label, CommitResult);
			return false;
		}

		InstalledHooks.Add({ RealSlot, Hook });
		UE_LOG(LogStarryUEZh, Log, TEXT("已挂载：%s"), Label);
		return true;
	}

	void LogSetTextVTable()
	{
		// Verify that the address we detoured is the one the virtual dispatch
		// actually uses. If these differ, patching the export has no effect.
		TSharedRef<FPlainTextLayoutMarshaller> Probe = FPlainTextLayoutMarshaller::Create();
		void** VTable = *reinterpret_cast<void***>(&Probe.Get());
		void* Target = FindExport(TEXT("Slate"), Export_SetText);
		UE_LOG(LogStarryUEZh, Log, TEXT("SetText 诊断：导出地址=%p，探测对象 vtable=%p"), Target, VTable);
		bool bFound = false;
		for (int32 Index = 0; Index < 12; ++Index)
		{
			UE_LOG(LogStarryUEZh, Log, TEXT("  vtable[%d]=%p%s"), Index, VTable[Index],
				VTable[Index] == Target ? TEXT("  <== 与导出地址相同") : TEXT(""));
			bFound |= (VTable[Index] == Target);
		}
		if (!bFound)
		{
			UE_LOG(LogStarryUEZh, Warning, TEXT("SetText 诊断：vtable 中找不到导出地址，虚拟调用不会经过我们的钩子"));
		}
	}

	// ---------------------------------------------------------------------
	// Display layer
	// ---------------------------------------------------------------------

	void Hook_SetText(void* This, const FString& SourceString, FTextLayout& TargetLayout)
	{
		if (Real_SetText)
		{
			++GSetTextCalls;
			FString Translated;
			const bool bHit = FStarryDictionary::Get().TranslateMultilineDisplay(SourceString, Translated);
			if (bHit)
			{
				++GSetTextHits;
			}
			else if (FStarryDictionary::Get().IsEnabled())
			{
				AddGap(SourceString);
			}

			if (bHit)
			{
				EnsureCjkFallbackForLayout(Translated, TargetLayout);
				Real_SetText(This, Translated, TargetLayout);
				return;
			}
			Real_SetText(This, SourceString, TargetLayout);
		}
	}

	FString TranslateRichText(const FString& Source)
	{
		FString Result;
		Result.Reserve(Source.Len());
		bool bAny = false;
		int32 Index = 0;
		while (Index < Source.Len())
		{
			if (Source[Index] == TEXT('<'))
			{
				const int32 TagEnd = Source.Find(TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
				if (TagEnd != INDEX_NONE)
				{
					Result += Source.Mid(Index, TagEnd - Index + 1);
					Index = TagEnd + 1;
					continue;
				}
			}

			const int32 NextTag = Source.Find(TEXT("<"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Index);
			const FString Segment = (NextTag == INDEX_NONE) ? Source.Mid(Index) : Source.Mid(Index, NextTag - Index);
			FString Translated;
			if (FStarryDictionary::Get().TranslateMultilineDisplay(Segment, Translated))
			{
				Result += Translated;
				bAny = true;
			}
			else
			{
				Result += Segment;
			}
			Index = (NextTag == INDEX_NONE) ? Source.Len() : NextTag;
		}
		return bAny ? Result : Source;
	}

	void Hook_RichSetText(void* This, const FString& SourceString, FTextLayout& TargetLayout)
	{
		if (Real_RichSetText)
		{
			const FString Translated = TranslateRichText(SourceString);
			EnsureCjkFallbackForLayout(Translated, TargetLayout);
			Real_RichSetText(This, Translated, TargetLayout);
		}
	}

#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 3
	FVector2D* Hook_ComputeDesiredSize(FSlateTextBlockLayout* This, FVector2D* OutResult,
		const FSlateTextBlockLayout::FWidgetDesiredSizeArgs& Args, float Scale)
	{
		if (bRefreshPending.load(std::memory_order_relaxed))
		{
			This->DirtyContent();
		}
		if (Real_ComputeDesiredSize)
		{
			return Real_ComputeDesiredSize(This, OutResult, Args, Scale);
		}
		if (OutResult)
		{
			*OutResult = FVector2D::ZeroVector;
		}
		return OutResult;
	}
#else
	FVector2D* Hook_ComputeDesiredSize(FSlateTextBlockLayout* This, FVector2D* OutResult,
		const FSlateTextBlockLayout::FWidgetDesiredSizeArgs& Args, float Scale, const FTextBlockStyle& Style)
	{
		if (bRefreshPending.load(std::memory_order_relaxed))
		{
			This->DirtyContent();
		}
		if (Real_ComputeDesiredSize)
		{
			return Real_ComputeDesiredSize(This, OutResult, Args, Scale, Style);
		}
		if (OutResult)
		{
			*OutResult = FVector2D::ZeroVector;
		}
		return OutResult;
	}
#endif

	FMeasureResult* Hook_MeasureText(const FSlateFontMeasure* This, FMeasureResult* OutResult,
		const FText& Text, const FSlateFontInfo& FontInfo, float FontScale)
	{
		if (Real_MeasureText)
		{
			FString Translated;
			if (FStarryDictionary::Get().TranslateDisplay(Text.ToString(), Translated))
			{
				if (TextHasCjk(Translated) && EnsureCjkFallback(FontInfo))
				{
					StarryHooks::RequestRefresh();
				}
				const FText TranslatedText = FText::FromString(MoveTemp(Translated));
				return Real_MeasureText(This, OutResult, TranslatedText, FontInfo, FontScale);
			}
			return Real_MeasureText(This, OutResult, Text, FontInfo, FontScale);
		}
		if (OutResult)
		{
			*OutResult = FMeasureResult(0.0f, 0.0f);
		}
		return OutResult;
	}

	void Hook_MakeText(FSlateWindowElementList& ElementList, uint32 InLayer, const FPaintGeometry& PaintGeometry,
		const FString& InText, const FSlateFontInfo& FontInfo, ESlateDrawEffect DrawEffects, const FLinearColor& Tint)
	{
		if (Real_MakeText)
		{
			FString Translated;
			if (FStarryDictionary::Get().TranslateMultilineDisplay(InText, Translated))
			{
				if (TextHasCjk(Translated) && EnsureCjkFallback(FontInfo))
				{
					StarryHooks::RequestRefresh();
				}
				Real_MakeText(ElementList, InLayer, PaintGeometry, Translated, FontInfo, DrawEffects, Tint);
				return;
			}
			Real_MakeText(ElementList, InLayer, PaintGeometry, InText, FontInfo, DrawEffects, Tint);
		}
	}

	// ---------------------------------------------------------------------
	// Search layer
	// ---------------------------------------------------------------------

	void AddUniqueTerm(FString& Combined, const FString& Term)
	{
		if (Term.IsEmpty())
		{
			return;
		}
		if (Combined.Contains(Term, ESearchCase::CaseSensitive))
		{
			return;
		}
		if (!Combined.IsEmpty())
		{
			Combined += TEXT(" ");
		}
		Combined += Term;
	}

	void AddTranslatedTerm(FString& Combined, const FString& Source)
	{
		if (Source.IsEmpty())
		{
			return;
		}

		FString Translated;
		if (FStarryDictionary::Get().Translate(Source, Translated))
		{
			AddUniqueTerm(Combined, Translated);
			return;
		}

		// Strings such as "Cast To {TargetName}" or "Add {Name} node here".
		int32 BraceIndex = INDEX_NONE;
		if (Source.FindChar(TEXT('{'), BraceIndex) && BraceIndex > 1)
		{
			if (FStarryDictionary::Get().Translate(Source.Left(BraceIndex).TrimEnd(), Translated))
			{
				AddUniqueTerm(Combined, Translated);
			}
		}
	}

	void Hook_UpdateSearchData(FEdGraphSchemaAction* This, FText NewMenuDescription, FText NewToolTipDescription, FText NewCategory, FText NewKeywords)
	{
		if (FStarryDictionary::Get().IsEnabled())
		{
			const FString OriginalKeywords = NewKeywords.ToString();
			FString Combined = OriginalKeywords;

			AddTranslatedTerm(Combined, NewMenuDescription.ToString());
			AddTranslatedTerm(Combined, NewMenuDescription.BuildSourceString());
			AddTranslatedTerm(Combined, NewCategory.ToString());
			AddTranslatedTerm(Combined, NewCategory.BuildSourceString());
			AddTranslatedTerm(Combined, NewKeywords.ToString());
			AddTranslatedTerm(Combined, NewKeywords.BuildSourceString());

			TArray<FString> CategoryParts;
			NewCategory.ToString().ParseIntoArray(CategoryParts, TEXT("|"), true);
			for (const FString& Part : CategoryParts)
			{
				AddTranslatedTerm(Combined, Part.TrimStartAndEnd());
			}

			if (Combined != OriginalKeywords && Combined.Len() <= 4096)
			{
				NewKeywords = FText::FromString(MoveTemp(Combined));
			}
		}

		if (Real_UpdateSearchData)
		{
			Real_UpdateSearchData(This, MoveTemp(NewMenuDescription), MoveTemp(NewToolTipDescription), MoveTemp(NewCategory), MoveTemp(NewKeywords));
		}
	}

	// ---------------------------------------------------------------------
	// Niagara search
	// ---------------------------------------------------------------------

	bool AnyTermIsChinese(const TArray<FString>& Terms)
	{
		for (const FString& Term : Terms)
		{
			if (FStarryDictionary::ContainsNonAscii(Term))
			{
				return true;
			}
		}
		return false;
	}

	bool ChineseTermsMatch(const TSharedPtr<FNiagaraMenuAction_Generic>& Item, const TArray<FString>& Terms)
	{
		if (!Item.IsValid() || Terms.Num() == 0)
		{
			return false;
		}

		TArray<FString> Haystack;
		auto AddTranslated = [&Haystack](const FString& Source)
		{
			FString Translated;
			if (FStarryDictionary::Get().Translate(Source, Translated))
			{
				Haystack.Add(MoveTemp(Translated));
			}
		};

		AddTranslated(Item->DisplayName.ToString());
		AddTranslated(Item->Keywords.ToString());
		for (const FString& Category : Item->Categories)
		{
			AddTranslated(Category);
		}

		if (Haystack.Num() == 0)
		{
			return false;
		}

		for (const FString& Term : Terms)
		{
			bool bFound = false;
			for (const FString& Hay : Haystack)
			{
				if (Hay.Contains(Term, ESearchCase::IgnoreCase))
				{
					bFound = true;
					break;
				}
			}
			if (!bFound)
			{
				return false;
			}
		}
		return true;
	}

	bool Hook_NiagaraMatch(const FText& FilterText, const TSharedPtr<FNiagaraMenuAction_Generic>& Item)
	{
		if (Real_NiagaraMatch && Real_NiagaraMatch(FilterText, Item))
		{
			return true;
		}
		if (!FStarryDictionary::Get().IsEnabled())
		{
			return false;
		}

		const FString Filter = FilterText.ToString();
		if (!FStarryDictionary::ContainsNonAscii(Filter))
		{
			return false;
		}

		TArray<FString> Terms;
		Filter.ParseIntoArray(Terms, TEXT(" "), true);
		return ChineseTermsMatch(Item, Terms);
	}

	int32 Hook_NiagaraWeight(const TSharedPtr<FNiagaraMenuAction_Generic>& Item, FName Profile, const TArray<FString>& FilterTerms)
	{
		const int32 Original = Real_NiagaraWeight ? Real_NiagaraWeight(Item, Profile, FilterTerms) : 0;
		if (Original > 0 || !FStarryDictionary::Get().IsEnabled() || !AnyTermIsChinese(FilterTerms))
		{
			return Original;
		}
		return ChineseTermsMatch(Item, FilterTerms) ? 100 : Original;
	}

	int32 Hook_NiagaraWeightLegacy(const TSharedPtr<FNiagaraMenuAction_Generic>& Item, const TArray<FString>& FilterTerms)
	{
		const int32 Original = Real_NiagaraWeightLegacy ? Real_NiagaraWeightLegacy(Item, FilterTerms) : 0;
		if (Original > 0 || !FStarryDictionary::Get().IsEnabled() || !AnyTermIsChinese(FilterTerms))
		{
			return Original;
		}
		return ChineseTermsMatch(Item, FilterTerms) ? 100 : Original;
	}

	bool InstallNiagaraHooks()
	{
		if (bNiagaraHooksInstalled)
		{
			return true;
		}

		const bool bMatch = InstallOne(TEXT("NiagaraEditor"), Export_NiagaraMatch,
			reinterpret_cast<void*>(&Hook_NiagaraMatch), reinterpret_cast<void**>(&Real_NiagaraMatch), TEXT("Niagara DoesItemMatchFilterText"));
		bool bWeight = InstallOne(TEXT("NiagaraEditor"), Export_NiagaraWeight,
			reinterpret_cast<void*>(&Hook_NiagaraWeight), reinterpret_cast<void**>(&Real_NiagaraWeight), TEXT("Niagara GetWeightForItem"));
		if (!bWeight)
		{
			bWeight = InstallOne(TEXT("NiagaraEditor"), Export_NiagaraWeightLegacy,
				reinterpret_cast<void*>(&Hook_NiagaraWeightLegacy), reinterpret_cast<void**>(&Real_NiagaraWeightLegacy),
				TEXT("Niagara GetWeightForItem (5.4/5.5)"));
		}

		bNiagaraHooksInstalled = bMatch || bWeight;
		return bNiagaraHooksInstalled;
	}

	bool LooksHarvestable(const FString& Text)
	{
		if (Text.Len() < 2 || Text.Len() > 400)
		{
			return false;
		}
		if (Text.Contains(TEXT("\n")) || Text.Contains(TEXT("\r")) || Text.Contains(TEXT("\t")))
		{
			return false;
		}
		if (Text.Contains(TEXT("/")) || Text.Contains(TEXT("\\")) || Text.Contains(TEXT("://")))
		{
			return false;
		}
		bool bHasAlpha = false;
		for (const TCHAR C : Text)
		{
			if (C > 127 || !FChar::IsPrint(C))
			{
				return false;
			}
			bHasAlpha |= FChar::IsAlpha(C);
		}
		return bHasAlpha;
	}

	void DumpGapsImpl()
	{
		TArray<TPair<FString, FGapRecord>> Sorted;
		{
			FScopeLock Lock(&GGapLock);
			Sorted.Reserve(GGaps.Num());
			for (const TPair<FString, FGapRecord>& Pair : GGaps)
			{
				Sorted.Emplace(Pair.Key, Pair.Value);
			}
		}
		Sorted.Sort([](const TPair<FString, FGapRecord>& A, const TPair<FString, FGapRecord>& B)
		{
			return A.Value.Count > B.Value.Count;
		});

		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("StarryUEZh"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory, TEXT("gaps.tsv"));

		FString Output = TEXT("# count\tsource\twindow\n");
		for (const TPair<FString, FGapRecord>& Pair : Sorted)
		{
			Output += FString::Printf(TEXT("%d\t%s\t%s\n"), Pair.Value.Count, *Pair.Key, *Pair.Value.Window);
		}
		FFileHelper::SaveStringToFile(Output, *Path);
		UE_LOG(LogStarryUEZh, Log, TEXT("缺口清单：%d 条 -> %s"), Sorted.Num(), *Path);
	}

	void HarvestStaticImpl()
	{
		TMap<FString, FString> Harvested;
		auto Add = [&Harvested](const FString& Text, const TCHAR* Category)
		{
			if (LooksHarvestable(Text))
			{
				Harvested.FindOrAdd(Text) = Category;
			}
		};

		int32 MaterialCount = 0;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (!Class->IsChildOf(UMaterialExpression::StaticClass())
				|| Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				continue;
			}
			if (UMaterialExpression* CDO = Cast<UMaterialExpression>(Class->GetDefaultObject()))
			{
				TArray<FString> Captions;
				CDO->GetCaption(Captions);
				for (const FString& Caption : Captions)
				{
					Add(Caption, TEXT("MaterialExpression"));
				}
				Add(CDO->GetDescription(), TEXT("MaterialExpression"));
				Add(CDO->GetKeywords().ToString(), TEXT("MaterialKeywords"));
				++MaterialCount;
			}
		}

		// Material function input / output pins are stored inside the function
		// assets, not in reflection data. Load every function and collect the
		// names shown on call nodes.
		int32 FunctionInputCount = 0;
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
			TArray<FAssetData> FunctionAssets;
			AssetRegistry.GetAssetsByClass(MakeAssetClassQuery(UMaterialFunction::StaticClass()), FunctionAssets, true);
			for (const FAssetData& Asset : FunctionAssets)
			{
				UMaterialFunction* Function = Cast<UMaterialFunction>(Asset.GetAsset());
				if (!Function)
				{
					continue;
				}
				TArray<FFunctionExpressionInput> Inputs;
				TArray<FFunctionExpressionOutput> Outputs;
				Function->GetInputsAndOutputs(Inputs, Outputs);
				for (const FFunctionExpressionInput& Input : Inputs)
				{
					if (Input.ExpressionInput)
					{
						Add(Input.ExpressionInput->InputName.ToString(), TEXT("MaterialFunctionInput"));
						Add(Input.ExpressionInput->Description, TEXT("MaterialFunctionInputTooltip"));
						++FunctionInputCount;
					}
				}
				for (const FFunctionExpressionOutput& Output : Outputs)
				{
					if (Output.ExpressionOutput)
					{
						Add(Output.ExpressionOutput->OutputName.ToString(), TEXT("MaterialFunctionOutput"));
						++FunctionInputCount;
					}
				}
			}
			UE_LOG(LogStarryUEZh, Log, TEXT("材质函数引脚扫描：%d 个资产，%d 个输入/输出"), FunctionAssets.Num(), FunctionInputCount);
		}

		// Animation blueprint state machine states and transitions are graph
		// nodes inside the AnimBP asset, so a reflection pass never sees them.
		// Load each blueprint and walk its state machine graphs.
		int32 AnimStateCount = 0;
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
			TArray<FAssetData> AnimAssets;
			AssetRegistry.GetAssetsByClass(MakeAssetClassQuery(UAnimBlueprint::StaticClass()), AnimAssets, true);
			for (const FAssetData& Asset : AnimAssets)
			{
				UAnimBlueprint* Blueprint = Cast<UAnimBlueprint>(Asset.GetAsset());
				if (!Blueprint)
				{
					continue;
				}
				ForEachObjectWithPackage(Blueprint->GetPackage(), [&Add, &AnimStateCount](UObject* Object)
				{
					UAnimationStateMachineGraph* Graph = Cast<UAnimationStateMachineGraph>(Object);
					if (!Graph)
					{
						return true;
					}
					for (UEdGraphNode* Node : Graph->Nodes)
					{
						if (const UAnimStateNodeBase* StateNode = Cast<UAnimStateNodeBase>(Node))
						{
							Add(StateNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString(), TEXT("AnimState"));
							++AnimStateCount;
						}
					}
					return true;
				}, true, RF_NoFlags, EInternalObjectFlags::None);
			}
			UE_LOG(LogStarryUEZh, Log, TEXT("动画蓝图状态扫描：%d 个蓝图，%d 个状态/过渡"), AnimAssets.Num(), AnimStateCount);
		}

		// Skeleton bone names are identifiers, but the skeleton tree is a
		// display surface. Collect them so users can opt into a display-only
		// translation table; the underlying FName is never changed.
		int32 BoneCount = 0;
		{
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
			IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
			TArray<FAssetData> SkeletonAssets;
			AssetRegistry.GetAssetsByClass(MakeAssetClassQuery(USkeleton::StaticClass()), SkeletonAssets, true);
			for (const FAssetData& Asset : SkeletonAssets)
			{
				USkeleton* Skeleton = Cast<USkeleton>(Asset.GetAsset());
				if (!Skeleton)
				{
					continue;
				}
				const FReferenceSkeleton& ReferenceSkeleton = Skeleton->GetReferenceSkeleton();
				for (int32 BoneIndex = 0; BoneIndex < ReferenceSkeleton.GetNum(); ++BoneIndex)
				{
					Add(ReferenceSkeleton.GetBoneName(BoneIndex).ToString(), TEXT("BoneName"));
					++BoneCount;
				}
			}
			UE_LOG(LogStarryUEZh, Log, TEXT("骨骼扫描：%d 个骨架，%d 个骨骼"), SkeletonAssets.Num(), BoneCount);
		}

		int32 FunctionCount = 0;
		for (TObjectIterator<UFunction> It; It; ++It)
		{
			UFunction* Function = *It;
			const FString OwnerName = Function->GetOuter() ? Function->GetOuter()->GetName() : FString();
#if ENGINE_MAJOR_VERSION > 5 || ENGINE_MINOR_VERSION >= 5
			Add(ObjectTools::GetUserFacingFunctionName(Function, true).ToString(), TEXT("UFunction"));
#else
			Add(UK2Node_CallFunction::GetUserFacingFunctionName(Function).ToString(), TEXT("UFunction"));
#endif
			Add(Function->GetDisplayNameText().ToString(), *FString::Printf(TEXT("UFunctionDisplayName:%s"), *OwnerName));
			Add(Function->GetToolTipText().ToString(), *FString::Printf(TEXT("UFunctionTooltip:%s"), *OwnerName));
			++FunctionCount;
		}

		int32 EnumCount = 0;
		for (TObjectIterator<UEnum> It; It; ++It)
		{
			UEnum* Enum = *It;
			for (int32 Index = 0; Index < Enum->NumEnums(); ++Index)
			{
				Add(Enum->GetDisplayNameTextByIndex(Index).ToString(),
					*FString::Printf(TEXT("UEnum:%s"), *Enum->GetName()));
			}
			++EnumCount;
		}

		int32 PropertyCount = 0;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			for (TFieldIterator<FProperty> PropIt(*It); PropIt; ++PropIt)
			{
				Add(PropIt->GetDisplayNameText().ToString(),
					*FString::Printf(TEXT("Property:%s"), *It->GetName()));
				Add(PropIt->GetToolTipText().ToString(),
					*FString::Printf(TEXT("PropertyTooltip:%s"), *It->GetName()));
				++PropertyCount;
			}
		}

		// Component / actor class names shown by the Blueprint "Add Component"
		// menu come from UClass display names, which the other passes miss.
		int32 ClassCount = 0;
		for (TObjectIterator<UClass> It; It; ++It)
		{
			UClass* Class = *It;
			if (Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
			{
				continue;
			}
			Add(Class->GetDisplayNameText().ToString(),
				*FString::Printf(TEXT("UClass:%s"), *Class->GetName()));
			++ClassCount;
		}

		// Long descriptions and tooltips live inside asset packages, not in the
		// reflection display names. Pull every sentence-like string out of the
		// Niagara and material function content so the AI pass can translate them.
		auto HarvestAssetDescriptions = [&Harvested](const FString& Root, const TCHAR* Category, double Deadline)
		{
			if (!IFileManager::Get().DirectoryExists(*Root))
			{
				return;
			}
			TArray<FString> Files;
			IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.uasset"), true, false);
			int32 Scanned = 0;
			const int32 MaxFiles = 3000;
			for (const FString& File : Files)
			{
				if (++Scanned > MaxFiles)
				{
					UE_LOG(LogStarryUEZh, Warning, TEXT("描述扫描达到上限（%d 个文件），其余跳过：%s"), MaxFiles, *Root);
					break;
				}
				if (FPlatformTime::Seconds() > Deadline)
				{
					UE_LOG(LogStarryUEZh, Log, TEXT("描述扫描超时，跳过剩余文件：%s"), *Root);
					break;
				}
				if (File.Contains(TEXT("/Intermediate/")) || File.Contains(TEXT("/Saved/")))
				{
					continue;
				}
				if (IFileManager::Get().FileSize(*File) > 8 * 1024 * 1024)
				{
					continue;
				}

				TArray<uint8> Data;
				if (!FFileHelper::LoadFileToArray(Data, *File))
				{
					continue;
				}

				FString Current;
				Current.Reserve(256);
				const FString FileCategory = FString::Printf(TEXT("%s:%s"), Category, *FPaths::GetBaseFilename(File));
				auto Consider = [&Harvested, &FileCategory](const FString& Text)
				{
					if (Text.Len() < 15 || Text.Len() > 400 || !LooksHarvestable(Text))
					{
						return;
					}
					int32 Spaces = 0;
					int32 Letters = 0;
					for (const TCHAR C : Text)
					{
						Spaces += C == TEXT(' ') ? 1 : 0;
						Letters += FChar::IsAlpha(C) ? 1 : 0;
					}
					if (Spaces < 3 || Letters * 100 < Text.Len() * 80)
					{
						return;
					}
					if (!FChar::IsUpper(Text[0]) || Text.Contains(TEXT("Default value for")))
					{
						return;
					}
					Harvested.FindOrAdd(Text) = FileCategory;
				};

				// Niagara module assets store parameter names as fully qualified
				// strings ("Module.Noise Strength"). The UI shows the leaf name,
				// so collect every spaced leaf that looks like a display name.
				auto ConsiderParameter = [&Harvested](const FString& Text)
				{
					static const TCHAR* Prefixes[] = {
						TEXT("Local.Module."), TEXT("Output.Module."), TEXT("Module."),
						TEXT("Particles."), TEXT("Emitter."), TEXT("System."),
						TEXT("Transient."), TEXT("Engine.")
					};
					for (const TCHAR* Prefix : Prefixes)
					{
						if (!Text.StartsWith(Prefix, ESearchCase::CaseSensitive))
						{
							continue;
						}
						const FString Leaf = Text.Mid(FCString::Strlen(Prefix));
						if (Leaf.Len() < 3 || Leaf.Len() > 80
							|| !Leaf.Contains(TEXT(" "))
							|| !FChar::IsUpper(Leaf[0])
							|| !LooksHarvestable(Leaf))
						{
							return;
						}
						Harvested.FindOrAdd(Leaf) = TEXT("NiagaraParameter");
						return;
					}
				};

				for (const uint8 Byte : Data)
				{
					if (Byte >= 32 && Byte < 127)
					{
						Current.AppendChar(static_cast<TCHAR>(Byte));
					}
					else if (!Current.IsEmpty())
					{
						Consider(Current);
						ConsiderParameter(Current);
						Current.Reset();
					}
				}
				if (!Current.IsEmpty())
				{
					Consider(Current);
					ConsiderParameter(Current);
				}
			}
		};

		const double StartTime = FPlatformTime::Seconds();
		HarvestAssetDescriptions(
			FPaths::Combine(FPaths::EnginePluginsDir(), TEXT("FX/Niagara/Content")),
			TEXT("NiagaraDescription"), StartTime + 8.0);
		HarvestAssetDescriptions(
			FPaths::Combine(FPaths::EngineContentDir(), TEXT("Functions")),
			TEXT("MaterialDescription"), StartTime + 12.0);
		HarvestAssetDescriptions(FPaths::EngineContentDir(), TEXT("EngineDescription"), StartTime + 16.0);
		HarvestAssetDescriptions(FPaths::ProjectContentDir(), TEXT("ProjectDescription"), StartTime + 24.0);
		HarvestAssetDescriptions(FPaths::ProjectPluginsDir(), TEXT("ProjectPluginDescription"), StartTime + 28.0);

		// Enabled plugins can add their own editor UI and descriptions.
		const double PluginDeadline = StartTime + 40.0;
		IPluginManager& PluginManager = IPluginManager::Get();
		for (const TSharedRef<IPlugin>& Plugin : PluginManager.GetEnabledPlugins())
		{
			if (FPlatformTime::Seconds() > PluginDeadline)
			{
				UE_LOG(LogStarryUEZh, Log, TEXT("插件描述扫描超时，跳过其余插件"));
				break;
			}
			const FString ContentDir = Plugin->GetContentDir();
			if (ContentDir.IsEmpty() || Plugin->GetName() == TEXT("StarryUEZh")
				|| !IFileManager::Get().DirectoryExists(*ContentDir))
			{
				continue;
			}
			const FString Category = FString::Printf(TEXT("PluginDescription:%s"), *Plugin->GetName());
			HarvestAssetDescriptions(ContentDir, *Category, PluginDeadline);
		}

		TArray<TPair<FString, FString>> Sorted;
		Sorted.Reserve(Harvested.Num());
		for (const TPair<FString, FString>& Pair : Harvested)
		{
			Sorted.Emplace(Pair.Key, Pair.Value);
		}
		Sorted.Sort([](const TPair<FString, FString>& A, const TPair<FString, FString>& B)
		{
			return A.Value < B.Value || (A.Value == B.Value && A.Key < B.Key);
		});

		const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("StarryUEZh"));
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = FPaths::Combine(Directory, TEXT("harvest.tsv"));

		FString Output = TEXT("# category\tsource\n");
		for (const TPair<FString, FString>& Pair : Sorted)
		{
			Output += FString::Printf(TEXT("%s\t%s\n"), *Pair.Value, *Pair.Key);
		}
		FFileHelper::SaveStringToFile(Output, *Path);
		UE_LOG(LogStarryUEZh, Log, TEXT("静态采词：材质 %d，函数 %d，枚举 %d，属性 %d，唯一 %d 条 -> %s"),
			MaterialCount, FunctionCount, EnumCount, PropertyCount, Sorted.Num(), *Path);
	}
}

namespace StarryHooks
{
	bool Install()
	{
		bool bAny = false;

		bAny |= InstallOne(TEXT("Slate"), Export_SetText,
			reinterpret_cast<void*>(&Hook_SetText), reinterpret_cast<void**>(&Real_SetText), TEXT("FPlainTextLayoutMarshaller::SetText"));
		bAny |= InstallOne(TEXT("Slate"), Export_RichSetText,
			reinterpret_cast<void*>(&Hook_RichSetText), reinterpret_cast<void**>(&Real_RichSetText), TEXT("FRichTextLayoutMarshaller::SetText"));
		bAny |= InstallOne(TEXT("Slate"), Export_ComputeDesiredSize,
			reinterpret_cast<void*>(&Hook_ComputeDesiredSize), reinterpret_cast<void**>(&Real_ComputeDesiredSize), TEXT("FSlateTextBlockLayout::ComputeDesiredSize"));
		bAny |= InstallOne(TEXT("SlateCore"), Export_MeasureText,
			reinterpret_cast<void*>(&Hook_MeasureText), reinterpret_cast<void**>(&Real_MeasureText), TEXT("FSlateFontMeasure::Measure(FText)"));
		bAny |= InstallOne(TEXT("SlateCore"), Export_MakeText,
			reinterpret_cast<void*>(&Hook_MakeText), reinterpret_cast<void**>(&Real_MakeText), TEXT("FSlateDrawElement::MakeText"));
		bAny |= InstallOne(TEXT("Engine"), Export_UpdateSearchData,
			reinterpret_cast<void*>(&Hook_UpdateSearchData), reinterpret_cast<void**>(&Real_UpdateSearchData), TEXT("FEdGraphSchemaAction::UpdateSearchData"));

		LogSetTextVTable();

		InstallNiagaraHooks();

		return bAny;
	}

	void Uninstall()
	{
		if (InstalledHooks.Num() > 0)
		{
			DetourTransactionBegin();
			DetourUpdateThread(GetCurrentThread());
			for (const FHookRecord& Record : InstalledHooks)
			{
				DetourDetach(reinterpret_cast<PVOID*>(Record.RealSlot), Record.Hook);
			}
			DetourTransactionCommit();
			InstalledHooks.Reset();
		}

		Real_SetText = nullptr;
		Real_RichSetText = nullptr;
		Real_ComputeDesiredSize = nullptr;
		Real_MeasureText = nullptr;
		Real_MakeText = nullptr;
		Real_UpdateSearchData = nullptr;
		Real_NiagaraMatch = nullptr;
		Real_NiagaraWeight = nullptr;
		Real_NiagaraWeightLegacy = nullptr;
		bNiagaraHooksInstalled = false;
	}

	void RequestRefresh()
	{
		if (!FSlateApplication::IsInitialized())
		{
			return;
		}

		static int32 GRefreshTicks = 0;
		bRefreshPending.store(true, std::memory_order_relaxed);
		GRefreshTicks = 10;
		FSlateApplicationBase::Get().InvalidateAllWidgets(true);

		FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([](float)
		{
			if (FSlateApplication::IsInitialized())
			{
				FSlateApplicationBase::Get().InvalidateAllWidgets(true);
			}
			if (--GRefreshTicks <= 0)
			{
				bRefreshPending.store(false, std::memory_order_relaxed);
				if (FSlateApplication::IsInitialized())
				{
					FSlateApplicationBase::Get().InvalidateAllWidgets(true);
				}
				return false;
			}
			return true;
		}), 0.2f);
	}

	bool HasNiagaraHooks()
	{
		return bNiagaraHooksInstalled;
	}

	void GetSetTextStats(int64& OutCalls, int64& OutHits)
	{
		OutCalls = GSetTextCalls;
		OutHits = GSetTextHits;
	}

	void DumpGaps()
	{
		DumpGapsImpl();
	}

	void HarvestStatic()
	{
		HarvestStaticImpl();
	}
}
