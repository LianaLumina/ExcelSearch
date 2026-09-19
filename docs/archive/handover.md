# 项目交接说明（开发进度与功能总览）

> 本文件是项目整体交接文档。接手的开发实例（人或 AI）请先完整阅读本文，再阅读各项目内文档，最后查看源码。

---

## 一、项目背景与目标

构建一套 **Excel 表格关键字搜索工具** 及配套 **表格加密工具**，用于工程表格数据的检索与核算场景：

1. **搜索工具**：脱机、免安装 Excel、支持 .xlsx/.xls 全文关键字搜索，附二次筛选、屏蔽、导出、颜色标记、智能列展示等能力
2. **加密工具**：将含敏感内容的表格加密为 .xse 格式分发，客户端搜索工具自动解密搜索，防止表格内容被第三方软件打开或外传
3. **部署形态**：Windows 离线版（当前主线）+ Linux ARM64 版（统信 UOS / 银河麒麟，已基本完成）

---

## 二、项目清单与关系

| 项目目录 | 定位 | 状态 |
|---------|------|------|
| `excel_search\` | **v0.1.0 原版**（无加密功能），作为回退还原点，**保持不动** | 冻结 |
| `excel_search_v0.2.0\` | **当前开发主线**（含加密搜索），Windows 离线版 + 共享模式 | 已编译可用 |
| `excel_encrypt\` | **加密工具**（仅在安全加密机运行，不分发客户端） | 已编译可用 |
| `<Linux 版目录>\` | Linux ARM64 版（UOS V20 / 麒麟 V10 SP1，Qt5），功能与 Windows 版对齐但**不含 .xse 加密** | 已编译通过，等待真机打包验证 |
| `<早期在线版>\` | 局域网在线版（数据源指向网络共享 UNC 路径） | **已由 v0.2.0 共享模式取代，不再单独开发** |

**依赖关系**：`excel_search_v0.2.0` 与 `excel_encrypt` 共享同一套 `xse_codec.h/.cpp`（加解密核心，两项目各持一份同版拷贝，修改需同步）。

---

## 三、版本演进历史

| 版本 | 关键变更 |
|------|---------|
| v0.1.0 | 控制台版 → Win32 GUI 版；搜索/二次筛选/详情弹窗/多选/屏蔽/导出/标记五色/已标记颜色筛选/核定工日·工时代码·任务标题智能列/缓存/管理后台（密码+超级密码<超管密码>+六子页）/说明书窗口/图标·标题·透明度自定义 |
| v0.2.0 | **新增加密体系**：.xse 加密格式（AES-256-GCM+PBKDF2+内置主密钥分片存储）；加密文件透明解密搜索；明文自动删除（双重比对）；无效 .xse 检测与分类提示；缓存整体加密；附加密码功能（默认关，仅超级密码可配置）；管理后台新增"加密设置"页 |
| v0.2.0+（共享模式） | **新增共享模式**：数据源可指向内网 UNC 共享目录（单 exe 双模式，主界面「模式切换」按钮）；共享模式下关闭明文自动删除；共享目录不可达时缓存兜底 + 提示；管理后台新增"共享路径设置"子页；默认共享路径 `\\server\share\excel_search`。**取代原 早期在线版** |
| v0.2.5 | **代码审查修复版**（2026-09-01 复审）：OCR 中文路径修复、模糊搜索按行去重 + 性能优化、缓存头解析边界检查、空正则提示、颜色筛选空结果状态一致、**.xse 缓存命中修复**（改用磁盘文件名 addFile）。详见第十二章 |
| v0.3.0 | **UI/外观美化版**（2026-09-01）：集成 Win32Acrylic（标准 Acrylic/Mica 毛玻璃，修复纯黑渐变文字不可见 bug）+ darkmode32plus（原生深色模式）；管理面板卡片化（账户安全/外观/数据三分组）；主界面扁平圆角按钮（owner-draw 悬停/按下/深色）；对话框深色适配。详见第十三章 |

---

## 四、v0.2.0 完整功能清单

### 4.1 基础功能
- data 文件夹自动扫描 .xlsx / .xls / .xse 三种格式，中文文件名支持
- 缓存机制：文件名+mtime+size 三重比对，10 天有效期，缓存文件整体 AES-GCM 加密
- 一级搜索（回车/按钮）、二级筛选（含各类前置校验提示）、清除筛选
- 搜索框灰色提示文字、Tab 键焦点切换
- 双击行详情弹窗（可复制文本，含核定工日/工时代码信息）

### 4.2 加密相关（v0.2.0 核心新增）
- **.xse 加载**：魔数 `XSE1` 校验 → AES-256-GCM 解密（认证防篡改）→ 负载魔数 `XSPD` 反序列化 → 注入搜索引擎
- **明文自动删除**：同名 .xse 与明文并存时，第一层比对 mtime+size（明文头免解密读取），第二层比对第二列（B列）全部单元格文本，两层一致则静默删除明文
- **无效文件检测**：失败原因分类（读取失败/格式无效/附加密码错误/数据损坏），左下角提示条汇总展示（最多 3 个文件名+省略号），10 秒自动消失
- **附加密码**：.xse 头标志位记录是否启用；启用时有效密钥=PBKDF2(主密钥+附加密码,盐)；客户端注册表存储启用状态与密码；首次加载弹窗输入，会话内缓存；密码错误区分提示
- **加密设置页**：仅以超级密码（<超管密码>）进入管理后台时可见（`g_adminIsSuper` 标志），可开关附加密码并修改密码

### 4.3 智能列展示
- 表头模糊匹配（包含命中→最短最优→精确匹配加分→平局取靠左列）
- 核定工日（两位小数四舍五入）、工时代码（原样）、任务标题（标记筛选时替换匹配内容列）

### 4.4 右键菜单
- 屏蔽（当前条目/所选条目 N/当前文件仅单选；确认弹窗；持久化；左下角"已屏蔽 N 条"提示）
- 导出（仅同文件条目；单条以匹配内容命名、多条"组合导出N"递增；从缓存数据直接生成 .xlsx；导出到 导出 文件夹并自动打开）
- 标记（红/紫/蓝/绿/黄五色+取消；NM_CUSTOMDRAW 整行变色；持久化）

### 4.5 管理后台（说明书中隐瞒，勿写入用户文档）
- 入口：一级框空 + 二级框输入 `<隐藏入口关键词>`
- 密码：普通管理密码（默认 <管理密码>，可改）+ 超级密码 <超管密码>（硬编码不可改）
- 子页：修改后台密码 / 修改窗口标题 / 修改应用图标 / 界面视觉效果（透明度滑块，毛玻璃已移除）/ 屏蔽管理 / 标记清除 / 加密设置（仅超级密码可见）

### 4.6 其他
- 屏蔽管理：条目列表+文件列表+解除
- 标记清除：单色/全部，二次确认（"此操作将会删除所有的x色标记，涉及xx个条目"）
- 窗口图标/标题修改、透明度、导出命名规则、说明书内置窗口（UTF-8 直读）

### 4.7 共享模式（v0.2.0+ 新增，取代 <早期在线版>）
- 主界面【模式切换】按钮：普通用户弹窗单选"离线模式 / 共享模式"，即时生效，无需重启
- 管理后台新增"共享路径设置"子页（普通管理密码可见）：配置 UNC 共享路径，保存后若处于共享模式立即重载
- 默认共享路径：`\\server\share\excel_search`（注册表无配置时生效）
- 共享模式行为差异：数据目录指向 UNC 路径；**关闭明文自动删除**（分享机管理员统一维护）；缓存比对保持严格 mtime+size（分享机为 Windows/NTFS 时秒级一致）；共享目录不可达时**不崩溃**，走本地缓存兜底 + 状态栏/toast 提示
- 窗口标题追加 `[共享模式]` 标识；配置存注册表 `ShareMode`(DWORD) / `SharePath`(REG_SZ)

---

## 五、加密技术方案（关键）

### 5.1 算法
- **AES-256-GCM**（Windows BCrypt API，零外部依赖，认证加密防篡改）
- **PBKDF2-HMAC-SHA256** 密钥派生，10 万次迭代，每文件随机 16B 盐、12B nonce
- **主密钥**：编译期嵌入 32 字节，以两个 32 字节分片 XOR 重组（`kKeyFragA`/`kKeyFragB`），防简单 strings 提取

### 5.2 .xse 文件格式
```
明文头: [4B]"XSE1" [2B]版本 [1B]标志(位0=附加密码) [16B]盐 [12B]nonce
        [8B]源mtime [8B]源size [2B]源扩展名长度 [N]源扩展名
