# StarryUEZh (星空汉化) English Documentation

StarryUEZh is a Chinese localization plugin for the Unreal Engine 5 editor. It displays Chinese text for Blueprint, Material, Niagara, and Animation Blueprint node titles, pins, parameters, details panels, menus, tooltips, and other editor UI. The original English text can be restored at any time with the F1 key.

The plugin works at the Slate display layer. It does not modify engine source code, project source code, asset names, asset paths, or packaged game data.

## Requirements

- Windows 10 or Windows 11, 64-bit.
- Unreal Engine 5.0 through 5.8, Editor build.
- The Niagara plugin must be enabled. It is enabled by default in most projects.
- A C++ toolchain is required only when Fab or the user rebuilds the plugin from source. Fab compiles the submitted source for each declared engine version.

## Supported engine versions

The same source compiles for Unreal Engine 5.0, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, and 5.8. Select the matching engine version when adding the product to your Fab library. Do not mix binary builds from different engine versions.

Engine differences handled by the source:

- UE 5.8 and later link the Detours module bundled with the engine.
- UE 5.0 through 5.7 link the Microsoft Detours v4.0.1 static library included in `Source/ThirdParty/Detours`.
- API differences in Slate, Asset Registry, Tool Menus, Niagara, and Material Editor are handled with engine-version branches.

## Installation

### Install into a project

1. Download the package for your engine version from Fab.
2. Extract the `StarryUEZh` folder into your project's `Plugins` folder:

   ```
   <YourProject>/Plugins/StarryUEZh
   ```

3. Open the project. In the editor, open **Edit > Plugins**, search for **星空汉化**, and enable it.
4. Restart the editor when prompted.

### Install into an engine

1. Extract the `StarryUEZh` folder into the engine's `Plugins` folder:

   ```
   <UE_5.x>/Engine/Plugins/StarryUEZh
   ```

2. Enable **星空汉化** in **Edit > Plugins** and restart the editor.

The plugin is an Editor module. It is not compiled or loaded by packaged games.

## Switching language

- Press **F1** anywhere in the editor to switch between Chinese and English.
- Click the star button on the editor toolbar to switch language.
- Right-click the star button to open the localization panel, reload dictionaries, or scan for text.
- The button label follows the current language: **切换中文 (F1)** in English mode, **切换英文 (F1)** in Chinese mode.

Language switching also works in Material, Niagara, Blueprint, Animation Blueprint, and other asset editors.

## What is translated

- Blueprint, Material, Niagara, and Animation Blueprint node titles, pins, and parameter names.
- Details panels, menus, toolbars, tooltips, and rich-text tooltips.
- Category names, enum labels, property names, function names, and asset descriptions.
- Material expression titles, including dynamic titles such as `Constant3Vector (0,0,0)` and `TexCoord[N]`.
- Niagara module names, parameter names, and palette categories.
- Animation Blueprint state machine state and transition titles.
- Bone names in the skeleton tree. Only the display name changes. The underlying FName, animation, skinning, and physics assets are not modified.

Node search matches both Chinese and English terms.

## Localization panel

Open the panel by right-clicking the toolbar star button and choosing **打开汉化面板**.

The panel provides:

- **Scan**: collects translatable text from the current project and engine.
- **Collect untranslated text**: adds text that has appeared in the editor but is not yet in the dictionary.
- **Search and filter**: search the dictionary and show only untranslated entries.
- **Manual editing**: select an entry, enter a translation, and save it immediately.
- **AI translation**: paste a DeepSeek API key, choose a model, and translate untranslated entries in batches. The key is stored in the Windows Credential Manager under `StarryUEZh/DeepSeek`, not in a project file.
- **Export**: export untranslated entries as a JSON file for external translation.

Dictionary changes are reloaded automatically. Restarting the editor is not required.

## Dictionary files

Dictionaries are stored in `Content/Dictionary/`. The main files are:

| File | Contents |
| --- | --- |
| `zh_cn.json` | Official English to Simplified Chinese editor localization pairs |
| `zh_cn_extra.json` | Additional editor and Niagara terminology |
| `zh_cn_material.json` | Material expression titles and roots |
| `zh_cn_material_funcs.json` | Material functions and material expression class names |
| `zh_cn_material_pins.json` | Material function input and output pins |
| `zh_cn_niagara.json` | Niagara modules, parameters, and palette categories |
| `zh_cn_niagara_attrs.json` | Niagara asset attribute leaf names |
| `zh_cn_niagara_params.json` | Niagara module parameters |
| `zh_cn_niagara_engine.json` | Niagara engine-provided properties |
| `zh_cn_bones.json` | Bone display names |
| `zh_cn_auto.json` | Generated UI text and composed terms |
| `glossary.json` | Fixed terminology overrides |
| `manual_zh.json` | User-created entries, highest priority |
| `notranslate.json` | Brand names and abbreviations that must stay in English |

