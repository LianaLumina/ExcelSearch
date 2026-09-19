# Excel 表格关键字搜索工具（ExcelSearch）

把 Excel / Word / CSV 表格放进 `data` 目录，即可对**全部表格内容**做关键字检索，
配合二级筛选、屏蔽与标记，用来在成百上千份工程表格里快速定位目标行。

- **版本**：V0.3.2（Qt6 重写版；本仓库即为该版本源码）
- **作者 / 发行者**：觉心恋影
- **许可证**：**GNU GPL-3.0**（见 [LICENSE](LICENSE)）
- **界面**：Qt 6 Widgets + 自绘 QSS —— 无边框窗口、深浅双主题、可一键关闭的控件动效

---

## 界面预览

| 搜索页（浅色） | 设置页（深色） |
|---|---|
| ![搜索页](docs/images/搜索页-浅色.png) | ![设置页](docs/images/设置页-深色.png) |

> 截图由程序自带的离屏截图钩子生成（`--shot`），与运行时界面一致。

---

## 功能

| 功能 | 说明 |
|---|---|
| **多格式检索** | `.xlsx` / `.xls` / `.csv` / `.docx` / `.xse`（加密表格） |
| **三种搜索** | 精确搜索、**模糊搜索**（默认开启，作为精确结果的补充）、正则（前缀 `re:`） |
| **二级筛选** | 在当前结果里继续筛；两种模式可选：**标准**（每次从基准重算，与顺序无关）/ **逐级**（层层收窄） |
| **屏蔽 / 标记** | 可屏蔽整个文件或某一行；可给行打 5 种颜色标记，并支持 `已标记颜色` 检索 |
| **可配置智能列** | 结果表列数与列内容自定义（3–7 列），中间列可映射到任意工作簿表头列名 |
| **搜索历史** | 可配置保留条数与时效（10 分钟 ~ 12 小时 / 关闭程序后删除） |
| **数据源** | 离线（程序目录 `data\`）/ **共享模式**（UNC 共享目录，不可达时自动吃缓存） |
| **缓存** | 索引落盘（`cache.dat` + 全量清单 `cache.inv`），清单精确匹配才复用 |
| **托盘 / 关闭方式** | 可最小化到托盘；点 × 可选「直接关闭 / 最小化到托盘」，支持「不再询问」 |
| **控件动效** | hover 过渡 / 页面切换 / 可折叠卡片 / 数值滚动 / 结果错峰等，**可用开关一键关闭** |
| **单实例** | 同一时间只允许一个实例（避免多实例互相覆盖配置与缓存） |

## 支持的文件格式

| 扩展名 | 说明 |
|---|---|
| `.xlsx` | Excel 2007+（可含多工作表） |
| `.xls` | Excel 97-2003 |
| `.csv` | 逗号分隔文本（自动识别编码） |
| `.docx` | Word 文档（按段落/表格提取文本） |
| `.xse` | 本项目的加密表格格式；若由「附加密码」加密，首次加载会提示输入 |

> 无法解析或损坏的文件会被**跳过并汇总提示**（不会中断加载）。

---

## 构建

### 环境要求

- Windows 10 / 11 **x64**
- **MSYS2 + MinGW-w64**（实测 g++ 16.1）
- **Qt 6**（模块：Widgets、Network）
- CMake ≥ 3.20、Ninja

### 构建命令

```powershell
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 `
      -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe
cmake --build build
```

产物：`build\bin\excel_search.exe`

> ⚠️ 构建前请确保**没有本程序在运行**（否则链接会因文件占用失败）。
> 程序是 **GUI 子系统**（无控制台），因此在脚本里调用它必须用 `Start-Process -Wait`，
> 不能依赖 PowerShell 的 `&` 等待。

### 部署（打包 Qt 运行时）

直接运行编译产物需要系统装有 Qt。要发给没装 Qt 的电脑，必须一并部署运行时：

```powershell
powershell -ExecutionPolicy Bypass -File tools\deploy.ps1
```

