# =============================================================================
#  VideoForge - 生成文档用的界面截图
#
#  背景：README 与 Release 里的 6 张截图以前是手工敲命令生成的，
#        既不可复现，也容易漏掉某个主题/尺寸。这里固化成脚本。
#
#  演示素材来自 testdata/（真实视频），不是合成的彩条测试图 —— 截图是给评审看的，
#  用真实片子才能看出界面到底好不好用。
#
#  用法：
#      powershell -ExecutionPolicy Bypass -File scripts/make-screenshots.ps1
#      powershell -ExecutionPolicy Bypass -File scripts/make-screenshots.ps1 -Only 01,06
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
#  跑一次 GUI 自测并取回指定截图
# -----------------------------------------------------------------------------
function Invoke-Shot {
    param(
        [Parameter(Mandatory)][string]$Tag,
        [Parameter(Mandatory)][string[]]$Arguments,
        [Parameter(Mandatory)][string]$Pick,
        [Parameter(Mandatory)][string]$SaveAs
    )

    $target = Join-Path $OutputDir $SaveAs
    if ($Only.Count -gt 0 -and -not ($Only -contains ($SaveAs -split '-')[0])) {
        Write-Host "  跳过 $SaveAs（-Only 已限定）" -ForegroundColor DarkGray
        return
    }

    $dir = Join-Path $scratch $Tag
    Remove-Item $dir -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force -Path $dir | Out-Null

    # 每次都从"全新安装"的状态开始，保证截图可复现
    Remove-Item "HKCU:\Software\VideoForge" -Recurse -Force -ErrorAction SilentlyContinue
    $env:VF_RESET_LAYOUT = "1"
    # 上一轮的输出文件可能还被句柄占着一瞬间，删不掉不该让整个脚本失败。
    # 但如果确实删不掉（例如被音乐播放器/同步盘占用），程序会给重名文件加 (2) 后缀，
    # 截图里就会看到"演示视频_转换后(2).mp4"这种像故障的名字 —— 所以这里要明确告警。
    Get-Process VideoForge -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 600
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
    Write-Host "  [$Tag] $($Arguments -join ' ')" -ForegroundColor Cyan
    $proc = Start-Process -FilePath $exe -ArgumentList $argList -WorkingDirectory $root -PassThru -Wait
    if ($proc.ExitCode -ne 0) {
        throw "截图任务 $Tag 退出码 $($proc.ExitCode)（非 0）"
    }

    $source = Join-Path $dir $Pick
    if (-not (Test-Path -LiteralPath $source)) {
        throw "截图任务 $Tag 没有产出 $Pick，实际产出：$((Get-ChildItem $dir -Filter *.png).Name -join ', ')"
    }
    Copy-Item -LiteralPath $source -Destination $target -Force
    $kb = [math]::Round((Get-Item -LiteralPath $target).Length / 1KB, 1)
    Write-Host "      -> $SaveAs ($kb KB)" -ForegroundColor Green
}

Write-Host "==> 目标目录：$OutputDir" -ForegroundColor Cyan

# 01 深色主界面：载入三个真实文件，看信息面板 + 胶片条 + 媒体库
Invoke-Shot -Tag "dark-idle" -Pick "01-主界面.png" -SaveAs "01-主界面-深色.png" -Arguments @(
    "--theme", "dark", $demoVideo, $demoSubs, $demoAudio
)

# 02 批量转换完成（深色）
Invoke-Shot -Tag "dark-batch" -Pick "03-转换完成.png" -SaveAs "02-批量转换完成.png" -Arguments @(
    "--theme", "dark", "--autoconvert", "--preset", "mp4-h264",
    $demoVideo, $demoSubs, $demoAudio
)

# 03 转换中（浅色）：看进度条 / 速度 / 剩余时间
Invoke-Shot -Tag "light-running" -Pick "02-转换中.png" -SaveAs "03-转换中.png" -Arguments @(
    "--theme", "light", "--autoconvert", "--preset", "mp4-h264", $demoVideo
)

# 04 浅色主题静态界面
Invoke-Shot -Tag "light-idle" -Pick "01-主界面.png" -SaveAs "04-浅色主题.png" -Arguments @(
    "--theme", "light", $demoVideo, $demoSubs, $demoAudio
)

# 05 音频提取
# 只用一个输入：testdata 里若残留上次的同名输出（可能被音乐播放器占着删不掉），
# 程序会给重名文件加 (2) 后缀，演示图里看着像出了故障
Invoke-Shot -Tag "audio" -Pick "03-转换完成.png" -SaveAs "05-音频提取.png" -Arguments @(
    "--theme", "dark", "--autoconvert", "--preset", "mp3", $demoVideo
)

# 06 小屏适配（1366x740，真实笔记本可视高度）
Invoke-Shot -Tag "small" -Pick "01-主界面.png" -SaveAs "06-小屏适配.png" -Arguments @(
    "--theme", "dark", "--window-size", "1366x740", $demoVideo, $demoSubs, $demoAudio
)

Get-Process VideoForge -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 800
Get-ChildItem $media -Filter "*转换后*" -ErrorAction SilentlyContinue |
    Remove-Item -Force -ErrorAction SilentlyContinue
Remove-Item $scratch -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "`n==> 完成，共 $((Get-ChildItem $OutputDir -Filter *.png).Count) 张截图在 $OutputDir" -ForegroundColor Green
