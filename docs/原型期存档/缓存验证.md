# 缓存命中/二次加载 —— 手动验证步骤

验证目标：**在「正确性优先」的前提下，缓存既能命中复用，又绝不漏掉任何文件。**

对应改动：`main.cpp` 中 `collectDiskFiles` / `writeInventory` / `readInventory` / `tryLoadCache`，
缓存为**两份文件**——`cache.dat`（引擎索引块）+ `cache.inv`（全量清单，缓存有效性的唯一凭据）。

---

## 0. 准备

```powershell
# 编译（确保验证的是最新代码）
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
cmake --build <prototype>\build
```

**关键路径**

| 项 | 路径 |
|---|---|
| 程序 | `<prototype>\build\bin\ui_proto.exe` |
| 数据目录 | `<prototype>\build\bin\data\` |
| 缓存目录 | `%APPDATA%\ui_proto\ExcelSearch\`（内含 `cache.dat` / `cache.inv` / `config.ini`） |

> 数据目录基准（当前测试集）：6 个文件 = 4 个非 `.xse` + 2 个 `.xse`（其中 1 个需附加密码，会被跳过）。
> 期望基线：`files=5 entries=43846 skipped=1`。

---

## 方案 A：无头自检（推荐，最快）

整段贴入 PowerShell：

```powershell
$env:PATH = "C:\msys64\mingw64\bin;" + $env:PATH
$env:QT_QPA_PLATFORM = "offscreen"
$exe  = "<prototype>\build\bin\ui_proto.exe"
$data = "<prototype>\build\bin\data"
$cdir = "$env:APPDATA\ui_proto\ExcelSearch"
$rep  = "$env:TEMP\cache_check.txt"

function Check($name) {
  & $exe --report $rep | Out-Null
  Write-Host ("{0,-30} {1}" -f $name, ((Get-Content $rep) -join '  '))
}

# --- 干净起点 ---
Remove-Item "$cdir\cache.dat","$cdir\cache.inv" -ErrorAction SilentlyContinue

Check "1) 首次启动"
Check "2) 二次启动"

Copy-Item "$data\105大修工日审查电气（初版）中核检修.xse" "$data\TT_新增加密.xse"
Check "3) 新增 .xse"
Remove-Item "$data\TT_新增加密.xse"
Check "4) 删除该 .xse"
Check "5) 再次启动"

(Get-Item "$data\106大修工日.xlsx").LastWriteTime = (Get-Date)
Check "6) 改动文件时间"
```

### 逐步预期

| 步骤 | 操作 | 期望输出 | 验证点 |
|---|---|---|---|
| 1 | 删除两个缓存文件后启动 | `cache=miss`，`files=5 entries=43846 skipped=1` | 全量解析并写出两份缓存 |
| 2 | 立刻再启动 | **`cache=hit`**，其余同上 | **核心**：磁盘上有「跳过态 `.xse`」仍能命中 |
| 3 | 复制出一个新 `.xse` 后启动 | **`cache=miss`**，`skipped=2` | **正确性**：新增 `.xse` 被察觉（旧放宽逻辑会漏掉） |
| 4 | 删掉该 `.xse` 后启动 | `cache=miss` | 上一步清单=7 文件，现在磁盘=6 → 不一致判 miss（正确） |
| 5 | 再启动 | **`cache=hit`**，`skipped=1` | 状态重新稳定 |
| 6 | 改某文件 `LastWriteTime` 后启动 | **`cache=miss`** | 改动检测生效 |

**通过标准**：`miss → hit → miss(skipped=2) → miss → hit → miss`，
且各步 `entries` 恒为 `43846`（新增那个文件是跳过态、不进索引，所以 `files` 仍为 5）。

---

## 方案 B：GUI 目视（验证真实交互文案）

```powershell
$exe = "<prototype>\build\bin\ui_proto.exe"
Remove-Item "$env:APPDATA\ui_proto\ExcelSearch\cache.dat","$env:APPDATA\ui_proto\ExcelSearch\cache.inv" -ErrorAction SilentlyContinue
& $exe
```

- **第 1 次**：底部状态栏 `正在加载 N/6：xxx` → `已加载 5 个文件 · 共 43846 条记录`；「数据概览」卡片 文件=5、索引=43846。约 0.5s 出结果。
- **第 2 次**（不删缓存）：状态栏应为 **`已加载(缓存) 5 个文件 · 共 43846 条记录`**，明显更快。
- **第 3 次**：往 `data\` 放一个新 `.xse` 再启动 → 状态栏为普通文案并带 `（1 个加密文件需附加密码未加载）`，
  **不含"缓存"字样**（miss）。
- 搜一个关键词（如 `工日`）确认结果条数与 `entries` 量级一致，确保缓存恢复的索引确实可搜。

---

## 附加：缓存文件本身核验

```powershell
$cdir = "$env:APPDATA\ui_proto\ExcelSearch"
Get-ChildItem $cdir | Select-Object Name,Length,LastWriteTime
# 魔数：cache.dat 以 EXSR2 开头，cache.inv 以 EXIV1 开头
-join ([IO.File]::ReadAllBytes("$cdir\cache.inv")[0..4] | % { [char]$_ })
```

预期：`cache.dat` ≈ 4.4 MB；`cache.inv` 仅数百字节（每个文件一条记录）。

**边界（可选）**

- **缓存损坏**：把 `cache.inv` 写成乱码 → 启动应 `miss`，随后自动重建并恢复 `hit`。
- **TTL 过期**：`cache.inv` 头 5 字节 `EXIV1` 之后是 8 字节写入时间戳（小端 int64 秒）；
  改为很久以前即可验证「超 10 天必 miss」（不便手改时可跳过）。
- **命中性能**：清缓存与不清缓存各跑一次计时对比（参考：全量重载 ~505ms、命中 ~163ms）。

---

## 相关代码位置

| 位置 | 作用 |
|---|---|
| `DiskFile` / `collectDiskFiles` | 扫描数据目录，取 `fn/mtime/size`；`mtime` 用原始 `file_clock` 计数（稳定可复现） |
| `writeInventory` / `readInventory` | 读写 `cache.inv`（`EXIV1` 格式：ts + count + (fn,mtime,size)*） |
| `tryLoadCache` | 清单与磁盘**逐项精确相等** + 10 天 TTL → `loadFromFile(cache.dat)` |
| `onLoadFinished` | 写 `cache.dat` 后再写 `cache.inv`（后者作为提交标记） |
