<p align="center">
  <img src="Resources/Icon128.png" width="120" alt="星空汉化">
</p>

<h1 align="center">UnrealEngine-Chinese</h1>

<p align="center">虚幻引擎 5.0 到 5.8 编辑器汉化插件 · 星空汉化</p>

<p align="center">
  蓝图 · 材质 · Niagara · 动画蓝图 · 细节面板 · 菜单
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Unreal%20Engine-5.0%20%7C%205.1%20%7C%205.2%20%7C%205.3%20%7C%205.4%20%7C%205.5%20%7C%205.6%20%7C%205.7%20%7C%205.8-313131?logo=unrealengine&logoColor=white" alt="Unreal Engine">
  <img src="https://img.shields.io/badge/Platform-Windows-0078D6?logo=windows&logoColor=white" alt="Windows">
  <img src="https://img.shields.io/badge/Code-MIT-green" alt="MIT">
  <img src="https://img.shields.io/badge/Dictionary-CC%20BY--SA%204.0-orange" alt="CC BY-SA 4.0">
</p>

给虚幻引擎编辑器做的中文显示插件。蓝图、材质、Niagara、动画蓝图里的节点标题、引脚、参数、细节面板、菜单和提示都会显示中文，支持中英切换和双语搜索。不修改源码、资产名、资产路径，不影响游戏打包。

![汉化面板](Docs/panel.png)

## 功能

- 蓝图、材质、Niagara、动画蓝图节点标题、引脚和参数中文化
- 细节面板、菜单、工具栏、提示文字、富文本提示框中文化
- F1 一键切换中文 / 英文
- 节点搜索支持中文和英文同时匹配
- 汉化面板：扫描词条、收集未翻译文字、手动编辑、导出清单
- 内置 DeepSeek 翻译，粘贴 API Key 即可批量翻译
- 固定译法术语表，统一 Solver、POM、Age、LODDistanceFraction 等术语
- 每次修改词库自动重载，不用重启编辑器

## 下载

**v0.4.4 安装包**（含预编译 DLL，开箱即用，支持 UE 5.0 – 5.8）：