密文区: [16B]GCM标签 + 密文(统一负载)
统一负载(解密后): [4B]"XSPD" + 文件数 + 每文件{文件名/源扩展名/mtime/size/工作表/行数据/表头}
```
- 源文件指纹（mtime+size+扩展名）放明文头 → 客户端免解密即可第一层比对
- 统一负载与缓存 EXSR2 序列化结构同构（writeStr/readStr 等辅助函数）

### 5.3 缓存加密
- cache.dat 保存流程：`saveToFile(临时明文) → encryptData → cachePath → 删临时`
- 读取流程：`decryptData → 临时明文 → EXSR2 头检查/loadFromFile → 删临时`
- 兼容旧 v0.1.0 明文缓存（EXSR2 魔数直接按明文处理，一次性迁移重写为加密格式）

### 5.4 安全边界
- 防护等级：防普通第三方软件打开、防一般用户外传；不防专业逆向（内置密钥可被提取）
- 明文自动删除仅指纹+第二列内容双层一致时执行，防止误删

---

## 六、构建方法（Windows）

**依赖**：Visual Studio 2022/2026（C++ 桌面开发）、CMake、第三方库已在 `thirdparty\`（miniz/pugixml/libxls/win_iconv，缺文件时运行 `check_deps.ps1` 按窗口提示下载）。

```bat
:: v0.2.0 主程序
cd excel_search_v0.2.0\build
cmake .. -G "Visual Studio 18 2026" -A x64
cmake --build . --config Release
:: 输出: build\bin\Release\excel_search.exe + 使用说明书.txt + update_icon.bat（POST_BUILD 自动拷贝）

