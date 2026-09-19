（[English](README.md) | 中文）

# ExcelSearch — Excel 表格关键字搜索工具

把表格文件放进 `data` 目录，即可对**全部表格内容**做关键字检索，直接定位到具体的文件、工作表与行。
支持 `.xlsx` / `.xls` / `.csv` / `.docx` / `.xse`（本项目加密格式）。

- **版本**：V0.3.2
- **许可证**：GNU GPL-3.0（见 [LICENSE](LICENSE)）
- **界面**：Qt 6 Widgets + 自绘 QSS（无边框窗口、深浅双主题、可一键关闭的控件动效）
- **平台**：Windows 10 / 11 x64

---

## 界面预览

| 搜索页（浅色） | 设置页（深色） |
|---|---|
| ![搜索页](docs/images/search-light.png) | ![设置页](docs/images/settings-dark.png) |

| 使用说明书（浅色） | 使用说明书（深色） |
|---|---|
| ![使用说明书](docs/images/manual-light.png) | ![使用说明书（深色）](docs/images/manual-dark.png) |

> 截图由程序自带的离屏截图钩子生成（`--shot`），与运行时界面一致。

---

## 功能

| 功能 | 说明 |
|---|---|
| **多格式检索** | `.xlsx` / `.xls` / `.csv` / `.docx` / `.xse` |
| **三种搜索方式** | 精确搜索；模糊搜索（默认开启，作为精确结果的补充）；正则（前缀 `re:`） |
| **二级筛选** | 在当前结果里继续筛，两种模式：标准（每次从基准重算，与顺序无关）/ 逐级（层层收窄） |
| **屏蔽 / 标记** | 屏蔽整个文件或某一行；行标记 5 种颜色，并可直接检索已标记行 |
| **智能列** | 结果表 3–7 列可配置，中间列可映射到任意表头列名 |
| **搜索历史** | 保留条数与时效可配置（10 分钟 ~ 12 小时 / 关闭程序后删除） |
| **数据源** | 离线（程序目录 `data\`）/ 共享模式（UNC 共享目录，不可达时自动使用缓存） |
| **缓存** | 索引落盘（`cache.dat`）+ 全量清单（`cache.inv`），清单精确匹配才复用 |
| **加密表格** | `.xse` 透明解密检索；由「附加密码」加密时首次加载提示输入 |
| **使用说明书** | 随程序分发的图文说明书窗口（章节导航 + 正文，独立窗口可最大化），入口在「关于我们」页 |
| **托盘与关闭方式** | 可最小化到托盘；点 × 可选「直接关闭 / 最小化到托盘」，支持「不再询问」 |
| **控件动效** | hover 过渡、页面切换、可折叠卡片、数值滚动等，可一键关闭 |
| **单实例** | 同一时间只运行一个实例，避免多实例互相覆盖配置与缓存 |

> 无法解析或损坏的文件会被跳过并在加载结束后汇总提示，不中断整体加载。

## 支持的文件格式

| 扩展名 | 说明 |
|---|---|
| `.xlsx` | Excel 2007+（可含多工作表） |
| `.xls` | Excel 97-2003 |
| `.csv` | 逗号分隔文本（自动识别编码） |
| `.docx` | Word 文档（提取段落与表格文本） |
| `.xse` | 本项目加密表格格式 |

---

## 项目结构与实现方式

| 部分 | 对应文件 | 实现方式 |
|---|---|---|
| 界面与业务编排 | `main.cpp` | Qt 6 Widgets 单文件实现；QSS 主题（深浅 + 强调色）；无边框自绘标题栏（`startSystemMove` / `startSystemResize`）；配置用 `QSettings`(INI)；单实例用 `QLocalServer` / `QLocalSocket`；说明书窗口用 `QTextBrowser::setMarkdown` |
| 检索核心 | `search_engine.*` | C++17 索引与检索：精确、正则、模糊（rapidfuzz 评分）；二级筛选两种模式；屏蔽与标记；搜索历史 |
| 表格解析 | `xlsx_reader.*`、`xls_reader.*`、`csv_reader.*`、`docx_reader.*` | xlsx/docx：miniz 解包 + pugixml 解析（含共享字符串）；xls：libxls（BIFF8）；csv：自写分隔符/引号状态机；编码转换统一走 win_iconv |
| 加密表格 | `xse_codec.*` | 容器格式 `XSE1`：AES-256-GCM 加密 + PBKDF2-HMAC-SHA256 派生密钥（盐 16 字节、10 万次迭代）；载荷为自定义序列化格式 `XSPD` |
| 平台加密实现 | `crypto_win.cpp` | 用 Windows BCrypt 实现 `core/crypto.h` 的接口：随机数、PBKDF2、AES-256-GCM |
| 跨平台基座 | `core/` | 文件 IO 与加密接口的抽象层（接口与平台实现分离） |
| 拼音匹配 | `pinyin.*`、`pinyin_table.inc`、`gen_pinyin.py` | 拼音/简拼码表匹配；码表由 `gen_pinyin.py` 生成 |
| 第三方库 | `thirdparty/` | miniz（ZIP）、pugixml（XML）、libxls（旧版 xls）、win_iconv（编码转换）、rapidfuzz（模糊匹配） |
| 资源与清单 | `app.ico`、`app_icon.qrc`、`manual.qrc`、`app_qt.rc`、`app_qt.manifest` | 程序图标、内嵌说明书副本、版本资源、应用清单（`asInvoker` + PerMonitorV2 高 DPI） |
| 构建定义 | `CMakeLists.txt` | CMake ≥ 3.20；Qt 6 模块 Widgets / Network；`AUTOMOC`/`AUTORCC`；GUI 子系统（`WIN32_EXECUTABLE`）；可选编译期注入本地密码文件 |
| 部署脚本 | `tools/deploy.ps1` | 绿色版打包：依赖闭包补齐 → 递归依赖校验 → PATH 剥离启动测试 → 收集第三方许可 → 生成空 `data\` |
| 安装包 | `tools/build-installer.ps1`、`installer/setup.iss` | Inno Setup 6 编译；管理员安装、快捷方式可选；`data\` 授予普通用户写权限；两页卸载向导；静默卸载保留用户数据 |
| 自检 / 冒烟 | `tools/selfcheck.ps1`、`tools/smoke.ps1` | 自检：调用程序内置离屏钩子（`--report` / `--search` / `--filter` / `--shot` 等）并输出截图矩阵；冒烟：在隔离目录造畸形与超大文件，计时校验加载与搜索 |
| 示例数据 | `data/`、`gen_test_files.ps1` | 合成的回归测试文件（csv / xls / docx / xse），用于本地验证 |
| 使用说明书正文 | `MANUAL.md` | 面向使用者的图文说明；随发行包放在程序目录，程序优先读取它、缺失时用内嵌副本 |
| 许可与声明 | `LICENSE`、`licenses/` | 本项目 GPL-3.0；第三方组件清单与替换说明见 `licenses/THIRD-PARTY-NOTICES.md` |
| 文档 | `docs/` | 交付验证报告、开源合规核查，以及历史沿革归档 |
| 更新记录 | `CHANGELOG.md` | 版本改动记录 |

---

## 构建

### 环境要求

- Windows 10 / 11 **x64**
- **Qt 6**（模块：Widgets、Network）
- **CMake ≥ 3.20** + **Ninja**
- 编译器：**MinGW-w64**（MSYS2，实测 g++ 16.1）或 MSVC
- 可选：**Inno Setup 6**（生成安装包）、**PowerShell 5.1+**（部署与自检脚本）

### 构建步骤（MinGW 示例）

```powershell
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 `
      -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe
cmake --build build
```

