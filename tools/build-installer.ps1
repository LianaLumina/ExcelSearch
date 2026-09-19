# 安装包构建脚本（Inno Setup 6）—— 带自动重试
# ---------------------------------------------------------------------------
# 为什么需要重试：
#   实测 Inno 编译偶尔报 `Resource update error: EndUpdateResource failed ... (110)`，
#   发生在 "Updating icons (Setup.exe)" 阶段。**不是脚本错误、也不是图标错误**，而是
#   Windows Defender 实时扫描抢占了刚生成的 Setup.exe 的文件句柄（Inno 的报错提示也指向杀软）。
#   实测同一命令**重试一次即成功**，故这里自动重试。
#   若长期频繁失败，可把输出目录加入杀软排除项（属于系统改动，请自行决定）。
#
# 前置：先跑 tools\deploy.ps1 生成 build\deploy\（否则安装包会打进旧程序！）
# 用法：powershell -ExecutionPolicy Bypass -File tools\build-installer.ps1
param(
    [int]$Retries = 3,
    [int]$DelaySec = 3
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$iss  = Join-Path $root "installer\setup.iss"
$outDir = Join-Path $root "build\installer"
$iscc = "C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $iscc)) { throw "找不到 Inno Setup：$iscc" }
if (-not (Test-Path (Join-Path $root "build\deploy\excel_search.exe"))) {
    throw "找不到 build\deploy\ —— 请先运行 tools\deploy.ps1"
}
# 提醒：部署目录里的 exe 必须比源码新，否则会把旧程序打进安装包
$exeTime = (Get-Item (Join-Path $root "build\deploy\excel_search.exe")).LastWriteTime
$srcTime = (Get-Item (Join-Path $root "main.cpp")).LastWriteTime
if ($srcTime -gt $exeTime) {
    Write-Host ("⚠ main.cpp（{0:HH:mm:ss}）比部署目录的 exe（{1:HH:mm:ss}）新 —— 请先跑 tools\deploy.ps1" -f $srcTime, $exeTime) -ForegroundColor Yellow
}

$ok = $false
for ($i = 1; $i -le $Retries; $i++) {
    Write-Host ("[尝试 {0}/{1}] 编译安装包…" -f $i, $Retries) -ForegroundColor Cyan
    Remove-Item $outDir -Recurse -Force -ErrorAction SilentlyContinue   # 旧产物被占用会导致资源更新失败
    Start-Sleep -Milliseconds 500
    $prev = $ErrorActionPreference; $ErrorActionPreference = "Continue"
    $r = & $iscc $iss 2>&1
    $code = $LASTEXITCODE
    $ErrorActionPreference = $prev
    if ($code -eq 0 -and (Test-Path (Join-Path $outDir "ExcelSearchSetup-0.3.2.exe"))) {
        $ok = $true
        Write-Host "  ✅ 编译成功" -ForegroundColor Green
        break
    }
    $r | Select-String -Pattern 'Error|Warning' | Select-Object -Last 2 | ForEach-Object { Write-Host ("  " + $_.Line.Trim()) -ForegroundColor DarkYellow }
    if ($i -lt $Retries) { Write-Host ("  失败（多为杀软瞬时占用），{0} 秒后重试…" -f $DelaySec) -ForegroundColor Yellow; Start-Sleep -Seconds $DelaySec }
}
if (-not $ok) { Write-Host "❌ 多次重试仍失败：可把 build\installer 加入杀软排除项后重试" -ForegroundColor Red; exit 1 }

$f = Get-Item (Join-Path $outDir "ExcelSearchSetup-0.3.2.exe")
Write-Host ("安装包：{0}  {1:N2} MB  {2:HH:mm:ss}" -f $f.FullName, ($f.Length/1MB), $f.LastWriteTime) -ForegroundColor Cyan
