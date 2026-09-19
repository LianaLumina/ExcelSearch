（English | [中文](README.zh-CN.md)）

# ExcelSearch — Keyword Search for Excel Workbooks

Drop your spreadsheets into the `data` folder and search the **full content** of every file at once —
jumping straight to the file, sheet and row that contains what you are looking for.
Supported formats: `.xlsx` / `.xls` / `.csv` / `.docx` / `.xse` (this project's encrypted format).

- **Version**: V0.3.2
- **License**: GNU GPL-3.0 (see [LICENSE](LICENSE))
- **UI**: Qt 6 Widgets with hand-written QSS (frameless window, light/dark themes, switchable widget animations)
- **Platform**: Windows 10 / 11 x64

---

## Features

| Feature | Description |
|---|---|
| **Multi-format search** | `.xlsx` / `.xls` / `.csv` / `.docx` / `.xse` |
| **Three search modes** | Exact; fuzzy (enabled by default, supplements exact hits); regular expression (prefix `re:`) |
| **Second-stage filter** | Narrow down the current result set — *standard* (recomputed from the base set, order-independent) or *cumulative* (narrowed step by step) |
| **Blocking / marking** | Block a whole file or a single row; mark rows in 5 colors and search the marked rows directly |
| **Configurable columns** | The result table shows 3–7 columns; middle columns can be mapped to any header column name |
| **Search history** | Configurable entry count and lifetime (10 minutes up to 12 hours, or cleared on exit) |
| **Data source** | Offline (the `data\` folder next to the executable) or shared mode (UNC share, falling back to the cache when unreachable) |
| **Cache** | On-disk index (`cache.dat`) plus a full inventory (`cache.inv`); reused only when the inventory matches exactly |
| **Encrypted workbooks** | `.xse` files are decrypted transparently; files protected with an additional password prompt for it on first load |
| **In-app manual** | Illustrated manual window shipped with the app (chapter list + content, standalone and maximizable), opened from the “About” page |
| **Tray & close behaviour** | Optionally minimize to the tray; closing asks “exit now / minimize to tray”, with a “don't ask again” option |
| **Widget animations** | Hover transitions, page transitions, collapsible cards, rolling numbers — can be disabled with one switch |
| **Single instance** | Only one instance runs at a time, so config and cache are never overwritten by a second copy |

> Files that cannot be parsed or are corrupt are skipped and summarized after loading — they never abort the load.

## Supported file formats

| Extension | Description |
|---|---|
| `.xlsx` | Excel 2007+ (multiple sheets supported) |
| `.xls` | Excel 97-2003 |
| `.csv` | Comma-separated text (encoding auto-detected) |
| `.docx` | Word documents (paragraph and table text is extracted) |
| `.xse` | This project's encrypted workbook format |

---

## Project layout and implementation

| Part | Files | Implementation |
|---|---|---|
| UI and application logic | `main.cpp` | Single-file Qt 6 Widgets implementation; QSS themes (light/dark plus accent color); frameless custom title bar (`startSystemMove` / `startSystemResize`); settings via `QSettings` (INI); single instance via `QLocalServer` / `QLocalSocket`; manual window rendered with `QTextBrowser::setMarkdown` |
| Search core | `search_engine.*` | C++17 indexing and retrieval: exact, regular expression and fuzzy matching (rapidfuzz scoring); both second-stage filter modes; blocking and marking; search history |
| Format readers | `xlsx_reader.*`, `xls_reader.*`, `csv_reader.*`, `docx_reader.*` | xlsx/docx: zip unpacking with miniz plus XML parsing with pugixml (including shared strings); xls: libxls (BIFF8); csv: hand-written delimiter/quote state machine; encoding conversion through win_iconv |
| Encrypted workbook codec | `xse_codec.*` | `XSE1` container: AES-256-GCM encryption with a PBKDF2-HMAC-SHA256 derived key (16-byte salt, 100,000 iterations); payload serialized in the project's own `XSPD` format |
| Platform crypto backend | `crypto_win.cpp` | Windows BCrypt implementation of the `core/crypto.h` interface: random bytes, PBKDF2, AES-256-GCM |
| Cross-platform base | `core/` | Abstraction layer for file I/O and the crypto interface (interface separated from the platform backend) |
| Pinyin matching | `pinyin.*`, `pinyin_table.inc`, `gen_pinyin.py` | Pinyin and initials matching against a generated lookup table (`gen_pinyin.py` produces the table) |
| Third-party libraries | `thirdparty/` | miniz (ZIP), pugixml (XML), libxls (legacy xls), win_iconv (encoding conversion), rapidfuzz (fuzzy matching) |
| Resources and manifest | `app.ico`, `app_icon.qrc`, `manual.qrc`, `app_qt.rc`, `app_qt.manifest` | Application icon, embedded manual copy, version resource, application manifest (`asInvoker` with PerMonitorV2 high-DPI support) |
| Build definition | `CMakeLists.txt` | CMake ≥ 3.20; Qt 6 modules Widgets / Network; `AUTOMOC` / `AUTORCC`; GUI subsystem (`WIN32_EXECUTABLE`); optional compile-time injection of a local secrets file |
| Deployment script | `tools/deploy.ps1` | Portable build packaging: complete the dependency closure → recursive dependency verification → launch test with a stripped PATH → collect third-party licenses → create an empty `data\` |
| Installer | `tools/build-installer.ps1`, `installer/setup.iss` | Built with Inno Setup 6; administrative install with optional shortcuts; `data\` is writable by regular users; two-page uninstall wizard; silent uninstall keeps user data |
| Self-check / smoke tests | `tools/selfcheck.ps1`, `tools/smoke.ps1` | Self-check drives the built-in offscreen hooks (`--report`, `--search`, `--filter`, `--shot`, …) and produces a screenshot matrix; smoke test creates malformed and very large files in an isolated folder and times loading and searching |
| Sample data | `data/`, `gen_test_files.ps1` | Synthetic regression fixtures (csv / xls / docx / xse) for local verification |
| Manual content | `MANUAL.md` | User-facing illustrated manual; shipped next to the executable, with an embedded fallback copy inside the binary |
| License and notices | `LICENSE`, `licenses/` | The project is GPL-3.0; third-party components and replacement instructions are listed in `licenses/THIRD-PARTY-NOTICES.md` |
| Documentation | `docs/` | Delivery verification report, open-source compliance review, and archived historical documents |
| Change log | `CHANGELOG.md` | Version change history |

---

## Building

### Requirements

- Windows 10 / 11 **x64**
- **Qt 6** (modules: Widgets, Network)
- **CMake ≥ 3.20** with **Ninja**
- Compiler: **MinGW-w64** (MSYS2, verified with g++ 16.1) or MSVC
- Optional: **Inno Setup 6** (to build the installer) and **PowerShell 5.1+** (deployment and self-check scripts)

### Build steps (MinGW example)

```powershell
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
cmake -G Ninja -S . -B build -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH=C:/msys64/mingw64 `
      -DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe
cmake --build build
```

Output: `build\bin\excel_search.exe`

- Make sure the application is **not running** while building, otherwise linking fails because the executable is locked;
- The program is a **GUI-subsystem** binary (no console), so scripts must launch it with `Start-Process -Wait` instead of relying on PowerShell's `&` to wait.

### Optional: compile-time password injection

The management password can be injected at build time so that real values never live in the source:

1. Copy `local_secrets.cmake.example` to `local_secrets.cmake` and set `ES_SUPER_PASSWORD` / `ES_DEFAULT_ADMIN_PASSWORD`;
2. `local_secrets.cmake` is ignored by `.gitignore` and is not published with the repository;
3. **The build works without that file**: the super-user channel stays disabled and the default admin password falls back to a weak built-in value (local development only).

---

## Deployment

### Portable build (no installation)

Running the compiler output directly requires Qt on the target machine. To ship it to a machine without Qt, deploy the runtime as well:

```powershell
powershell -ExecutionPolicy Bypass -File tools\deploy.ps1
```

The result is the portable build in `build\deploy\` — copy the whole folder and run it. It contains `excel_search.exe`,
the Qt runtime and plugins, the MinGW runtime, `licenses\` (full third-party license texts and the bundled-library
list), `MANUAL.md`, and an empty `data\`.

### Installer

```powershell
powershell -ExecutionPolicy Bypass -File tools\build-installer.ps1
```

Output: `build\installer\ExcelSearchSetup-<version>.exe`

- The script first makes sure the deployment output is up to date (**run `deploy.ps1` before packaging after code
  changes**, otherwise the installer would carry an older build);
- Installation: administrative install; install location and desktop / start-menu / taskbar shortcuts are optional;
  an empty `data\` is created and made writable for regular users;
- Uninstallation: a two-page wizard (page 1 selects what to remove, page 2 confirms); spreadsheet data and user
  settings can be kept; **silent uninstall (`/VERYSILENT`) shows no UI and keeps user data**.

---

## Running and configuration

1. Put your spreadsheets into the **`data\`** folder next to the executable (created by the installer);
2. Start the application and click “Reload” (the first launch loads automatically);
3. Type a keyword, then press Enter or click “Search”; use the second-stage filter to narrow the results.

- The application runs with **regular user privileges** (`asInvoker`); no administrator rights are needed;
- Settings and cache live in **`%APPDATA%\ExcelSearch\`** (`config.ini`, `cache.dat`, `cache.inv`);
- Settings stored in the registry by the older V0.3.0 are **migrated automatically on first launch** into
  `config.ini` (the registry entries are kept read-only).

---

## License and third-party components

- This project is released under **GPL-3.0**; the full text is in [`LICENSE`](LICENSE);
- **Qt 6** is used through **dynamic linking** (LGPL-3.0); the following are compiled in: miniz (MIT),
  pugixml (MIT), rapidfuzz (MIT), libxls (BSD), win_iconv (Public Domain);
- The component list, copyright notices and replacement instructions are in
  [`licenses/THIRD-PARTY-NOTICES.md`](licenses/THIRD-PARTY-NOTICES.md); the full license texts are collected by
  `tools\deploy.ps1` at packaging time and shipped in the `licenses\` folder of the release;
- The visual style of the UI and the interaction of the manual dialog are inspired by
  [MAA / MaaWpfGui](https://github.com/MaaAssistantArknights/MaaAssistantArknights) and
  [MaaEnd](https://github.com/MaaEnd/MaaEnd); **no source code, style sheets or assets from those projects are
  used or copied** — see [`docs/LICENSE-COMPLIANCE.md`](docs/LICENSE-COMPLIANCE.md) for the review record.