:: 加密工具
cd excel_encrypt\build
cmake .. -G "Visual Studio 18 2026" -A x64
cmake --build . --config Release
:: 输出: build\bin\Release\excel_encrypt.exe
```

**编译器**：MSVC，/MT 静态链接 CRT（单 exe 分发无需运行库）、/SUBSYSTEM:WINDOWS、requireAdministrator manifest。

---

## 七、关键代码位置索引（v0.2.0 main.cpp 约 2900 行）

| 功能 | 位置线索 |
|------|---------|
| 加密核心（全部） | `xse_codec.h/.cpp`：`xse::encryptData` / `decryptData` / `readHeaderMeta` / `serializePayload` / `deserializePayload` |
| 文件加载总流程 | `LoadFiles()`：扫描 → **明文自动删除（双重比对）** → 加密缓存读写 → 三格式分派加载 → 无效 .xse 收集 |
| .xse 单文件加载 | `LoadXseFile()`：返回 `XseFailReason` 枚举（OK/ReadFail/NotEncrypted/WrongPassword/Corrupted） |
| 双层比对 | `CompareCol1Sheets()`（第二列文本对比）+ `ParsePlainSheets()` |
| 附加密码弹窗 | `PromptAdditionalPassword()` / `PwdPromptWndProc` |
| 附加密码配置 | `SubPageEncryptProc`（加密设置子页，仅 `g_adminIsSuper` 时入口可见） |
| 超级密码标志 | `AdminPwdWndProc` 中 `g_adminIsSuper = (密码命中超管密码常量 && pw != g_adminPassword)` |
| 无效 .xse 提示条 | `LoadFiles` 末尾 `invalidXseNames`/`invalidXseReasons` → 左下角 toast（10 秒） |
| 提示条机制 | `g_hToast`：常显空文字策略（`WS_VISIBLE` 创建，隐藏=置空文字），`ShowWindow(SW_SHOW)` 兜底，WM_TIMER 清空 |
| 缓存文件 | `LoadFiles` 中 `cacheTemp` 临时文件 + `xse::encryptData/decryptData` 包裹 `EXSR2` 序列化 |
| 图标替换 | 走 `update_icon.bat`（ResourceHacker），`icon_updater` 已废弃删除 |
| 加密工具 GUI | `excel_encrypt\main.cpp`：文件列表+附加密码折叠+逐文件加密+每文件删除询问+输出"已加密文件/"+注册表配置（`LoadPwdSettings`/`SavePwdSettings`，启动预填+"保存为默认"按钮） |
| 共享模式开关 | `g_shareMode`/`g_sharePath` 全局变量 + 注册表 `ShareMode`/`SharePath`；`ShowModeDialog()`（主界面模式切换弹窗）；`SubPageShareProc`（管理后台共享路径子页）；`UpdateWindowTitle()` 标题标识 |
| 共享模式数据目录 | `WinMain` 中按 `g_shareMode` 赋值 `g_dataFolder`（默认 `exeDir\data` 或 UNC 路径） |
| 共享模式加载容错 | `LoadFiles()` 开头 `shareUnreachable` 检测（error_code 遍历）→ 不可达时跳过明文删除、强制缓存兜底并提示 |

---

## 八、已完成的测试验证（Windows 实测）

1. ✅ xse 加解密回环（中英文内容逐字段一致）
2. ✅ 密码模式往返（正确密码解出、错误密码拒绝）
3. ✅ .xse 加载与搜索（中文表头/数据完整）
4. ✅ 明文自动删除（双层比对命中 → `DeleteFileW` 成功；指纹不符 → 保留）
5. ✅ 缓存加密（cache.dat 头为 `XSE1`，重启缓存命中）
6. ✅ 无效 .xse 检测（随机文本改名 .xse → 识别"格式无效"，跳过且不影响其他文件）
7. ✅ 混合场景（明文+.xse+伪.xse 三文件并存）行为正确
8. ✅ 共享模式（v0.2.0+）：终端实测可正常读取分享机共享目录中的 .xse 并搜索（用户验收通过）
9. ✅ 加密工具注册表配置（v0.2.0+）：启动读取注册表预填勾选状态与密码；"保存为默认"写入 `HKCU\Software\ExcelSearch`（与客户端同键）
10. ✅ 安装包（v0.2.0+）：Inno Setup 编译成功；实测静默安装到 `C:\Program Files\ExcelSearch`（exe/说明书/脚本/data/卸载程序齐全）、安装后程序正常启动并读取既有配置、卸载后程序目录清除且 `%APPDATA%\ExcelSearch` 用户配置保留

**重要修复记录**：
- `xlsx_reader.cpp` Windows 路径文件句柄泄漏（`mz_zip_reader_init_cfile` 后未 `fclose`）→ 曾导致删除明文时 ERROR_SHARING_VIOLATION(32)，已在 v0.2.0 与 excel_encrypt 两处修复（函数末尾 `#ifdef _WIN32 fclose(fp)`）
- 提示条 SW_HIDE 与"常显空文字"策略冲突 → 统一为置空文字 + SW_SHOW 兜底

