# 动效任务提示词（**已归档** · 动效已完成）

> **状态：已完成，本文件仅作历史记录。** 动效制作已于 2026-09 结束，成果提交见
> `docs/动效交接.md`（权威交接记录）；验收记录见 `docs/动效验收清单.md`；
> 设计与节拍表见 `docs/动效设计.md`；回迁注意见 `docs/回迁标注.md` 第 16 / 19 / 20 条。
> 下方内容保留为"当时交给动效实例的任务书"，**不要再据此新开动效轮次**。

---

## 任务：为 ExcelSearch Qt 原型（V0.3.1.5）制作全局控件动效

你是**动效专班的实现实例**。目标：给这个功能已完备的 Qt 原型**加一层克制、跟手的控件动效**，
让它从"能用"变成"有质感"，同时**不改变任何业务行为**。

### 1. 项目事实（照这里给的做，不用重新调研）

- 仓库：`<prototype>`（git，起始提交 `583087d`，工作区干净）
- 代码：单文件 UI `main.cpp`（2510 行）+ `CMakeLists.txt`；文档 `README.md`、`docs/回迁标注.md`、`docs/缓存验证.md`
- 定位：**实验/方向验证原型**，成果最终要迁移回 `<workspace>`（V0.3.0，Win32 主程序），
  迁移后正式命名 **V0.3.2**
- 视觉基准：MAA(MaaWpfGui) 风格 —— 科技蓝强调色 `#326cf3`、深色面板 `#17171C`/卡片 `#24242B`、
  浅色面板 `#F5F6FA`/卡片 `#FFFFFF`、圆角、微软雅黑、无边框自绘窗口
- 技术栈：Qt6 Widgets + QSS，MinGW(MSYS2 g++ 16.1) + Ninja，**GUI 子系统**
- 共享 core（**只读，绝对不要改**）：`<repo-v0.3.0>`（search_engine/各 reader/xse 加密/pinyin/file_io/thirdparty）

### 2. 环境与构建（照抄）

```powershell
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
cmake -G Ninja -S <prototype> -B <prototype>\build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe
cmake --build <prototype>\build
```

**三个必踩的坑（先读）**：
1. **构建前必须确保没有 `ui_proto.exe` 在运行**，否则链接报 `Permission denied`：
   `Get-Process ui_proto -ErrorAction SilentlyContinue | Stop-Process -Force`
2. 程序是 **GUI 子系统**（不分配控制台），因此 **PowerShell 的 `&` 不会等待进程结束** ——
   所有自检/截图调用必须写成 `Start-Process -FilePath $exe -ArgumentList @(...) -Wait -NoNewWindow`，
   否则会读到"文件还没写完"的中间态。
3. 存在**单实例闸门**：若已有实例在跑，自检钩子会输出 `error=another-instance-running` 并退出。

### 3. 自检钩子（你的验证手段）

无头运行请设 `QT_QPA_PLATFORM=offscreen`；结果**写文件**（GUI 子系统无 stdout）：

| 钩子 | 作用 |
|---|---|
| `--report <file>` | 输出规模与状态：`files= entries= skipped= cache= blockedEntries= blockedFiles= marked= hist= histShow= histTtl= filterMode= shareMode= sharePath= tray= closeAction=` |
| `--search <关键词> --out <file>` | 搜索命中：`hits= blocked= first=` |
| `--search <一级> --filter <二级> --out <file>` | 二级筛选（逗号分隔可连筛）：`filtered[词]=N` |
| `--colprobe <列名> --out <file>` | 智能列名解析 |
| `--shot <png> [--page settings\|adv\|closedlg] [--sec N] [--advsec N] [--tab N] [--dark\|--light] [--kw 词]` | 离屏截图 |
| `--exportdemo <xlsx>` / `--block` / `--mark` / `--share` / `--shareoff` / `--clearmarks` | 其它自检 |

**基线（必须逐字段保持一致）**：
```
files=5  entries=43846  skipped=1  blockedEntries=7  blockedFiles=0  marked=1
histShow=5  histTtl=0  filterMode=standard  shareMode=off  closeAction=ask
搜索「工日」→ hits=54 blocked=7
```
**绝对不要运行 `--clearmarks`** —— 会清掉用户配置里的 7 条屏蔽与 1 条标记。
需要动配置的测试请先 `Copy-Item config.ini config.ini.bak`，测完还原。

### 4. 代码结构要点（动效挂在哪）

- 主题令牌：`makeProto(bool dark, const QColor& accent)` → `Proto{ accent, panelBg, card, hover, pressed,
  text, sub, editBg, editBorder, border, altRow, headerBg, closeHover }`；
  `qssFor(p)` 生成全量样式表；`apply()` 应用样式并刷新**自绘控件**
- 注册表驱动：`m_pages`(顶部标签) / `m_cards`(搜索页卡片) / `m_secs`(设置分区) / `m_advSecs`(高级设置子分区) /
  `m_utilTools`(预留未启用)