脚本会：`windeployqt` → **依赖闭包补齐**（MSYS2 的 windeployqt 不带 MinGW 运行时与 Qt 传递依赖，
实测会缺 12 个以上 DLL）→ 收录开源许可 → 生成空 `data\` → **递归依赖校验** →
**PATH 剥离启动测试**（模拟没装 Qt 的环境）。

产物 `build\deploy\` 即**绿色免安装版**（整目录拷走即可用）。

### 生成安装包

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-installer.ps1
```

> 该脚本带**自动重试**：Inno 偶尔报 `EndUpdateResource failed (110)`，那是 Windows Defender
> 实时扫描抢占了刚生成的 Setup.exe（**不是脚本或图标问题**），重试一次即成功。
> 另注意：改完代码后要**先跑 `deploy.ps1` 再编译安装包**，否则会把旧程序打进去。

产物：`build\installer\ExcelSearchSetup-0.3.2.exe`
（管理员安装；桌面 / 开始菜单 / 任务栏快捷方式与安装位置均可选；
`data\` 空着自带并**授予普通用户写权限**，装完丢文件即可用。）

### 卸载

- 从「设置 → 应用」/「控制面板 → 程序和功能」卸载，或直接双击安装目录下的 **`uninstall.exe`**；
- 卸载器弹出一个**两页向导**（仿安装向导布局：粗体标题 + 灰色说明 + 底部按钮，按钮为
  `上一步` / `下一步…` / `取消`，第二页的确认按钮变为 `卸载`）：
  - **第 1 页 卸载选项** —— 与安装向导「任务」页同款控件（`TNewCheckListBox`）：
    1. **删除全部数据**（勾选后下面两项**强制勾选并置灰**，不可单独取消）；
    2. **删除 data 文件夹内的数据**（你自己的表格文件）；
    3. **删除用户配置与索引缓存**（主题 / 屏蔽 / 标记 / 搜索历史）；
  - **第 2 页 确认卸载** —— 逐条列出「将删除 / 将保留」的路径，可回上一步修改；
  - 卸载完成后弹提示，明确哪些「已删除」、哪些「已保留」。
- **静默卸载**（`uninstall.exe /VERYSILENT`）不弹任何界面，且**绝不删用户数据**（安全默认）。
- 卸载前会**自动结束正在运行的程序**：否则程序自身与已加载的 Qt DLL / 插件被系统锁住，
  卸载器删不掉它们，会出现"卸载成功但留下几十 MB 残骸"。本程序设置即时落盘、无未保存状态，
  且全机单实例，故按映像名结束最多影响一个进程，安全。
- 实现说明：Inno Setup 官方不支持自定义卸载器文件名（实测 6.7.3 无该指令），
  因此安装结束时把 `unins000.exe` 与 `unins000.dat` **一起改名**为 `uninstall.exe` / `uninstall.dat`
  （卸载器按**自身文件名**推导 .dat，必须同改），并同步注册表卸载入口；
  任一步失败都会**回滚**，保证「卸载器能正常用」优先于「名字好看」。

---

## 使用

1. 把表格文件放进程序目录下的 **`data\`**（安装版已自带该目录）；
2. 启动程序 → 点「**重新加载**」（首次启动会自动加载）；
3. 输入关键字 → 回车或点「搜索」；
4. 需要收窄结果时，在「二级筛选」里填词再点「筛选」。

- 配置与缓存位置：**`%APPDATA%\ExcelSearch\`**（`config.ini` / `cache.dat` / `cache.inv`）
  —— 与旧版本同目录，升级不会丢设置。
- 程序以**普通用户权限**运行（`asInvoker`），不需要管理员。

### 从旧版（V0.3.0）升级

旧版把设置存在**注册表** `HKCU\Software\ExcelSearch`，本版改用 `config.ini`。
**首次启动会自动搬迁一次**，带过来：管理密码 / 深色主题 / 屏蔽（文件级与条目级）/ 标记 /
搜索历史 / 共享模式与共享路径。

- 注册表**只读、原样保留** —— 想回退到旧版，设置还在；
- **已废弃的设置项不再迁移**：自定义窗口标题 / 应用图标 / 窗口透明度 / 毛玻璃（旧版的后台功能）；
- 旧版有**两级历史**（一级搜索 + 二级筛选），本版只保留一级搜索历史，故只迁前者；
- 搬迁只做一次（`config.ini` 里记 `meta/registryMigrated`）。客服排障可手动重跑：

```powershell
Start-Process .\excel_search.exe -ArgumentList @('--migrate','--out','migrate.txt') -Wait
```

## 自检与回归

程序内置无头自检钩子（结果**写文件**，因为 GUI 子系统没有控制台）：

```powershell
$exe = "build\bin\excel_search.exe"
$env:QT_QPA_PLATFORM = "offscreen"