---

## 九、遗留事项 / 后续开发建议

1. **Linux 版未含 .xse**：`<Linux 版>` 需补充 xse_codec（用 OpenSSL/libgcrypt 替代 BCrypt，或条件编译），且其缓存仍为明文 EXSR2（需同步缓存加密改造）
2. ~~**在线版**：基于旧版本，若需复活需合并 v0.2.0 全部特性~~ → **已完成**：v0.2.0 内置共享模式（UNC 数据源 + 共享路径配置），取代 <早期在线版>，无需再单独开发在线版
3. ~~**附加密码在加密机的配置**~~ → **已完成**：加密工具新增注册表配置支持。启动时读取 `HKCU\Software\ExcelSearch` 的 `AdditionalPwdEnabled`/`AdditionalPwd` 预填界面；界面新增"保存为默认"按钮写入注册表（与客户端同键，天然互通）。安全性权衡：密码明文落盘注册表，与客户端行为一致，加密机为受控安全环境可接受
4. **指纹比对的 mtime 精度**：两端均取秒级截断；**已评估**：分享机为 Windows/NTFS 时秒级一致，无需容差；仅当分享机换为 Linux/Samba 时需考虑 ±2 秒容差（并注意容差可能漏检真实更新）
5. ~~**部署包装**~~ → **已完成**：Inno Setup 6 安装脚本 仓库内的 `installer\setup.iss`（编译器 ISCC 需单独安装）。产物 `ExcelSearchSetup-0.2.0.exe`（~2.2MB，输出到 build\bin\Release）。要点：管理员安装到 `{autopf}\ExcelSearch`；桌面/开始菜单快捷方式；data 目录含占位说明文件；**卸载保留 %APPDATA%\ExcelSearch 用户配置**；简体中文向导（ChineseSimplified.isl 需从 issrc 仓库下载放入 Inno Languages 目录）。已实测安装/卸载/启动/配置保留全链路通过
6. ~~**加密工具图标/版本信息**~~ → **已完成**：加密工具已加自定义图标（蓝底金锁 app.ico）+ VERSIONINFO（"Excel 表格加密工具 v0.2.0"）。**主程序亦同步加图标**：WEPE.ico（9 帧多尺寸）嵌入 excel_search.exe（app.rc + 窗口类挂载），并补 VERSIONINFO（"Excel 表格关键字搜索工具 0.2.0"）；安装程序图标同用 WEPE.ico