The plugin scans `Content/Dictionary/*.json` for additional files, so new dictionaries do not require recompiling the plugin.

Load order and priority: main dictionaries are loaded first and existing keys are kept; `glossary.json` and `manual_zh.json` override earlier entries. `manual_zh.json` has the highest priority.

## Custom entries

Add entries to `Content/Dictionary/manual_zh.json`:

```json
{
  "entries": {
    "Your English Text": "你的中文译文"
  }
}
```

Save the file and the plugin reloads it within about two seconds. You can also add entries from the panel.

## Troubleshooting

**The plugin does not appear in the Plugins window.**
Check that the folder layout is `<Project>/Plugins/StarryUEZh/StarryUEZh.uplugin`. The `.uplugin` file must be directly inside the `StarryUEZh` folder.

**The editor reports a missing module or a different engine version.**
The package was built for a different engine version. Install the package that matches your engine version, or let Fab rebuild the source for the selected version.

**Some text remains English.**
The plugin translates Slate text at the display layer. Text drawn directly to a Canvas, text rendered by a third-party plugin with its own drawing pipeline, and text that is generated at runtime may remain English. Open the window once, then use **Collect untranslated text** in the panel to capture it.

**Some labels stay Chinese after switching to English.**
Material and Blueprint asset editors bake pin and node labels when the asset is opened. Close and reopen the asset editor after switching language.

**Very long descriptions are not translated.**
Long Niagara and Material descriptions are not included in the built-in dictionary. Use the panel's AI translation or add manual entries.

**The editor is slow on first launch.**
The first launch builds the dictionary index. Later launches load faster.

**Where are logs?**
Plugin log messages use the `LogStarryUEZh` category. They appear in the editor Output Log and in the project's `Saved/Logs` folder.

## Game text and fonts

- When you run the game inside the editor (PIE), Slate-rendered game text (UMG widgets, Slate-based 3D text plugins) is translated as well.
- If a game font has no CJK glyphs, the plugin adds `DroidSansFallback` to that font's composite font in memory, so translated text no longer renders as tofu boxes. Font assets are not modified; the fallback exists only for the current editor session.
- World-space `TextRenderComponent` text uses pre-baked glyph atlases and cannot gain new glyphs at runtime. The plugin does not translate it, so it stays in the game's original language.
- Packaged games and Standalone runs do not load the plugin and are never affected.

## Known limitations

- Only Slate display text is covered. Canvas-drawn text and third-party custom-drawn UI may stay unchanged.
- After switching language, reopen open Material and Blueprint asset editors.
- Fallback Chinese glyphs come from DroidSansFallback, so translated text does not keep the game font's artistic style.
- World-space `TextRenderComponent` text is not translated.
- The language switch only controls the plugin's own translations. If the editor itself is running in Chinese (engine language), engine text stays Chinese after switching to English.

## Packaging and shipping

- The plugin is an Editor module. Game targets do not compile or load it.
- The plugin contains JSON dictionaries and no `.uasset` files.
- The plugin does not modify asset names, asset paths, or engine files.
- No plugin files are included in packaged games.

## Licensing

- Plugin source code and configuration: MIT License. Full text: https://github.com/Ker0el/UnrealEngine-Chinese/blob/main/LICENSE
- Dictionary data: CC BY-SA 4.0. Full text: https://github.com/Ker0el/UnrealEngine-Chinese/blob/main/LICENSE-DICTIONARY
- `Source/ThirdParty/Detours`: Microsoft Detours v4.0.1, MIT License. The full license text is reproduced in `NOTICE.md`.
- Some dictionary entries are derived from the Simplified Chinese localization resources shipped with Unreal Engine. Unreal Engine and its bundled resources and trademarks are copyright Epic Games, Inc. This project is not affiliated with or endorsed by Epic Games. See `NOTICE.md`.

Per Fab technical review requirements, the plugin package distributed through Fab does not contain the `LICENSE` and `LICENSE-DICTIONARY` files; the repository links above are the authoritative license texts.

## AI usage

The shipped dictionaries are not generated by a generative AI model. They are assembled from the engine's localization resources, project scans, manually written entries, and rule-based composition.

The plugin includes an optional AI translation feature. It runs only when the user provides their own DeepSeek API key and clicks **Start translation**. Text selected for translation is sent to the DeepSeek API configured by the user. No text is sent without that action.

## Support

- Source code and issue tracker: https://github.com/Ker0el/UnrealEngine-Chinese
- Author: 星空汉化 (Ker0el)
- Bilibili: https://space.bilibili.com/177308205