产物：`build\bin\excel_search.exe`

- 构建前请确认本程序**没有正在运行**（否则链接会因可执行文件被占用而失败）；
- 程序是 **GUI 子系统**（不带控制台），在脚本中调用它需用 `Start-Process -Wait`，不能依赖 PowerShell 的 `&` 等待。

### 可选：编译期密码注入

管理后台的密码可在编译期注入，避免把真值写进源码：

1. 复制 `local_secrets.cmake.example` 为 `local_secrets.cmake`，填入 `ES_SUPER_PASSWORD` / `ES_DEFAULT_ADMIN_PASSWORD`；
2. `local_secrets.cmake` 已被 `.gitignore` 忽略，不随仓库发布；
3. **不提供该文件也能正常构建**：此时超管通道关闭，默认管理密码退回内置的弱默认值（仅适合本机开发）。

---

## 部署

### 绿色版（免安装）

直接运行编译产物需要目标机器装有 Qt；要发给没装 Qt 的电脑，需一并部署运行时：

```powershell
powershell -ExecutionPolicy Bypass -File tools\deploy.ps1
```

产物 `build\deploy\` 即绿色版，整目录拷贝即可运行。其中包含：`excel_search.exe`、Qt 运行时与插件、
MinGW 运行时、`licenses\`（第三方许可全文与依赖清单）、`MANUAL.md`、空 `data\`。

### 安装包

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-installer.ps1
```

产物：`build\installer\ExcelSearchSetup-<版本>.exe`

- 会先确保部署产物是最新的（**改完代码应先跑 `deploy.ps1` 再打包**，否则会把旧程序打进安装包）；
- 安装：管理员权限安装，安装位置、桌面 / 开始菜单 / 任务栏快捷方式可选；`data\` 空着自带并授予普通用户写权限；
- 卸载：两页向导（第 1 页勾选删除范围，第 2 页确认），可选是否删除表格数据与用户配置；
  **静默卸载（`/VERYSILENT`）不弹界面且保留用户数据**。

---

## 运行与配置

1. 把表格文件放进程序目录下的 **`data\`**（安装版已自带该目录）；
2. 启动程序，点「重新加载」（首次启动会自动加载）；
3. 输入关键字，回车或点「搜索」；需要收窄结果时用「二级筛选」。

- 程序以**普通用户权限**运行（`asInvoker`），不需要管理员；
- 配置与缓存位置：**`%APPDATA%\ExcelSearch\`**（`config.ini`、`cache.dat`、`cache.inv`）；
- 旧版（V0.3.0）存在注册表里的配置，会在**首次启动时自动迁移**到 `config.ini`（注册表只读保留）。

---

## 许可证与第三方组件

- 本项目以 **GPL-3.0** 发布，全文见 [`LICENSE`](LICENSE)；
- 以**动态链接**方式使用 **Qt 6**（LGPL-3.0）；另静态编译 miniz(MIT) / pugixml(MIT) / rapidfuzz(MIT) /
  libxls(BSD) / win_iconv(Public Domain)；
- 第三方组件清单、版权与替换说明见 [`licenses/THIRD-PARTY-NOTICES.md`](licenses/THIRD-PARTY-NOTICES.md)；
  各许可证全文由 `tools\deploy.ps1` 在打包时收集，随发行包放在安装目录的 `licenses\`；
- 界面视觉风格与说明书弹窗的交互方式参考 [MAA / MaaWpfGui](https://github.com/MaaAssistantArknights/MaaAssistantArknights)
  与 [MaaEnd](https://github.com/MaaEnd/MaaEnd)；**未使用、未复制其任何源代码、样式表或资源文件**，
  核查记录见 [`docs/LICENSE-COMPLIANCE.md`](docs/LICENSE-COMPLIANCE.md)。
