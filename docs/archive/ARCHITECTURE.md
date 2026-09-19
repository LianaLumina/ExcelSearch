# V0.3.1「铺路」重构 — 架构说明与拆分指引

> 本文档说明 V0.3.1 重构的目标、目录/模块边界，以及「若因拆得不够细导致后续开发不便，可按本文指引继续拆」的方法。
> 最后更新：V0.3.1 重构进行中（core 逻辑解耦已完成，main.cpp 拆分进行中）

## 1. 为什么做 V0.3.1

- 目标是让 ExcelSearch 最终成为「人人可用的文件综合处理工具」，并沿 **Qt + Android（V0.8+ 移动端 / 服务器加密分发特供版）** 路线演进。
- V0.3.1 只「铺路」：**把纯逻辑与 Win32 UI 彻底解耦**，证明核心可跨平台移植（MinGW 编译验证）；**不迁 Qt、不写 Android、不改任何 UI/功能**。

## 2. 分层目标（最终形态）

```
core/           纯 C++ 静态库，零 windows.h，可编到 Windows/Linux/Android
                含：文档解析(reader) / 搜索 / 拼音 / xse 格式 / 通用文件IO / 加密接口
platform/       平台实现（现在只有 win）
                crypto_win.cpp（BCrypt 实现 core::crypto）、ocr(WinRT)、darkmode32plus、
                UI 相关（Win32/GDI 界面、托盘、自绘控件）→ 未来 Qt 版整层替换
main.cpp / ui_*  目前 Win32 UI 单文件形态（V0.3.1 正按功能区拆出）
```

## 3. core 模块边界（已完成解耦，均无 windows.h）

| 模块 | 依赖 | 状态 |
|---|---|---|
| core/file_io.h/.cpp | std::filesystem（UTF-8 路径跨平台） | ✅ 已解耦 |
| xlsx_reader / csv_reader / docx_reader / xls_reader | miniz / pugixml / libxls + core::ReadFileBytes | ✅ 已解耦 |
| search_engine | rapidfuzz + core file_io（缓存改内存 buffer） | ✅ 已解耦 |
| pinyin | 纯查表 | ✅ 本就纯 C++ |
| xse_codec | 只调 core::crypto 接口（格式/序列化在本模块） | ✅ 已解耦 |
| core/crypto.h | 接口声明（Random/Pbkdf2Sha256/AesGcmEncrypt/Decrypt） | ✅ |
| crypto_win.cpp | BCrypt（**平台侧，不进 core**） | ✅ |

> 规则：core 内**严禁 `#include <windows.h>`**；文件路径一律 UTF-8 字符串 + `core::ReadFileBytes/WriteFileBytes`（内部 `std::filesystem::u8path`）。

## 4. xse 加密跨平台约定（护城河）

- `xse_codec` 只做格式（XSE1 头 / XSPD payload 序列化 / 主密钥 XOR 拼合 / material = masterKey+password）。
- 加密原语全部走 `core::crypto` 接口；两端（Win / Android）用**同一参数**调用对等实现：
  - PBKDF2-HMAC-SHA256(iterations=100000)
  - AES-256-GCM（nonce 12B，tag 16B，无 AAD）
- 将来 Android 实现 = 同一接口的 OpenSSL 等实现。改算法前必须保证两端文件互通。

## 5. 平台层（不进入 core，将来被 Qt 版替代）

- ocr_reader.cpp：WinRT OCR（Android 将来用 ML Kit 等）。
- crypto_win.cpp：BCrypt。
- darkmode32plus、Win32Acrylic、GDI 自绘、系统托盘：Win UI/平台能力。

## 6. main.cpp 拆分（进行中）与后续再拆指引

main.cpp 已按**主要功能区**拆出（当前约 229 行，原 4722 行）。若之后发现「拆得不够细导致开发不便」，按下面的组继续拆：

- **ui_titlebar**：标题栏矩形/命中/绘制、月亮太阳切换按钮、对话框标题栏（DrawDlgTitleBar）。✅ 已拆
- **ui_controls**：扁平按钮（CreateFlatButton/DrawFlatButton）、CustomEdit、面板/卡片绘制、DialogFrameProc、ApplyDialogFrame。✅ 已拆
- **ui_tray**：托盘注册/气泡/隐藏恢复（函数已独立）。✅ 已拆
- **ui_dialogs**：各对话框 WndProc 与 Show*Dialog 入口（管理后台/密码/子页/统计/说明书/详情/模式/附加密码）。✅ 已拆（ui_dialogs.cpp/.h，2026-09）
- **ui_mainwindow**：主窗口逻辑（WndProc / InitControls / OnSize / 自定义控件 / 文件加载 / 搜索·筛选·导出）。✅ 已拆（ui_mainwindow.cpp/.h，2026-09）
- **ui_shared**：跨模块共享工具/配置读写（编码、状态/标题/图标、圆角、视觉观感、LoadSettings/SaveSettings、列映射、HeaderSubclassProc）。✅ 已拆（ui_shared.cpp，2026-09）
- **main.cpp**：仅保留全局对象、WinMain、消息循环、入口组装。

> 拆分注意：跨文件共享的全局变量/工具函数统一放共享头 `ui_shared.h`（extern 声明），共享工具函数实现已收敛到 `ui_shared.cpp`；全局变量定义仍留在 main.cpp（避免静态初始化顺序问题）。抽取时按「① 去 static ② 共享头声明 ③ 每抽一组立刻编译回归」的节奏走。

> 拆分注意：当前大量 `static` 全局/函数是文件作用域。抽取跨文件时需：① 去 static 并放到共享头（如 app_shared.h：g_hInst/g_hMainWnd/g_hFont/g_darkMode/g_winTitle 等）；② 同组函数尽量同文件减少 extern；③ 每抽一组立刻编译回归，不要一次搬完。

## 7. 验证方法（V0.3.1 每步）

1. MSVC Release 编译通过（已还原点 V0.3.0_还原点 可回滚）。
2. 功能回归：托盘、深浅色、多格式读取、加密文件、缓存命中一致。
3. MinGW（g++ 无 windows.h 环境）编译 core 验证可移植。
