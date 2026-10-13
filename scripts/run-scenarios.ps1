# =============================================================================
#  VideoForge - 端到端场景回归
#
#  做两件事：
#    1. 在**独立的临时目录**里跑真实转换（不碰 testdata，多个场景也不会互相抢输出文件）；
#    2. 用 ffprobe 复核产物（编码、分辨率、时长、字幕轨），而不是只看退出码。
#
#  为什么需要它：TEST_REPORT 里那张"场景与结果"表以前是手工敲命令跑出来的，
#  既不可复现也容易过期。把它固化成脚本后，任何时候都能重跑一遍拿到同样的证据。
#
#  用法：
#      powershell -ExecutionPolicy Bypass -File scripts/run-scenarios.ps1
#      powershell -ExecutionPolicy Bypass -File scripts/run-scenarios.ps1 -IncludeSlow
# =============================================================================

param(
    [switch]$IncludeSlow,   # 加上 HEVC 场景（720p 用 x265 要跑好几分钟）
    [switch]$KeepWorkDir
)

$ErrorActionPreference = "Stop"

$root = Split-Path $PSScriptRoot -Parent
$exe = Join-Path $root "build/VideoForge.exe"
$ffprobe = Join-Path $root "build/ffmpeg/bin/ffprobe.exe"
foreach ($need in @($exe, $ffprobe)) {
    if (-not (Test-Path $need)) { throw "缺少 $need，请先运行 scripts/build.ps1（并用 get-ffmpeg.ps1 准备 ffmpeg）" }
}

$media = Join-Path $root "testdata"
$sources = @{
    Video = Join-Path $media "演示视频.mp4"
    Subs  = Join-Path $media "演示视频-带字幕.mkv"
    Audio = Join-Path $media "音乐-正弦波.mp3"
}
foreach ($name in $sources.Keys) {
    if (-not (Test-Path -LiteralPath $sources[$name])) {
        throw "缺少演示素材：$($sources[$name])"
    }
}

$work = Join-Path $env:TEMP "vf-scenarios"
Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $work | Out-Null
foreach ($name in $sources.Keys) {
    Copy-Item -LiteralPath $sources[$name] -Destination $work -Force
}
$video = Join-Path $work "演示视频.mp4"
$subs  = Join-Path $work "演示视频-带字幕.mkv"
$audio = Join-Path $work "音乐-正弦波.mp3"

$script:results = @()

function Clear-Outputs {
    Get-ChildItem $work -Filter "*转换后*" -ErrorAction SilentlyContinue |
        Remove-Item -Force -ErrorAction SilentlyContinue
    Remove-Item "$work\shots" -Recurse -Force -ErrorAction SilentlyContinue
}

# 跑一次 GUI；返回退出码
function Invoke-App {
    param([string[]]$Arguments)
    Remove-Item "HKCU:\Software\VideoForge" -Recurse -Force -ErrorAction SilentlyContinue
    $env:VF_RESET_LAYOUT = "1"
    Get-Process VideoForge -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 500
    $proc = Start-Process -FilePath $exe `
        -ArgumentList (@("--screenshot", "$work\shots") + $Arguments) `
        -WorkingDirectory $root -PassThru -Wait
    return $proc.ExitCode
}