- 自绘控件（动效重点对象）：`WinBtn`(标题栏三键)、`FolderBtn`(文件夹图标)、`MarkBarDelegate`(行首标记色条)、
  `#toast` 提示气泡；QSS 控件：`QPushButton#primaryBtn|themeBtn|navBtn|themeToggle|tbBtn`、
  `QListWidget`(设置分区列表)、`QTableWidget`(结果表)、`QCheckBox/QRadioButton` 指示器
- 现有页面：搜索页（搜索行 + 二级筛选行 + 数据概览卡片 + 搜索结果卡片）、设置页 8 个分区
  （通用设置/搜索设置/搜索历史/加密设置/共享设置/智能列设置/高级设置/关于我们）、高级设置解锁层、关闭对话框

### 5. 要做的动效（范围）

之前规划里明确列过：**hover 动效 / 页面切换动效 / 可折叠面板**

1. **hover / 按下反馈**：按钮、设置列表项、结果表行、托盘菜单 —— 颜色过渡（用 `QVariantAnimation`
   驱动自绘控件的颜色插值，或 `QPropertyAnimation`）
2. **页面/分区切换过渡**：顶部 Tab 切换、设置分区切换 —— 淡入 + 轻微位移，**克制**（150–220ms，`OutCubic`）
3. **可折叠面板**：搜索页两张卡片（数据概览 / 搜索结果）做成可折叠（点标题展开/收起，高度动画）
4. 其它你判断值得的：toast 淡入淡出+上浮、解锁层淡入、卡片 hover 轻微抬升、结果表行 hover 等

### 6. 硬约束（违背即失败）

1. **必须提供"关闭动效"的开关，且无头自检能用** —— 否则截图会拍到动画中间态、验证变随机失败。
   要求：环境变量（推荐 `UI_PROTO_NO_ANIM=1`）或 CLI 参数，**在动画启动前即生效**（时长置 0 / 直接落到终态），
   并在 README 写清用法。
2. **不改任何业务行为**：搜索 / 筛选（标准·逐级）/ 屏蔽 / 标记 / 缓存（`cache.dat`+`cache.inv`）/ 历史 /
   共享模式 / 单实例 / 托盘 / 门禁 —— 行为、持久化格式、`--report` 现有字段含义**必须与现在完全一致**
   （可新增字段，不可改既有字段含义）。
3. **不要动**：`<repo-v0.3.0>` 下任何文件；`CMakeLists.txt` 里的 `WIN32_EXECUTABLE TRUE`
   与 `Qt6::Network` 链接；单实例闸门代码。
4. **不要给顶级无边框窗口做几何动画**：它开了 `WA_TranslucentBackground`，Win10 下做尺寸/位置动画会闪
   （历史已确认，用户选择"质感优先、接受静止"）。动效**只作用在子控件**上。
5. **不引入新依赖 / 新 Qt 模块**（现有仅 Widgets + Network）；**不要引入 QML/QtQuick**。
6. **主题一致性**：动效颜色必须取自 `Proto` 令牌，不要硬编码颜色；**深浅色 + 5 种强调色下都要正常**。
7. 代码风格沿用现状：中文注释、需要时标 `★回迁注意`；以单文件为主，若新增文件必须同步改 `CMakeLists.txt`
   并在 README 说明。
8. 提交粒度小、中文提交信息；**每个小步都要 build + 自检回归**。

### 7. 验收标准

- 构建通过；无新增警告
- **基线自检逐字段一致**（见第 3 节），搜索 `工日` 仍为 `hits=54 blocked=7`
- 截图对比：搜索页 / 设置页（通用设置、搜索设置、共享设置）/ 高级设置解锁层 / 关闭对话框，
  **浅色 + 深色**下都正常；加 `UI_PROTO_NO_ANIM=1` 后截图应与现在一致或仅有预期内变化
- 手验：hover 跟手不卡；切换页面不闪不抖；折叠面板展开收起顺滑

### 8. 完成后按此格式回报（主会话据此接续）

1. **提交清单**：起始 `583087d` → 结束 `<hash>`，每条一句话
2. **改动概览**：改了哪些文件、新增了什么（特别是**关闭动效的开关名与取值**）
3. **动效清单**：每条动效 → 作用对象 → 时长/easing → 涉及文件
4. **截图/自检用法变化**：在现有钩子之上需要额外加什么参数或环境变量
5. **回归结果**：`--report` 与 `--search 工日` 的**完整输出**（证明业务未变）
6. **回迁影响**：是否需要更新 `docs/回迁标注.md`（尤其第 16 条"控件动效归属"）与 README
7. **遗留/未做**：判断该做但没做的，及原因

### 9. 边界与协调

- **本仓库在你工作期间由你独占**：主会话（负责回迁准备）不会同时改它
- 不要 `git push`（无远端）；不要 rebase/amend 既有提交；不要删 `docs/` 下文档
- 用户配置：`%APPDATA%\ui_proto\ExcelSearch\config.ini`（7 条屏蔽 + 1 条标记 + 深色 + `closeAction=ask`）
  —— 测试前备份、测试后还原
- 数据目录 `build\bin\data`（6 个文件：4 个非 xse + 2 个 xse）也不要动
