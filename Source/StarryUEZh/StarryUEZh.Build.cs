// Copyright 2026 StarGame. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class StarryUEZh : ModuleRules
{
	public StarryUEZh(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		// Stay on the engine's default C++ standard: C++17 in 5.0-5.2, C++20 in 5.3+.
		// Forcing C++20 breaks UE 5.0 because NiagaraScript.h compares an
		// FNiagaraVariable with an FNiagaraVariableBase, which is ambiguous once
		// C++20 reversed comparison candidates are considered (C2666 in
		// GetCompilerTag). Engine 5.1 fixed that header; the flag was never needed.
		// ModuleRules.bWarningsAsErrors only exists from 5.3. The target rules
		// already set bWarningsAsErrors = false in every build host.

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Slate",
			"SlateCore",
			"ApplicationCore",
			"InputCore",
			"UnrealEd",
			"LevelEditor",
			"ToolMenus",
			"Projects",
			"AssetRegistry",
			"Json",
			"JsonUtilities",
			"HTTP",
			"BlueprintGraph",
			"Kismet",
			"GraphEditor",
			"MaterialEditor",
			"AnimGraph",
			"Niagara",
			"NiagaraEditor"
		});

		// The bundled Detours module only exists from UE 5.8. Older engines link
		// the same Microsoft Detours v4.0.1 static library from ThirdParty.
		if (Target.Version.MajorVersion > 5 || Target.Version.MinorVersion >= 8)
		{
			PrivateDependencyModuleNames.Add("Detours");
		}
		else
		{
			string DetoursDir = Path.Combine(ModuleDirectory, "..", "ThirdParty", "Detours");
			PublicSystemIncludePaths.Add(Path.Combine(DetoursDir, "include"));
			PublicAdditionalLibraries.Add(Path.Combine(DetoursDir, "lib", "detours.lib"));
		}

		PublicSystemLibraries.Add("Advapi32.lib");
	}
}
