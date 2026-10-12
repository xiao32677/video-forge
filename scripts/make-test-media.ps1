# =============================================================================
#  生成测试用媒体文件（合成视频，不涉及任何版权素材）
#
#  用法： powershell -ExecutionPolicy Bypass -File scripts/make-test-media.ps1
#  需要： ffmpeg.exe / ffprobe.exe（可用 scripts/get-ffmpeg.ps1 下载）
#
#  产出（testdata/）：
#     演示视频-720p.mp4      1280x720 30fps H.264 + AAC 12s   —— 主测试素材
#     演示视频2-540p.avi      960x540 25fps MPEG-4 + MP3 8s    —— 老格式输入
#     演示视频3-HEVC.mkv      640x360 24fps H.265 + AC3 6s     —— 新编码/多容器
#     音乐-正弦波.mp3         44.1kHz 立体声 MP3 10s           —— 纯音频输入
# =============================================================================

[CmdletBinding()]
param(
    [string]$FfmpegBin,
    [string]$OutputDir = "testdata"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if (-not $FfmpegBin) {
    foreach ($candidate in @(
        (Join-Path $root "third_party/ffmpeg/bin"),
        (Join-Path $root "build/ffmpeg/bin"),
        "C:/ffmpeg/bin",
        "D:/ffmpeg/bin"
    )) {
        if (Test-Path (Join-Path $candidate "ffmpeg.exe")) { $FfmpegBin = $candidate; break }
    }
}
if (-not $FfmpegBin) {
    throw "找不到 ffmpeg.exe。请先运行 scripts/get-ffmpeg.ps1，或用 -FfmpegBin 指定 bin 目录。"
}

$ffmpeg = Join-Path $FfmpegBin "ffmpeg.exe"
$ffprobe = Join-Path $FfmpegBin "ffprobe.exe"
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

function New-TestVideo {
    param([string[]]$Arguments, [string]$Name, [string]$Description)
    Write-Host "==> $Description" -ForegroundColor Cyan
    & $ffmpeg -y -hide_banner -loglevel error @Arguments
    if ($LASTEXITCODE -ne 0) { throw "生成 $Name 失败" }
}

New-TestVideo -Name "演示视频-720p.mp4" -Description "720p H.264 + AAC" -Arguments @(
    "-f", "lavfi", "-i", "testsrc2=size=1280x720:rate=30",
    "-f", "lavfi", "-i", "sine=frequency=440:sample_rate=48000",
    "-t", "12", "-c:v", "libx264", "-preset", "veryfast", "-crf", "20",
    "-pix_fmt", "yuv420p", "-c:a", "aac", "-b:a", "128k", "-shortest",
    (Join-Path $OutputDir "演示视频-720p.mp4")
)

New-TestVideo -Name "演示视频2-540p.avi" -Description "540p MPEG-4 + MP3 (AVI)" -Arguments @(
    "-f", "lavfi", "-i", "testsrc=size=960x540:rate=25",
    "-f", "lavfi", "-i", "sine=frequency=880:sample_rate=44100",
    "-t", "8", "-c:v", "mpeg4", "-q:v", "5", "-c:a", "libmp3lame", "-b:a", "128k", "-shortest",
    (Join-Path $OutputDir "演示视频2-540p.avi")
)

New-TestVideo -Name "演示视频3-HEVC.mkv" -Description "360p H.265 + AC3 (MKV)" -Arguments @(
    "-f", "lavfi", "-i", "smptebars=size=640x360:rate=24",
    "-f", "lavfi", "-i", "sine=frequency=220:sample_rate=48000",
    "-t", "6", "-c:v", "libx265", "-preset", "ultrafast", "-crf", "30",
    "-pix_fmt", "yuv420p", "-c:a", "ac3", "-b:a", "192k", "-shortest",
    (Join-Path $OutputDir "演示视频3-HEVC.mkv")
)

New-TestVideo -Name "音乐-正弦波.mp3" -Description "纯音频 MP3" -Arguments @(
    "-f", "lavfi", "-i", "sine=frequency=523:sample_rate=44100",
    "-t", "10", "-c:a", "libmp3lame", "-b:a", "192k",
    (Join-Path $OutputDir "音乐-正弦波.mp3")
)

Write-Host ""
Write-Host "==> 生成完毕，校验结果：" -ForegroundColor Green
Get-ChildItem $OutputDir -File | ForEach-Object {
    $info = & $ffprobe -v error -show_entries format=format_name,duration -of csv=p=0 $_.FullName
    Write-Host ("    {0,-24} {1,8:N0} KB   {2}" -f $_.Name, ($_.Length / 1KB), $info)
}
