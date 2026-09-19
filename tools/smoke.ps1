# 性能与健壮性冒烟脚本（qt_ui_proto 专用）
# ---------------------------------------------------------------------------
# 造一套"畸形 + 超大"的隔离数据集（不动用户 data\），跑加载/缓存/搜索并计时。
# 用法：
#   powershell -ExecutionPolicy Bypass -File tools\smoke.ps1
#   powershell -ExecutionPolicy Bypass -File tools\smoke.ps1 -Rows 50000   # 调整大文件行数
# 产物：
#   <smoke-dir>\bin\data\...   隔离数据集（含坏文件与 10 万行 CSV）
#   %TEMP%\smoke-*.txt            各步输出
# 注意：
#   - 会覆盖 %APPDATA%\ui_proto\ExcelSearch\cache.*（缓存是按数据清单重新生成的）；
#     跑完请用真实 exe 跑一次 --report 把用户数据的缓存重建回来。
#   - 绝不执行 --clearmarks（会清掉用户配置里的屏蔽/标记）。
param(
    [string]$Exe     = "<repo>\build\bin\excel_search.exe",
    [string]$Root    = "<smoke-dir>",
    [int]   $Rows    = 100000,
    [string]$RealData = "<repo>\build\bin\data"
)
$ErrorActionPreference = "Stop"
$env:QT_QPA_PLATFORM = "offscreen"
Get-Process excel_search -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300

Remove-Item $Root -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path "$Root\bin\data" | Out-Null
$data = "$Root\bin\data"
Copy-Item $Exe "$Root\bin\ui_proto.exe" -Force
$smokeExe = "$Root\bin\ui_proto.exe"

# ---- 造数据集：正常 + 各类坏文件 + 超大 CSV ----
$ok = Get-ChildItem $RealData -Filter *.xlsx | Select-Object -First 1
if ($ok) { Copy-Item $ok.FullName "$data\ok.xlsx" }
New-Item -ItemType File -Path "$data\bad_zero.xlsx" -Force | Out-Null
$rnd = New-Object System.Random 42
$b = New-Object byte[] 8192; $rnd.NextBytes($b); [IO.File]::WriteAllBytes("$data\bad_random.xlsx", $b)
$b2 = New-Object byte[] 4096; $rnd.NextBytes($b2); $b2[0]=0x50; $b2[1]=0x4B; $b2[2]=0x03; $b2[3]=0x04
[IO.File]::WriteAllBytes("$data\bad_zip.xlsx", $b2)          # PK 头 + 垃圾（半坏 zip）
$b3 = New-Object byte[] 2048; $rnd.NextBytes($b3)
[IO.File]::WriteAllBytes("$data\bad.xse", $b3)
[IO.File]::WriteAllBytes("$data\bad.docx", $b3)
[IO.File]::WriteAllBytes("$data\bad.xls", $b3)
[IO.File]::WriteAllText("$data\empty.csv", "")
[IO.File]::WriteAllText("$data\header_only.csv", "姓名,部门,核定工日`r`n")
if ($ok) { Copy-Item $ok.FullName "$data\带 空格 的 文件 (1).xlsx" }
$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("序号,姓名,部门,工日,备注")
for ($i = 1; $i -le $Rows; $i++) { [void]$sb.AppendLine("$i,张三$i,检修部$($i % 20),$($i % 50),大修工日核定备注文本$i") }
[IO.File]::WriteAllText("$data\large.csv", $sb.ToString(), (New-Object Text.UTF8Encoding($false)))

function Shot([string]$name, [string[]]$a) {
    $t = Measure-Command { Start-Process -FilePath $smokeExe -ArgumentList $a -Wait -NoNewWindow }
    Write-Host ("{0}: {1:N0} ms" -f $name, $t.TotalMilliseconds)
}

Write-Host "数据集：$((Get-ChildItem $data).Count) 个文件"
Shot "全量加载#1(应 miss)" @("--report", "$env:TEMP\smoke-load1.txt")
Get-Content "$env:TEMP\smoke-load1.txt" | Select-String 'files=|entries=|skipped=|cache='
Shot "全量加载#2(应 hit) " @("--report", "$env:TEMP\smoke-load2.txt")
Shot "搜索(大结果集)     " @("--search", "工日", "--out", "$env:TEMP\smoke-search.txt")
Get-Content "$env:TEMP\smoke-search.txt" | Select-String 'hits='
Shot "搜索(短词/最坏情况) " @("--search", "张三", "--out", "$env:TEMP\smoke-search2.txt")
Get-Content "$env:TEMP\smoke-search2.txt" | Select-String 'hits='
Shot "搜索(长词/小结果集) " @("--search", "电动机定期检查", "--out", "$env:TEMP\smoke-search3.txt")
Get-Content "$env:TEMP\smoke-search3.txt" | Select-String 'hits='

Write-Host "=== 把用户数据的缓存重建回来 ==="
Start-Process -FilePath $Exe -ArgumentList @("--report", "$env:TEMP\smoke-restore.txt") -Wait -NoNewWindow
Start-Process -FilePath $Exe -ArgumentList @("--report", "$env:TEMP\smoke-restore2.txt") -Wait -NoNewWindow
Get-Content "$env:TEMP\smoke-restore2.txt" | Select-String 'files=|entries=|cache='