---

## 十、文档清单

| 文档 | 位置 | 用途 |
|------|------|------|
| 本交接说明 | 包根目录 | 开发交接 |
| 使用说明书.txt | v0.2.0 项目及成品目录 | 客户端用户（隐藏管理后台） |
| README_V0.3.0.md | v0.2.0 项目 | 随包分发版说明书 |
| 加密工具使用说明.md | excel_encrypt 项目 | 仅加密机操作人员 |
| 回退到原版.bat | v0.2.0 项目 | 一键用 v0.1.0 源码覆盖回退 |

---

## 十一、成品文件（本包内 成品程序\ 目录）

```
成品程序\
├── v0.2.0主程序\
│   ├── excel_search.exe    (约 870KB，v0.2.5 修复版，含 11 项新功能)
│   ├── 使用说明书.txt
│   ├── update_icon.bat
│   └── data\               (存放待搜索文件)
└── 加密工具\
    ├── excel_encrypt.exe   (约 207KB，含注册表配置 + 图标/版本信息)
    └── 加密工具使用说明.md
```

> 注：最新构建产物位于 仓库的构建输出目录（excel_search.exe + ExcelSearchSetup-0.3.0.exe 安装包），成品程序目录可随时以最新版替换。源码新增模块：`csv_reader.*`、`docx_reader.*`、`ocr_reader.*`、`pinyin.*`（含 `pinyin_table.inc`）、`app.manifest`；第三方新增 `thirdparty/rapidfuzz/`（header-only，MIT）、`thirdparty/acrylic/`（Win32Acrylic，MIT）、`thirdparty/darkmode32plus/`（BSD-3）。

---

## 十二、v0.2.5 代码审查修复记录（2026-09-01 复审，版本由 v0.2.0++ 重命名为 v0.2.5）

对当时的源码全量复审后修复 6 项问题，已重新编译并在共享模式实测验证：

| # | 问题 | 位置 | 修复 |
|---|------|------|------|
| 1 | **OCR 中文路径必然失败**：`std::wstring wpath(filepath.begin(), filepath.end())` 逐字节扩展而非 UTF-8→UTF-16 转换，中文文件名/路径的图片 `GetFileFromPathAsync` 收到乱码路径 | `ocr_reader.cpp` | 改用 `MultiByteToWideChar(CP_UTF8,...)` 转换（与 main.cpp `Utf8ToWide` 一致） |
| 2 | **模糊搜索结果未按行去重**：`fuzzySearch()` 遍历单元格级 entry 独立入列，同一行多个单元格命中产生重复行；与 `search()`/`regexSearch()` 行为不一致 | `search_engine.cpp` `fuzzySearch()` | 按 `(filename,sheetName,row)` 聚合取最高分，一行一条结果 |
| 3 | **缓存头解析无边界检查**：`fread` 长度字段未校验，损坏缓存 `std::string(slen)` 可能崩溃/超分配 | `main.cpp` 缓存头读取 | 校验 `slen` 范围（>0 且 ≤1MB）、`fread` 返回值；非法即放弃缓存 |
| 4 | **空正则返回全量结果**：`re:` 后无模式时 `std::regex("")` 匹配所有条目 | `main.cpp` DoSearch | 空 pattern 弹提示并返回 |
| 5 | **颜色筛选空结果状态不一致**：`g_results` 已清空但 ListView 未刷新，后续操作基于不一致状态 | `main.cpp` DoFilter | 空结果时同步 `UpdateListView(g_results)` |
| 6 | **.xse 缓存永不命中（既有深层缺陷）**：加载 .xse 用负载内原始文件名（.xlsx）而非磁盘名（.xse），与 `setFileMeta`/缓存头 key 不一致 → namesMatch 恒 false → 每次全量重载（共享模式尤为明显） | `main.cpp` LoadXseFile | 改用磁盘文件名 `fname` addFile；缓存与磁盘 key 一致，实测 3 次启动全部命中缓存 |

