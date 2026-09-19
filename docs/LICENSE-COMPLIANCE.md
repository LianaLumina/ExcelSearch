# 开源合规核查记录（V0.3.2）

> 核查时间：2026-09-18（首次）、2026-09-19（复查：新增「使用说明书」弹窗）　核查人：开发会话（AI 辅助）
> 触发：用户要求确认"界面部分（含新增的使用说明书弹窗）是否直接使用了 MAA / MaaEnd 的代码"，
> 以决定发行许可与鸣谢措辞。

---

## 1. 背景与风险

本项目界面**视觉风格**参考 MAA（MaaWpfGui）。二者许可证均为 **AGPL-3.0**：

| 项目 | 许可证 | 依据 |
|---|---|---|
| MAA / MaaWpfGui | AGPL-3.0 | GitHub 仓库 license 字段实查 |
| MaaEnd | AGPL-3.0 | GitHub 仓库 license 字段实查 |

- **只借鉴视觉风格**（配色、圆角、布局语言）→ 不构成侵权，不触发 AGPL 传染；
- **抄入代码 / QSS / XAML / 资源** → 该部分须按 AGPL 处理（署名 + 相应源码义务）。
  （本项目最终采用 **GPL-3.0** 发行，与 AGPL-3.0 在 GPLv3 §13 下兼容；但**是否抄入仍必须查清**，
  否则鸣谢声明本身就是虚假陈述。）

## 2. 关键事实

- 开发期确实查阅过上游仓库源码（用于提取视觉语言）；参考检出为开发环境的临时副本，**未随本项目分发**。
- **技术栈不同**：MAA 的 GUI 是 **C# / XAML(WPF)**（327 个 `.cs` + `Res\Styles\*.xaml`），
  MaaEnd 是 **Go / C++**；本项目界面是 **C++ / Qt Widgets + QSS**。
  跨语言逐字搬运不可行，也不符合本项目界面的实际形态（QSS 与自绘控件全部为手写）。

## 3. 核查方法（四类指纹）

### 指纹 1：本项目自创标识符是否出现在 MAA/MaaEnd 源码

对 15 个本项目特有的类名 / 函数名 / 变量名做全仓检索，结果**全部未出现**：

```
WinBtn / MarkBarDelegate / FadeSlideEffect / StateTint / backDropOf / m_accentBtns /
CloseDialog / ConfirmDialog / PasswordDialog / DetailDialog / enableRowHoverAnim /
kFuzzySupplementMaxExact / ExcelSearchUiProto / settleAnim / CardHeader
```

> 若发生抄写，这类自造命名几乎必然同时出现——全部未命中是"未抄写"的强证据。

### 指纹 2：界面中文文案是否成段出现在 MAA/MaaEnd

对 8 条本项目界面文案检索：`数据概览 / 本次命中 / 已标记颜色 / 关闭选项设置 / 最小化到托盘 /
智能列设置 / 共享目录（UNC 路径） / 数据源为空`

- 仅 **"最小化到托盘"** 命中 MAA 的 `zh-cn.xaml` —— 属**通用中文界面用词**（Windows 通行的功能性表述），
  不构成受保护的独创性表达；
- 其余全部未出现。

### 指纹 3：样式层文件形态比对

- MAA 的样式为 `Res\Styles\*.xaml`（Button/CheckBox/ComboBox/DataGrid… 的 WPF 样式）；
- 本项目样式为 C++ 中手写的 **QSS 字符串**（`qssFor()`）与 `QPainter` 自绘控件，选择器/属性均按 Qt 体系编写。

### 附带发现：强调色

`#326cf3` 在 MAA 源码中出现（`Gui.cs` 的 `BackgroundMonetCustomColor` 默认值；
`BackgroundSettingsUserControlModel.cs` 取色回退值）。本项目强调色与之相同 →
说明**调色板系参考 MAA 视觉语言取得**。单个色值属事实性信息，不受著作权保护。

### 指纹 4：使用说明书弹窗（2026-09-19 新增功能复查）

本版的说明书弹窗在**版式与交互**上刻意对齐 MAA 的「公告」框，因此单独复核：

