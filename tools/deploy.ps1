# V0.3.2 部署脚本（Qt 运行时打包 + 依赖闭包补齐 + 校验 + 无 Qt 环境启动测试）
# ---------------------------------------------------------------------------
# 为什么不能只用 windeployqt（实测教训，务必保留这段说明）：
#   MSYS2 的 windeployqt **不复制 MinGW 运行时**（libgcc/libstdc++/libwinpthread），
#   也**不解析 Qt 的传递依赖**（freetype / harfbuzz / libpng / icu / pcre2 / zstd / zlib / md4c…）。
#   实测只跑 windeployqt 会缺 12 个 DLL，拷到没装 Qt 的电脑上启动直接 0xC0000135。
#   因此本脚本在 windeployqt 之后做**依赖闭包补齐**：递归解析每个模块的依赖，
#   缺哪个就从 msys64 拷哪个，直到完全没有缺失。
#
# 步骤：
#   1) 构建  2) 准备目录  3) windeployqt  4) 依赖闭包补齐
#   5) 收录开源许可文本（Qt LGPL 全文 + 各库 + 实际部署的 msys64 依赖包）+ 生成空 data\
#   6) 递归依赖校验 + **PATH 剥离启动测试**（模拟没装 Qt 的电脑）
#
# 用法：
#   powershell -ExecutionPolicy Bypass -File tools\deploy.ps1
#   powershell -ExecutionPolicy Bypass -File tools\deploy.ps1 -SkipBuild
# 产物：build\deploy\  （可直接压 zip 作"绿色免安装版"分发；也是安装包的输入）
param(
    [string]$Src     = "",
    [string]$OutDir  = "",
    [switch]$SkipBuild
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if ($Src -eq "")    { $Src = Join-Path $root "build\bin\excel_search.exe" }
if ($OutDir -eq "") { $OutDir = Join-Path $root "build\deploy" }

$MSYS        = "C:\msys64\mingw64"
$WINDEPLOYQT = Join-Path $MSYS "bin\windeployqt.exe"
$OBJDUMP     = Join-Path $MSYS "bin\objdump.exe"
$MSYSBIN     = Join-Path $MSYS "bin"
foreach ($t in @($WINDEPLOYQT, $OBJDUMP)) { if (-not (Test-Path $t)) { throw "缺少工具：$t" } }

# 原生命令包装：PS 5.1 在 $ErrorActionPreference="Stop" 下会把 stderr 当终止错误，
# 而 windeployqt / objdump 会往 stderr 打无害警告 —— 统一吞掉 stderr，只取退出码与输出。
function Invoke-Native([string]$exe, [string[]]$cmdArgs) {
    $prev = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    $out = & $exe @cmdArgs 2>&1
    $code = $LASTEXITCODE
    $ErrorActionPreference = $prev
    return @{ Out = $out; Code = $code }
}
function Get-DllDeps([string]$file) {
    (Invoke-Native $OBJDUMP @("-p", $file)).Out | Select-String 'DLL Name:' |
        ForEach-Object { ($_.Line -replace '.*DLL Name:\s*','').Trim() }
}
function Test-SystemDll([string]$name) {
    if ($name -match '^(api-ms-|ext-ms-)') { return $true }
    return (Test-Path (Join-Path $env:WINDIR "System32\$name"))
}
# 递归解析部署目录里所有模块的依赖，返回缺失项（名 → 谁需要它）
function Get-MissingDeps([string]$dir) {
    $seen = @{}; $queue = New-Object System.Collections.Queue
    Get-ChildItem $dir -File | Where-Object { $_.Extension -in '.exe','.dll' } | ForEach-Object { $queue.Enqueue($_.FullName) }
    $missing = [ordered]@{}
    while ($queue.Count -gt 0) {
        $f = $queue.Dequeue()
        if ($seen[$f]) { continue }
        $seen[$f] = $true
        foreach ($d in Get-DllDeps $f) {
            if (Test-SystemDll $d) { continue }
            $p = Join-Path $dir $d
            if (Test-Path $p) { $queue.Enqueue($p) }
            elseif (-not $missing.Contains($d)) { $missing[$d] = (Split-Path $f -Leaf) }
        }
    }
    return $missing
}

# ---------- 1) 构建 ----------
if (-not $SkipBuild) {
    Write-Host "[1/6] 构建..." -ForegroundColor Cyan
    $env:PATH = "$MSYSBIN;" + $env:PATH
    Get-Process excel_search -ErrorAction SilentlyContinue | Stop-Process -Force
    (Invoke-Native "cmake" @("--build", (Join-Path $root "build"))).Out | Select-Object -Last 1 |
        ForEach-Object { Write-Host "      $_" }
} else { Write-Host "[1/6] 跳过构建" -ForegroundColor DarkGray }
if (-not (Test-Path $Src)) { throw "找不到 exe：$Src" }

# ---------- 2) 准备目录 ----------
Write-Host "[2/6] 准备部署目录..." -ForegroundColor Cyan
if (Test-Path $OutDir) { Remove-Item $OutDir -Recurse -Force }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Copy-Item $Src (Join-Path $OutDir "excel_search.exe") -Force

# ---------- 3) windeployqt ----------
Write-Host "[3/6] windeployqt（Qt DLL + 平台插件）..." -ForegroundColor Cyan
$env:PATH = "$MSYSBIN;" + $env:PATH
$wd = Invoke-Native $WINDEPLOYQT @("--release","--no-translations","--compiler-runtime","--no-opengl-sw",
                                   "--no-system-d3d-compiler", (Join-Path $OutDir "excel_search.exe"))
$wd.Out | Where-Object { $_ -match 'Cannot|error' } | Select-Object -First 4 | ForEach-Object { Write-Host "      $_" }
if (-not (Test-Path (Join-Path $OutDir "platforms\qwindows.dll"))) { throw "windeployqt 未产出 platforms\qwindows.dll" }
# ★额外平台插件：windeployqt 只带 qwindows。我们的自检钩子（--shot/--report）用 offscreen，
#   若不部署 qoffscreen，部署版跑无头钩子会弹 Qt 致命框
#   "no Qt platform plugin could be initialized" 并卡在模态框上（实测踩过，见 docs 记录）。
foreach ($pl in @("qoffscreen.dll","qminimal.dll")) {
    $ps = Join-Path $MSYS "share\qt6\plugins\platforms\$pl"
    if (Test-Path $ps) { Copy-Item $ps (Join-Path $OutDir "platforms\$pl") -Force }
}
Write-Host ("      ✅ 平台插件：" + ((Get-ChildItem (Join-Path $OutDir 'platforms') -Filter *.dll |
    ForEach-Object { $_.Name }) -join ', ')) -ForegroundColor Green

# ---------- 4) 依赖闭包补齐 ----------
Write-Host "[4/6] 依赖闭包补齐（递归拉齐 msys64 依赖）..." -ForegroundColor Cyan
$iter = 0; $copied = @()
while ($iter -lt 12) {
    $iter++
    $missing = Get-MissingDeps $OutDir
    if ($missing.Count -eq 0) { break }
    $progress = $false
    foreach ($d in $missing.Keys) {
        $srcDll = Join-Path $MSYSBIN $d
        if (Test-Path $srcDll) { Copy-Item $srcDll $OutDir -Force; $copied += $d; $progress = $true }
    }
    if (-not $progress) { break }   # 剩下的 msys64 里也没有 → 交给第 6 步报错
}
if ($copied.Count -gt 0) {
    Write-Host ("      ✅ 补齐 {0} 个依赖（{1} 轮收敛）" -f $copied.Count, $iter) -ForegroundColor Green
    ($copied | Sort-Object -Unique) | ForEach-Object { Write-Host "         + $_" }
} else { Write-Host "      （无需补齐）" }

# ---------- 5) 许可文本 + 空 data ----------
Write-Host "[5/6] 收录开源许可 + 生成空 data\ ..." -ForegroundColor Cyan
$licOut = Join-Path $OutDir "licenses"
New-Item -ItemType Directory -Force -Path $licOut | Out-Null
Copy-Item (Join-Path $root "licenses\*") $licOut -Recurse -Force
Copy-Item (Join-Path $root "LICENSE") $OutDir -Force -ErrorAction SilentlyContinue
# 使用说明书：随包放在 exe 同目录（可外部替换，无需重编译；缺省则程序用内嵌副本）
Copy-Item (Join-Path $root "MANUAL.md") $OutDir -Force -ErrorAction SilentlyContinue
# 说明书插图（可选）：若存在则一并带上 —— 手册窗口左下角的装饰位会自动显示
if (Test-Path (Join-Path $root "说明书插图.png")) { Copy-Item (Join-Path $root "说明书插图.png") $OutDir -Force }

# Qt 许可证全文（本机 Qt 安装内最权威）
$qtLic = Join-Path $MSYS "share\licenses\qt6-base"
foreach ($f in @("LGPL-3.0-only.txt","GPL-3.0-only.txt","Qt-GPL-exception-1.0.txt")) {
    $p = Join-Path $qtLic $f
    if (Test-Path $p) { Copy-Item $p (Join-Path $licOut ("Qt-" + $f)) -Force }
}
# 自带库：逐字取源码文件头的版权/许可声明（避免手抄出错）
$licTxt = @("# 自带第三方库的许可与版权声明（逐字取自各库源码文件头）", "")
foreach ($rel in @("thirdparty\miniz.c","thirdparty\pugixml.cpp","thirdparty\libxls\src\xls.c","thirdparty\win_iconv\win_iconv.c")) {
    $p = Join-Path $root $rel
    if (-not (Test-Path $p)) { continue }
    $licTxt += ("=" * 70); $licTxt += "文件：$rel"; $licTxt += ("=" * 70)
    $licTxt += (Get-Content $p -TotalCount 40 -Encoding UTF8 | Where-Object { $_ -match 'Copyright|Licen|licen|public domain|SPDX|permission' })
    $licTxt += ""
}
if (Test-Path (Join-Path $root "thirdparty\rapidfuzz\LICENSE")) {
    $licTxt += ("=" * 70); $licTxt += "文件：thirdparty\rapidfuzz\LICENSE"; $licTxt += ("=" * 70)
    $licTxt += (Get-Content (Join-Path $root "thirdparty\rapidfuzz\LICENSE") -Encoding UTF8)
}
[IO.File]::WriteAllLines((Join-Path $licOut "BUNDLED-LIBRARIES.txt"), $licTxt, (New-Object Text.UTF8Encoding($false)))

# 实际部署的 msys64 依赖包 → 收录其许可证目录
$msysLic = Join-Path $MSYS "share\licenses"
# DLL 真名普遍带 lib 前缀（libbrotlicommon.dll / libicudt78.dll …），故用正则匹配
$pkgGuess = [ordered]@{
    '^qt6'='qt6-base'; '^libqt6'='qt6-base'; '^libpng'='libpng'; '^libfreetype'='freetype';
    '^libharfbuzz'='harfbuzz'; '^zlib'='zlib'; '^libb2'='libb2'; '^libdouble-conversion'='double-conversion';
    '^libpcre2'='pcre2'; '^libbrotli'='brotli'; '^libzstd'='zstd'; '^libmd4c'='md4c';
    '^libgraphite2'='graphite2'; '^libicu'='icu'; '^libiconv'='libiconv'; '^libintl'='gettext';
    '^libglib'='glib2'; '^libbz2'='bzip2'; '^libunistring'='libunistring'; '^gettext'='gettext';
    '^libgcc'='gcc-libs'; '^libstdc'='gcc-libs'; '^libwinpthread'='gcc-libs'; '^libssp'='gcc-libs'
}
$msysOut = Join-Path $licOut "msys2"
New-Item -ItemType Directory -Force -Path $msysOut | Out-Null
$copiedPkg = @(); $unknownDll = @()
Get-ChildItem $OutDir -Filter "*.dll" | ForEach-Object {
    $n = $_.Name.ToLower(); $matched = $false
    foreach ($k in $pkgGuess.Keys) {
        if ($n -match $k) {
            $matched = $true; $pk = $pkgGuess[$k]
            if ($copiedPkg -notcontains $pk -and (Test-Path (Join-Path $msysLic $pk))) {
                Copy-Item (Join-Path $msysLic $pk) (Join-Path $msysOut $pk) -Recurse -Force
                $copiedPkg += $pk
            }
        }
    }
    if (-not $matched) { $unknownDll += $_.Name }
}
New-Item -ItemType Directory -Force -Path (Join-Path $OutDir "data") | Out-Null
Write-Host ("      许可包 {0} 个：{1}" -f $copiedPkg.Count, (($copiedPkg | Sort-Object) -join ', '))
if ($unknownDll.Count -gt 0) { Write-Host ("      ⚠ 未匹配到许可证包的 DLL（需人工确认）：" + (($unknownDll | Sort-Object) -join ', ')) -ForegroundColor Yellow }

# ---------- 6) 依赖校验 + PATH 剥离启动测试 ----------
Write-Host "[6/6] 依赖校验 + 无 Qt 环境启动测试..." -ForegroundColor Cyan
$missing = Get-MissingDeps $OutDir
if ($missing.Count -gt 0) {
    Write-Host "      ❌ 仍缺失依赖：" -ForegroundColor Red
    $missing.Keys | Sort-Object | ForEach-Object { Write-Host ("         {0}  (被 {1} 需要)" -f $_, $missing[$_]) -ForegroundColor Red }
} else {
    Write-Host "      ✅ 依赖完备（除 Windows 系统 DLL 外无缺失）" -ForegroundColor Green
}
$savedPath = $env:PATH
$env:PATH = (($env:PATH -split ';') | Where-Object { $_ -notmatch 'msys64' }) -join ';'
$env:QT_QPA_PLATFORM = "offscreen"
$testOut = Join-Path $env:TEMP "deploy_selftest.txt"
Remove-Item $testOut -Force -ErrorAction SilentlyContinue
# 超时保护：进程若卡住（多半是弹了 Qt 致命错误框）不能无限等，否则脚本会挂死
$p = Start-Process -FilePath (Join-Path $OutDir "excel_search.exe") -ArgumentList @('--report', $testOut) -NoNewWindow -PassThru
$exited = $p.WaitForExit(30000)
if (-not $exited) { $p | Stop-Process -Force; Start-Sleep -Milliseconds 400 } else { $p.WaitForExit() }   # 无参再等一次：.NET 需此步状态才最终化
if ($exited) { $exitCode = $p.ExitCode } else { $exitCode = -1 }   # PS 5.1 不支持 if 表达式
$ok = (Test-Path $testOut) -and ((Get-Content $testOut -Raw) -match 'files=')
if ($ok) {
    Write-Host ("      ✅ 通过：无 Qt 环境下正常启动（退出码 {0}）" -f $(if ($null -eq $exitCode) { "n/a" } else { ("0x{0:X8}" -f $exitCode) })) -ForegroundColor Green
    Get-Content $testOut | Select-String 'files=|entries=|anim=' | ForEach-Object { Write-Host "         $_" }
} elseif (-not $exited) {
    Write-Host "      ❌ 超时 30s 未退出（很可能弹了 Qt 致命错误框，已强制结束）" -ForegroundColor Red
} else {
    Write-Host ("      ❌ 失败：退出码 0x{0:X8}（0xC0000135=缺DLL；0xC0000142=DLL初始化失败，常见于平台插件缺失）" -f $exitCode) -ForegroundColor Red
}
$env:PATH = $savedPath
$env:QT_QPA_PLATFORM = ""

# ---------- 汇总 ----------
$size = (Get-ChildItem $OutDir -Recurse -File | Measure-Object -Property Length -Sum).Sum
$cnt  = (Get-ChildItem $OutDir -Recurse -File).Count
Write-Host ""
Write-Host ("部署完成：{0}" -f $OutDir) -ForegroundColor Cyan
Write-Host ("  文件 {0} 个 · 共 {1:N1} MB" -f $cnt, ($size/1MB))
if ($missing.Count -gt 0 -or -not $ok) { exit 1 }