到 [Releases v0.4.4](https://github.com/Ker0el/UnrealEngine-Chinese/releases/tag/v0.4.4) 按引擎版本下载：

| 引擎版本 | 安装包 |
|---|---|
| UE 5.0 | `StarryUEZh_v0.4.4_UE5.0.zip` |
| UE 5.1 | `StarryUEZh_v0.4.4_UE5.1.zip` |
| UE 5.2 | `StarryUEZh_v0.4.4_UE5.2.zip` |
| UE 5.3 | `StarryUEZh_v0.4.4_UE5.3.zip` |
| UE 5.4 | `StarryUEZh_v0.4.4_UE5.4.zip` |
| UE 5.5 | `StarryUEZh_v0.4.4_UE5.5.zip` |
| UE 5.6 | `StarryUEZh_v0.4.4_UE5.6.zip` |
| UE 5.7 | `StarryUEZh_v0.4.4_UE5.7.zip` |
| UE 5.8 | `StarryUEZh_v0.4.4_UE5.8.zip` |

同一个 Release 里还有 `StarryUEZh_v0.4.4_UE5.x_Source.zip`，那是不带 DLL 的纯源码包，供 Fab 审核和自行编译的开发者使用。

每个安装包是同一个插件，只有 `.uplugin` 里的 `EngineVersion` 和 `Binaries\Win64` 里的 DLL 不同，不能混用。安装包内含预编译模块和完整源码，不需要自己编译。

> 在 Fab 购买的用户不用下载这里的包：在 Fab 库里选中引擎版本安装即可，Fab 会在服务器端按版本编译好。

> 旧版本 `v0.4.2` 是纯源码包（不含 DLL），只适合愿意自行编译的开发者。

## 安装

1. 解压压缩包，把 `StarryUEZh` 文件夹放到项目的 `Plugins` 目录：

```
<你的项目>\Plugins\StarryUEZh
```

   也可以放到引擎插件目录 `Engine\Plugins\Marketplace\StarryUEZh`，对所有项目生效。安装包自带对应版本的 DLL，两种位置都能直接用。

2. 打开项目，在「编辑 > 插件」里启用 **星空汉化**，重启编辑器。
3. 插件依赖引擎自带的 **Niagara** 插件，确保它是启用状态。

如果提示「模块缺失或引擎版本不同」，说明下载的包和引擎版本不一致，或者用的是不含 DLL 的源码包。请换成 `v0.4.4` 里与引擎大版本对应的安装包；源码包必须放到项目 `Plugins` 目录并用 Visual Studio 自行编译。

## 使用

1. 点工具栏上的星星按钮，或者按 `F1`，切换中文 / 英文。
2. 右键星星按钮，点「打开汉化面板」。
3. 点「扫描词条」，等它扫完。这一步不用打开窗口。
4. 打开你想汉化的窗口，把折叠的菜单和下拉框展开一次。
5. 回到面板，点「收集未翻译文字」，新出现的词会列在下面。
6. 选中一条，填中文，点「保存词条」，马上生效。
7. 不想手动翻，就用面板上的 AI：填 Key、选模型、点开始翻译。

## 扫描范围

**不用打开窗口就能扫到：**

- 材质函数输入 / 输出引脚
- Niagara 模块参数
- 动画蓝图状态机状态和过渡
- 骨骼名
- 类名、函数名、枚举、属性名和提示
- 资产里的说明文字

**需要打开窗口显示一次：**

- 第三方插件自己绘制的界面
- 运行时拼接的文字
- Canvas 直接绘制的文字

这些文字只要显示过一次，就会记进未翻译清单。点面板上的「收集未翻译文字」即可刷新。

## 骨骼名

骨骼名只替换显示，不改实际的 FName、动画、蒙皮和物理资产。骨骼树里看到中文，动画和代码里仍然是英文。

## 会影响打包吗

不会。插件是 Editor 模块，游戏目标不会编译也不会加载它。它不修改资产、资产名、路径和引擎文件，插件 Content 里只有 JSON 词库，没有 `.uasset`。

## 从源码编译

每个引擎版本用一个宿主工程编译：

```bash
UnrealBuildTool.exe StarryUEZhHostEditor Win64 Development ^
  -Project="<宿主工程>\StarryUEZhHost.uproject" -WaitMutex -NoHotReloadFromIDE
```

产物在 `Plugins\StarryUEZh\Binaries\Win64\UnrealEditor-StarryUEZh.dll`。

- UE 5.8 使用引擎自带的 Detours 模块。
- UE 5.3 到 5.7 使用 `Source\ThirdParty\Detours` 里的 Microsoft Detours v4.0.1。
- 插件依赖 `AnimGraph`、`NiagaraEditor`、`MaterialEditor`、`BlueprintGraph` 等编辑器模块。

## 已知限制

1. 只覆盖 Slate 显示层的文字。Canvas 直绘文字和第三方插件自绘界面可能保持原样。
2. 切换语言后，已经打开的材质 / 蓝图资产编辑器建议重新打开一次。
3. 极少数和界面词条同名的用户变量名可能被显示层替换，只影响显示，不影响数据。

## 许可证

- 代码：MIT，完整文本见仓库 [LICENSE](https://github.com/Ker0el/UnrealEngine-Chinese/blob/main/LICENSE)
- 词库：CC BY-SA 4.0，完整文本见仓库 [LICENSE-DICTIONARY](https://github.com/Ker0el/UnrealEngine-Chinese/blob/main/LICENSE-DICTIONARY)
- `Source/ThirdParty/Detours`：Microsoft Detours v4.0.1，MIT，完整许可证文本见 `NOTICE.md`
- 部分词条来自 Unreal Engine 自带的 zh-Hans 本地化资源，版权归 Epic Games 所有，详见 `NOTICE.md`

按 Fab 审核要求，分发给 Fab 的插件包里不带 `LICENSE` 和 `LICENSE-DICTIONARY`，许可证全文以上面的仓库链接为准。

## English

StarryUEZh is a Chinese localization plugin for the Unreal Engine 5.0 to 5.8 editor. It translates Blueprint, Material, Niagara and Animation Blueprint node titles, pins, parameters, details panels, menus and tooltips at the Slate display layer. It supports Chinese/English switching and bilingual node search, adds an in-editor scanning and translation panel with optional DeepSeek support, and never modifies source code, asset names, asset paths or packaged games.

Download the package matching your engine version from Releases. Code is MIT licensed; dictionary data is CC BY-SA 4.0.