**性能优化**（随 #2 一并完成）：`fuzzySearch()` 预计算 keyword 字符集合，单元格与 keyword 无公共字符时跳过 rapidfuzz 调用，减少大量无效评分。

**行为说明**：#6 修复后，.xse 文件在搜索结果中显示磁盘文件名（`xxx.xse`），不再显示加密前的原始明文名（`xxx.xlsx`）——与 data 文件夹实际一致，屏蔽/标记/智能列的 key 与磁盘统一。

**验证记录**：本地模式 + 共享模式（UNC 含 .xlsx 与 .xse 实测）双场景缓存命中通过；3 次连续启动缓存时间戳不变；缓存头文件列表与磁盘 namesMatch/metaMatch 均为 YES。

---

## 十三、v0.3.0 UI/外观美化记录（2026-09-01）

**目标**：修复毛玻璃等视觉 bug、提升整体美感（管理面板/主界面），优先采用 GitHub 可下载/可集成的插件化方案（同 rapidfuzz header 集成模式）。

### 13.1 第三方库集成（thirdparty/）
| 库 | 用途 | 许可 | 集成方式 |
|----|------|------|---------|
| Win32Acrylic（ALTaleX531） | 标准 Acrylic/Mica 毛玻璃 | MIT | header-only，已增强 SetBlurAmount/SetTintColor/UseLegacy |
| darkmode32plus（anthonyleestark） | 原生深色模式（Win10 1809+） | BSD-3（含 MIT/MPL-2.0 衍生） | 静态库源码入 CMake（DarkMode.cpp + DMSubclass.cpp + SysColorHook.cpp） |

### 13.2 修复与美化清单
1. **毛玻璃 bug 修复**：删除旧 `SetWindowCompositionAttribute` + `ACCENT_ENABLE_ACRYLICBLURBEHIND` 纯黑 GradientColor 实现（调高强度文字不可见）→ Win32Acrylic 标准 Acrylic；tint 随深浅色模式联动（浅色白底 0.62 alpha / 深色深灰 0.72 alpha），机制上保证文字可读；blurPercent 0-100 → blur 4-64 映射
2. **深色模式补全**：darkmode32plus 提供标题栏/滚动条/控件原生深色；对话框（管理面板/子页/密码框）接入 `DarkDlgColor()`（背景 RGB(44,44,46)、编辑框 RGB(32,32,32)、文字 RGB(230,230,230)）
3. **管理面板卡片化**：9 按钮垂直堆砌 → 3 圆角卡片（owner-draw STATIC）：账户与安全（修改密码/加密设置[超管]）、外观（标题/图标/视觉效果/深色模式，含状态提示）、数据（屏蔽/标记/共享路径，含模式提示）；窗口 460x740（超管）/460x700
4. **主界面扁平圆角按钮**：全部 10 个按钮改 `BS_OWNERDRAW`（`CreateFlatButton` + `DrawFlatButton`）：圆角 7px、悬停蓝边高亮、按下凹陷、深浅色自适应；主窗口 WM_DRAWITEM + WM_MOUSEMOVE/WM_MOUSELEAVE 悬停跟踪

### 13.3 关键代码位置
- 毛玻璃：`ApplyAcrylicEffect`/`UpdateVisualEffect`/`AcrylicTintColor`（main.cpp 视觉区）
- 深色：`ApplyDarkMode`/`DarkDlgColor`/`EnsureDlgDarkBg`
- 扁平按钮：`CreateFlatButton`/`DrawFlatButton`/`g_hHoverBtn`
- 管理面板：`AdminPanelWndProc` WM_CREATE 卡片布局 + WM_DRAWITEM

### 13.4 验证记录
- ✅ 编译通过（exe 约 931KB）；冒烟运行 6-8 秒无崩溃（含 BlurPercent=60 强制 acrylic 路径）
- ✅ 缓存命中回归正常；BM_CLICK help/mode 按钮弹窗验证消息通道
- ✅ 10 个主界面按钮 ownerDraw 枚举确认生效
- ⏳ **待视觉目检**：毛玻璃 0-100% 文字可读性、卡片/按钮配色与对齐、深色模式无白块、高 DPI、Win10 legacy 配方真机确认
