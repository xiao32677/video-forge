# =============================================================================
#  VideoForge - 生成文档用的界面截图
#
#  背景与两条约束：
#
#  1) 演示素材用 testdata/ 里的**真实视频**，不用合成彩条 —— 截图是给评审看的，
#     彩条上根本看不出界面好不好用（换成真实素材后，立刻就暴露了信息表只显示
#     一行的问题）。
#
#  2) 一次启动程序可以产出多张截图，所以这里只跑 **4 轮**而不是 6 轮：
#       A 深色 + 转换  -> 01 主界面（空闲态）、02 批量转换完成
#       B 浅色 + 转换  -> 04 浅色主题（空闲态）、03 转换中
#       C 深色 + 音频  -> 05 音频提取
#       D 深色 + 小窗  -> 06 小屏适配
#     程序在 --screenshot 模式下会把窗口挪到屏幕外（WA_DontShowOnScreen），
#     所以跑脚本不会再反复弹窗抢焦点（想亲眼看加 --show-window）。
#
#  用法：
#      powershell -ExecutionPolicy Bypass -File scripts/make-screenshots.ps1
#      powershell -ExecutionPolicy Bypass -File scripts/make-screenshots.ps1 -Only 01,02
# =============================================================================

param(
    [string]$OutputDir = "",
    [string[]]$Only = @()
)

$ErrorActionPreference = "Stop"

# 用 -File 调用脚本时参数会先被 PowerShell 当表达式解析：
# `-Only 01,06` 被当成整数数组，前导零被吃掉，实际传进来的是 "1,6"。
# 所以这里既拆逗号、又补足两位，让 -Only 01,06 / -Only 1,6 / -Only 6 都能用。
if ($Only.Count -eq 1 -and $Only[0] -like "*,*") {
    $Only = $Only[0] -split ','
}
$Only = @($Only | Where-Object { $_ } | ForEach-Object { $_.Trim().PadLeft(2, '0') })

$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root "build/VideoForge.exe"
if (-not (Test-Path $exe)) {
    throw "找不到 $exe，请先运行 scripts/build.ps1"
}

$media = Join-Path $root "testdata"
$demoVideo = Join-Path $media "演示视频.mp4"
$demoSubs = Join-Path $media "演示视频-带字幕.mkv"
$demoAudio = Join-Path $media "音乐-正弦波.mp3"
foreach ($f in @($demoVideo, $demoSubs, $demoAudio)) {
    if (-not (Test-Path -LiteralPath $f)) {
        throw "缺少演示素材：$f`n（真实演示视频请放到 testdata/，见 README 的说明）"
    }
}

if (-not $OutputDir) { $OutputDir = Join-Path $root "docs/images" }
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

$scratch = Join-Path $env:TEMP "vf-screenshots"