# 用 ffprobe 读一个字段
function Probe {
    param([string]$File, [string]$Selector)
    $out = & $ffprobe -v error -select_streams v:0 -show_entries "stream=$Selector" `
                      -of default=noprint_wrappers=1:nokey=1 -- $File 2>$null
    return ($out | Select-Object -First 1)
}
function ProbeAny {
    param([string]$File, [string]$Args)
    return (& $ffprobe -v error $Args -of default=noprint_wrappers=1 -- $File 2>$null) -join ' '
}
function Duration {
    param([string]$File)
    $d = & $ffprobe -v error -show_entries format=duration -of default=noprint_wrappers=1:nokey=1 -- $File 2>$null
    return [double]($d | Select-Object -First 1)
}

function Add-Result {
    param([string]$Name, [bool]$Pass, [string]$Detail)
    $script:results += [pscustomobject]@{ 场景 = $Name; 结果 = $(if ($Pass) { "通过" } else { "失败" }); 说明 = $Detail }
    $color = if ($Pass) { "Green" } else { "Red" }
    Write-Host ("  [{0}] {1} —— {2}" -f $(if ($Pass) { "通过" } else { "失败" }), $Name, $Detail) -ForegroundColor $color
}

Write-Host "==> 工作目录：$work" -ForegroundColor Cyan

# ---------------------------------------------------------------- 1. 批量转换
Write-Host "`n[1/7] 批量转换 · MP4/H.264（3 个文件，其中 1 个是纯音频应被拦下）" -ForegroundColor Cyan
Clear-Outputs
$code = Invoke-App @("--theme", "dark", "--autoconvert", "--preset", "mp4-h264", $video, $subs, $audio)
$made = @(Get-ChildItem $work -Filter "*转换后*.mp4" -ErrorAction SilentlyContinue)
Add-Result "批量转换" ($code -eq 0 -and $made.Count -eq 2) "退出码=$code，产出 $($made.Count) 个 MP4（期望 2：视频 2 个；纯音频被拦下）"

# ---------------------------------------------------------------- 2. 产物校验
if ($made.Count -ge 1) {
    Write-Host "`n[2/7] 转换后产物复核（编码 / 分辨率 / 时长）" -ForegroundColor Cyan
    $vcodec = Probe $made[0].FullName "codec_name"
    $w = Probe $made[0].FullName "width"
    $h = Probe $made[0].FullName "height"
    $srcDur = Duration $video
    $outDur = Duration $made[0].FullName
    $durOk = [math]::Abs($srcDur - $outDur) -lt 1.0
    Add-Result "产物复核" ($vcodec -eq "h264" -and $w -eq "1280" -and $h -eq "720" -and $durOk) `
        "编码=$vcodec 分辨率=${w}x${h} 时长 源=$([math]::Round($srcDur,2))s 产物=$([math]::Round($outDur,2))s"
} else {
    Add-Result "产物复核" $false "上一步没有产物，跳过"
}

# ---------------------------------------------------------------- 3. 音频提取
Write-Host "`n[3/7] 音频提取 · MP3" -ForegroundColor Cyan
Clear-Outputs
$code = Invoke-App @("--autoconvert", "--preset", "mp3", $video)
$mp3 = Join-Path $work "演示视频_转换后.mp3"
$hasVideo = (& $ffprobe -v error -select_streams v -show_entries stream=codec_type -of csv=p=0 -- $mp3 2>$null)
$acodec = (& $ffprobe -v error -select_streams a:0 -show_entries stream=codec_name -of default=noprint_wrappers=1:nokey=1 -- $mp3 2>$null | Select-Object -First 1)
Add-Result "音频提取" ((Test-Path $mp3) -and -not $hasVideo -and $acodec -eq "mp3") `
    "退出码=$code，产物存在=$(Test-Path $mp3)，音频编码=$acodec，含视频流=$([bool]$hasVideo)"

# ---------------------------------------------------------------- 4. 只转选中项
Write-Host "`n[4/7] 只转选中项（3 个文件里只选 1 个）" -ForegroundColor Cyan
Clear-Outputs
$code = Invoke-App @("--autoconvert", "--preset", "mp4-h264", "--select", "带字幕", $video, $subs, $audio)
$made = @(Get-ChildItem $work -Filter "*转换后*" -ErrorAction SilentlyContinue)
Add-Result "只转选中项" ($code -eq 0 -and $made.Count -eq 1) "退出码=$code，产出 $($made.Count) 个（期望 1）"

# ---------------------------------------------------------------- 5. 字幕保留 → MP4
Write-Host "`n[5/7] 字幕保留 · MP4（应转成 mov_text 内嵌字幕）" -ForegroundColor Cyan
Clear-Outputs
$code = Invoke-App @("--autoconvert", "--preset", "mp4-h264", "--subtitles", "copy", $subs)
$out = Join-Path $work "演示视频-带字幕_转换后.mp4"
$subCodec = (& $ffprobe -v error -select_streams s -show_entries stream=codec_name -of default=noprint_wrappers=1:nokey=1 -- $out 2>$null | Select-Object -First 1)
Add-Result "字幕保留(MP4)" ((Test-Path $out) -and $subCodec -eq "mov_text") `
    "退出码=$code，字幕编码=$subCodec（期望 mov_text）"

# ---------------------------------------------------------------- 6. 字幕保留 → MKV 复制
Write-Host "`n[6/7] 字幕保留 · MKV 无损换壳（应原样复制 subrip）" -ForegroundColor Cyan
Clear-Outputs
$code = Invoke-App @("--autoconvert", "--preset", "mkv-remux", "--subtitles", "copy", $subs)
$out = Join-Path $work "演示视频-带字幕_转换后.mkv"
$subCodec = (& $ffprobe -v error -select_streams s -show_entries stream=codec_name -of default=noprint_wrappers=1:nokey=1 -- $out 2>$null | Select-Object -First 1)
$srcSize = (Get-Item -LiteralPath $subs).Length
$outSize = if (Test-Path $out) { (Get-Item -LiteralPath $out).Length } else { 0 }
$sameSize = ($srcSize -eq $outSize)
Add-Result "字幕保留(MKV)" ((Test-Path $out) -and $subCodec -eq "subrip") `
    "退出码=$code，字幕编码=$subCodec（期望 subrip）"
Add-Result "MKV 无损换壳字节一致" $sameSize `
    "源=$([math]::Round($srcSize/1MB,2)) MB，产物=$([math]::Round($outSize/1MB,2)) MB（同容器换壳应完全一致）"

# ---------------------------------------------------------------- 7. 小屏布局
Write-Host "`n[7/7] 小屏布局（1366x740）" -ForegroundColor Cyan
Clear-Outputs
$code = Invoke-App @("--window-size", "1366x740", $video, $subs, $audio)
$shot = Join-Path $work "shots\01-主界面.png"
$debugFile = Join-Path $work "shots\layout-debug.txt"
$debugOk = $false
if (Test-Path $debugFile) {
    $debugOk = -not ((Get-Content $debugFile -Raw) -match "重叠|overlap")
}
Add-Result "小屏布局" ($code -eq 0 -and (Test-Path $shot)) `
    "退出码=$code，截图=$([bool](Test-Path $shot))，布局诊断无重叠=$debugOk"

# ---------------------------------------------------------------- 可选：HEVC
if ($IncludeSlow) {
    Write-Host "`n[+] HEVC 场景（较慢，请耐心等待）" -ForegroundColor Cyan
    Clear-Outputs
    $code = Invoke-App @("--autoconvert", "--preset", "mp4-h265", $video)
    $out = Join-Path $work "演示视频_转换后.mp4"
    $vcodec = if (Test-Path $out) { Probe $out "codec_name" } else { "" }
    Add-Result "HEVC 编码" ((Test-Path $out) -and $vcodec -eq "hevc") `
        "退出码=$code，视频编码=$vcodec（期望 hevc，不能回退成 h264）"
}

# ---------------------------------------------------------------- 汇总
Write-Host "`n================ 汇总 ================" -ForegroundColor Cyan
$script:results | Format-Table -AutoSize
$failed = @($script:results | Where-Object { $_.结果 -eq "失败" })
Write-Host ("共 {0} 项，通过 {1} 项，失败 {2} 项" -f $script:results.Count, ($script:results.Count - $failed.Count), $failed.Count) `
    -ForegroundColor $(if ($failed.Count -eq 0) { "Green" } else { "Red" })

if (-not $KeepWorkDir) {
    Get-Process VideoForge -ErrorAction SilentlyContinue | Stop-Process -Force
    Start-Sleep -Milliseconds 600
    Remove-Item $work -Recurse -Force -ErrorAction SilentlyContinue
}

if ($failed.Count -gt 0) { exit 1 }
exit 0
