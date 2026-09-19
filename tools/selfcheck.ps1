# 自检 / 回归脚本（qt_ui_proto 专用）
# ---------------------------------------------------------------------------
# 为什么需要它：本程序是 GUI 子系统（不分配控制台），PowerShell 的 & 不会等待进程结束，
# 且存在单实例闸门（有实例在跑时钩子直接 error=another-instance-running）。
# 本脚本统一处理：先杀残留实例 → 离屏(offscreen) → Start-Process -Wait → 结果写文件。
#
# 用法（本机只有 Windows PowerShell 5.1，脚本已存为 UTF-8 with BOM；pwsh 7 亦可）：
#   powershell -ExecutionPolicy Bypass -File tools\selfcheck.ps1                 # 动效开启（默认）
#   powershell -ExecutionPolicy Bypass -File tools\selfcheck.ps1 -NoAnim        # 关闭动效（UI_PROTO_NO_ANIM=1）
#   powershell -ExecutionPolicy Bypass -File tools\selfcheck.ps1 -NoAnim -SkipShots
#
# 产物：默认写到 build\selfcheck\（-NoAnim 时 build\selfcheck-noanim\），含
#   report.txt / search.txt / filter.txt / colprobe.txt / shot-*.png
# 注意：脚本**不会**动用户配置里的屏蔽/标记；也**不会**执行 --clearmarks。
param(
    [string]$Exe    = "<repo>\build\bin\excel_search.exe",
    [string]$OutDir = "",
    [switch]$NoAnim,
    [switch]$SkipShots,
    [switch]$SkipBusiness
)
$ErrorActionPreference = "Stop"
$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
if ($OutDir -eq "") {
    # 注意：PS 5.1 不支持把 if 当表达式用（pwsh 7 才支持），这里必须写成显式分支
    if ($NoAnim) { $sub = "build\selfcheck-noanim" } else { $sub = "build\selfcheck" }
    $OutDir = Join-Path $root $sub
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Get-Process excel_search -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 300

$env:QT_QPA_PLATFORM = "offscreen"
if ($NoAnim) { $env:EXCELSEARCH_NO_ANIM = "1" } else { Remove-Item Env:\EXCELSEARCH_NO_ANIM -ErrorAction SilentlyContinue }

# 注意：不能把参数命名为 $Args —— 那是 PowerShell 的自动变量，赋值会得到 null 集合
function Run-Hook([string[]]$HookArgs, [string]$Name) {
    $proc = Start-Process -FilePath $Exe -ArgumentList $HookArgs -Wait -NoNewWindow -PassThru
    Write-Host ("[{0}] exit={1}" -f $Name, $proc.ExitCode)
}

$out = $OutDir
if (-not $SkipBusiness) {
    Run-Hook @("--report", "$out\report.txt")                     "report"
    Run-Hook @("--search", "工日", "--out", "$out\search.txt")     "search 工日"
    Run-Hook @("--search", "工日", "--filter", "辅助,检修", "--out", "$out\filter.txt") "filter 辅助,检修"
    Run-Hook @("--colprobe", "核定工日", "--out", "$out\colprobe.txt") "colprobe"
}

if (-not $SkipShots) {
    # 截图矩阵：搜索页 / 通用设置 / 搜索设置 / 共享设置 / 高级设置解锁层 / 关闭对话框 × 浅色·深色
    $shots = @(
        @("search-light",  @("--light", "--kw", "工日")),
        @("search-dark",   @("--dark",  "--kw", "工日")),
        @("empty-light",   @("--light", "--kw")),
        @("sec0-light",    @("--light", "--page", "settings", "--sec", "0")),
        @("sec0-dark",     @("--dark",  "--page", "settings", "--sec", "0")),
        @("sec1-dark",     @("--dark",  "--page", "settings", "--sec", "1")),
        @("sec4-light",    @("--light", "--page", "settings", "--sec", "4")),
        @("sec4-dark",     @("--dark",  "--page", "settings", "--sec", "4")),
        @("advlock-light", @("--light", "--page", "adv")),
        @("advlock-dark",  @("--dark",  "--page", "adv")),
        @("advsec0-light", @("--light", "--page", "adv", "--advsec", "0")),
        @("closedlg-light",@("--light", "--page", "closedlg")),
        @("closedlg-dark", @("--dark",  "--page", "closedlg"))
    )
    foreach ($s in $shots) {
        $png = Join-Path $out ("shot-" + $s[0] + ".png")
        Run-Hook (@("--shot", $png) + $s[1]) ("shot " + $s[0])
    }
}
Write-Host "输出目录：$out"