| 复核项 | MAA 原版 | 本项目 | 判定 |
|---|---|---|---|
| 界面技术 | C# / XAML(WPF)，`hc:Window` + `MdXaml` + Stylet | C++ / Qt Widgets + QSS，全部手写 | 无共享代码 |
| 标题栏按钮 | `ShowCloseButton="False"`、`ShowMinButton="False"`（只剩最大化） | 自绘：只有一个最大化/还原按钮 | 仅**行为一致**，实现不同 |
| 导航/正文 | `ListBox` 选项 + `mdxam:MarkdownScrollViewer` | `QListWidget` + `QTextBrowser`（`setMarkdown`） | 仅**行为一致**，实现不同 |
| 首项名称 | `ALL~ the Announcements` | 「全部内容」 | 文案不同 |
| 「不再提醒」复选框 | 「下次公告更新前不再显示」 | 「下次更新前不再展示」 | 文案不同（且由用户指定） |
| 未读完点确认的调侃 | 「没看完点什么确认」/「说的就是你，还点」/「还点」 | 「还没看完呢，往下翻翻～」/「后面还有内容，别急着关～」/「真的不看一下吗？就一点点～」 | 文案**完全不同** |
| 硬点 20 次以上放行 | `_notFinishedClick > 20` 后关闭 | 同阈值（`m_stubborn > 20`） | 仅**行为一致** |
| 读完才可勾选、读到底才能关 | `HasEverScrolledToBottom` 门禁 | 等价门禁（`m_readToBottom`） | 仅**行为一致** |

> 结论：说明书弹窗为**行为/版式层面的参考**，不含 MAA 的任何代码、XAML 或资源；
> 全部中文文案均为本项目自行撰写（上表逐条比对，无一条与 MAA 本地化文案相同）。
> 依著作权法，**思想与功能行为不受保护**，受保护的是具体表达——本项不存在表达层面的复制。

核查命令（可重跑，证明两者不是同一套表达）：

```powershell
# 本项目弹窗文案
Select-String -Path main.cpp -Pattern 'kManualNags|下次更新前不再展示'
# MAA 对应文案（应完全不同）
Select-String -Path <maa-ref>\src\MaaWpfGui\Res\Localizations\zh-cn.xaml -Pattern 'AnnouncementNotFinishedConfirm|DoNotRemindThisAnnouncementAgain'
```

## 4. 结论

1. **本程序未包含 MAA / MaaEnd 的任何源代码、QSS、XAML 或资源文件**；
2. 参考的是**视觉设计语言**（配色、圆角、深色面板、无边框布局）与**说明书弹窗的交互行为**，
   实现代码与全部文案均为本项目原创；
3. 因此：
   - 鸣谢措辞应写为**"界面视觉风格与弹窗交互方式参考自 MAA / MaaWpfGui 与 MaaEnd，未使用其任何代码或资源"**，
     **不得**写成"使用了其代码"；
   - 本项目可采用自选许可证（最终采用 **GPL-3.0**），不受 AGPL 传染条款约束；
   - 不使用 MAA / MaaEnd 的名称或 Logo 作为本产品名称或标识（当前产品名与图标均为本项目自有）。

## 5. 复核方法（可重跑）

```powershell
# 指纹 1：自创标识符核查
$mine = @('WinBtn','MarkBarDelegate','FadeSlideEffect','StateTint','backDropOf','CloseDialog','ConfirmDialog')
foreach ($id in $mine) {
  Get-ChildItem <maa-ref> -Recurse -File -Include *.cs,*.cpp,*.h,*.xaml,*.go,*.qss |
    Select-String -Pattern $id -SimpleMatch | Select-Object -First 1
}
# 指纹 3：确认 MAA 样式层为 XAML 而非 QSS
Get-ChildItem <maa-ref> -Recurse -File -Include *.qss | Measure-Object
```

## 6. 若将来需要引入 MAA 代码

则必须：① 明确标注来源文件与许可证（AGPL-3.0）；② 遵守其署名与源码义务；
③ 重新评估与本项目 GPL-3.0 的组合方式（GPLv3 §13）。当前**不存在**这种情况。