Start-Process $exe -ArgumentList @('--report', 'out.txt') -Wait      # 加载规模/缓存/屏蔽标记等
Start-Process $exe -ArgumentList @('--search','工日','--out','s.txt') -Wait
Start-Process $exe -ArgumentList @('--shot','ui.png','--dark') -Wait # 离屏截图
Start-Process $exe -ArgumentList @('--migrate','--out','m.txt') -Wait # 强制重跑旧版设置搬迁
Start-Process $exe -ArgumentList @('--help') -Wait                   # 其余钩子见源码 main()
```

一键回归脚本：

| 脚本 | 用途 |
|---|---|
| `tools\selfcheck.ps1 [-NoAnim]` | 业务字段 + **13 张截图矩阵**（浅/深色 × 搜索页/设置页/解锁层/关闭对话框） |
| `tools\smoke.ps1` | 性能与健壮性冒烟：畸形文件 + 10 万行大文件 + 计时（隔离目录，不动 `data\`） |
| `tools\deploy.ps1` | 部署 + 依赖校验 + 无 Qt 环境启动测试 |

**关闭动效**（截图/回归必须带，否则会拍到动画中间态）：环境变量 `EXCELSEARCH_NO_ANIM=1`
或命令行 `--no-anim`。

## 目录结构

```
main.cpp             全部界面与业务编排（Qt，单文件）
search_engine.*      索引与检索核心
xlsx_reader.*  xls_reader.*  csv_reader.*  docx_reader.*   各格式解析
xse_codec.*          加密表格（.xse）编解码
crypto_win.cpp       Windows 平台加密实现（BCrypt）
core/                跨平台基础（文件 IO、加密接口）
pinyin.*             拼音/简拼匹配
thirdparty/          miniz / pugixml / libxls / win_iconv / rapidfuzz
licenses/            第三方组件许可与声明
docs/                开源合规核查等文档
tools/               部署、自检、冒烟脚本
installer/           Inno Setup 安装脚本
app_qt.rc            版本资源（版本号 / 图标 / 清单）
app_qt.manifest      应用清单（asInvoker + PerMonitorV2 高 DPI）
```

## 第三方组件与致谢

本程序以**动态链接**方式使用 **Qt 6（LGPL-3.0）**；另静态编译了
miniz(MIT) / pugixml(MIT) / rapidfuzz(MIT) / libxls(BSD) / win_iconv(Public Domain)。
完整许可与版权声明见 [`licenses/`](licenses/)。

界面**视觉风格**参考了 [MAA / MaaWpfGui](https://github.com/MaaAssistantArknights/MaaAssistantArknights)
与 [MaaEnd](https://github.com/MaaEnd/MaaEnd) —— **仅为风格参考，未使用其任何代码或资源**；
核查过程与证据见 [`docs/开源合规核查.md`](docs/开源合规核查.md)。

## 文档

| 文档 | 说明 |
|---|---|
| [`CHANGELOG.md`](CHANGELOG.md) | 版本改动记录（自 V0.3.2 起） |
| [`docs/V0.3.2_验证报告.md`](docs/V0.3.2_验证报告.md) | 本版交付验证报告（构建 / 部署 / 安装包 / 功能 / 性能实测） |
| [`docs/开源合规核查.md`](docs/开源合规核查.md) | 开源合规性核查记录（含 MAA / MaaEnd 使用核查） |
| [`docs/原型期存档/`](docs/原型期存档/) | Qt 原型期的设计与迁移记录（回迁清单、动效设计、缓存验证、冒烟报告等） |
| [`docs/历史/`](docs/历史/) | V0.3.0（Win32 版）时期文档，仅作沿革参考 |