# -----------------------------------------------------------------------------
#  跑一次 GUI 自测，把它产出的多张截图分别取回来
#    -Outputs：@( @{ Pick = "01-主界面.png"; SaveAs = "01-主界面-深色.png" }, ... )
# -----------------------------------------------------------------------------
function Invoke-Shot {
    param(
        [Parameter(Mandatory)][string]$Tag,
        [Parameter(Mandatory)][string[]]$Arguments,
        [Parameter(Mandatory)][hashtable[]]$Outputs
    )

    $wanted = @()
    foreach ($o in $Outputs) {
        $index = ($o.SaveAs -split '-')[0]
        if ($Only.Count -eq 0 -or $Only -contains $index) { $wanted += $o }
    }
    if ($wanted.Count -eq 0) {
        Write-Host "  跳过 [$Tag]（这一轮没有 -Only 需要的产物）" -ForegroundColor DarkGray
        return
    }

    $dir = Join-Path $scratch $Tag
    Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $dir | Out-Null

    # 每次都从"全新安装"的状态开始，保证截图可复现
    Remove-Item "HKCU:\Software\VideoForge" -Recurse -Force -ErrorAction SilentlyContinue
    $env:VF_RESET_LAYOUT = "1"
    Get-Process VideoForge -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 600

    # 上一轮的输出文件可能还被句柄占着一瞬间，删不掉不该让整个脚本失败。
    # 但若确实删不掉（例如被音乐播放器/同步盘占用），程序会给重名文件加 (2) 后缀，
    # 截图里就会出现"演示视频_转换后(2).mp4"这种看着像故障的名字 —— 所以要明确告警。
    $leftover = @()
    foreach ($f in (Get-ChildItem $media -Filter "*转换后*" -ErrorAction SilentlyContinue)) {
        try { Remove-Item -LiteralPath $f.FullName -Force -ErrorAction Stop }
        catch { $leftover += $f.Name }
    }
    if ($leftover.Count -gt 0) {
        Write-Host "      警告：有 $($leftover.Count) 个旧输出删不掉，截图里可能出现 (2) 后缀：" -ForegroundColor Yellow
        $leftover | ForEach-Object { Write-Host "        $_" -ForegroundColor Yellow }
    }

    $argList = @("--screenshot", $dir) + $Arguments
    Write-Host "  [$Tag] 启动 1 次，取 $($wanted.Count) 张：$(($wanted | ForEach-Object { $_.SaveAs }) -join '、')" -ForegroundColor Cyan
    $proc = Start-Process -FilePath $exe -ArgumentList $argList -WorkingDirectory $root -PassThru -Wait
    if ($proc.ExitCode -ne 0) {
        throw "截图任务 $Tag 退出码 $($proc.ExitCode)（非 0）"
    }

    foreach ($o in $wanted) {
        $source = Join-Path $dir $o.Pick
        if (-not (Test-Path -LiteralPath $source)) {
            throw "截图任务 $Tag 没有产出 $($o.Pick)，实际产出：$((Get-ChildItem $dir -Filter *.png).Name -join ', ')"
        }
        $target = Join-Path $OutputDir $o.SaveAs
        Copy-Item -LiteralPath $source -Destination $target -Force
        $kb = [math]::Round((Get-Item -LiteralPath $target).Length / 1KB, 1)
        Write-Host "      -> $($o.SaveAs) ($kb KB)" -ForegroundColor Green
    }
}

Write-Host "==> 目标目录：$OutputDir" -ForegroundColor Cyan

# A 深色 + 批量转换（这一轮的 01 就是空闲态主界面，不必再单独跑一次）
Invoke-Shot -Tag "dark-batch" -Arguments @(
    "--theme", "dark", "--autoconvert", "--preset", "mp4-h264",
    $demoVideo, $demoSubs, $demoAudio
) -Outputs @(
    @{ Pick = "01-主界面.png";   SaveAs = "01-主界面-深色.png" },
    @{ Pick = "03-转换完成.png"; SaveAs = "02-批量转换完成.png" }
)

# B 浅色 + 转换（同样一次拿到空闲态与转换中两帧）
# 输入与深色那轮保持一致，媒体库条目数与信息表内容才能和深色版对上
Invoke-Shot -Tag "light-batch" -Arguments @(
    "--theme", "light", "--autoconvert", "--preset", "mp4-h264",
    $demoVideo, $demoSubs, $demoAudio
) -Outputs @(
    @{ Pick = "01-主界面.png"; SaveAs = "04-浅色主题.png" },
    @{ Pick = "02-转换中.png"; SaveAs = "03-转换中.png" }
)

# C 音频提取
# 只用一个输入：testdata 里若残留上次的同名输出（可能被音乐播放器占着删不掉），
# 程序会给重名文件加 (2) 后缀，演示图里看着像出了故障
Invoke-Shot -Tag "audio" -Arguments @(
    "--theme", "dark", "--autoconvert", "--preset", "mp3", $demoVideo
) -Outputs @(
    @{ Pick = "03-转换完成.png"; SaveAs = "05-音频提取.png" }
)

# D 小屏适配（1366x740，真实笔记本可视高度）
Invoke-Shot -Tag "small" -Arguments @(
    "--theme", "dark", "--window-size", "1366x740", $demoVideo, $demoSubs, $demoAudio
) -Outputs @(
    @{ Pick = "01-主界面.png"; SaveAs = "06-小屏适配.png" }
)

Get-Process VideoForge -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
Get-ChildItem $media -Filter "*转换后*" -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
Remove-Item $scratch -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "`n==> 完成，共 $((Get-ChildItem $OutputDir -Filter *.png).Count) 张截图在 $OutputDir" -ForegroundColor Green
