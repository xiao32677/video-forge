# =============================================================================
#  下载 ffmpeg（构建/打包/测试都要用）
#
#  用法： powershell -ExecutionPolicy Bypass -File scripts/get-ffmpeg.ps1
#  产出： third_party/ffmpeg/bin/{ffmpeg.exe, ffprobe.exe}
#
#  为什么需要它：ffmpeg 二进制体积 >100MB，不适合放进 git 仓库，
#  所以仓库里只保留这个下载脚本，保证任何人都能一条命令拿到依赖。
# =============================================================================

[CmdletBinding()]
param(
    [string]$Destination = "third_party/ffmpeg",
    [string]$Url = "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip"
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$binDir = Join-Path $Destination "bin"
if (Test-Path (Join-Path $binDir "ffmpeg.exe")) {
    Write-Host "==> 已存在：$binDir（如需重新下载请先删除该目录）" -ForegroundColor Yellow
    & (Join-Path $binDir "ffmpeg.exe") -version 2>&1 | Select-Object -First 1
    exit 0
}

$tempDir = Join-Path $env:TEMP ("videoforge-ffmpeg-" + [guid]::NewGuid().ToString("N").Substring(0, 8))
New-Item -ItemType Directory -Force -Path $tempDir | Out-Null
$zip = Join-Path $tempDir "ffmpeg.zip"

Write-Host "==> 下载 ffmpeg（约 115 MB，来自 gyan.dev）" -ForegroundColor Cyan
Write-Host "    $Url"
$ProgressPreference = "SilentlyContinue"
Invoke-WebRequest -Uri $Url -OutFile $zip -UseBasicParsing

Write-Host "==> 解压" -ForegroundColor Cyan
Expand-Archive -Path $zip -DestinationPath $tempDir -Force

$found = Get-ChildItem -Path $tempDir -Recurse -Filter "ffmpeg.exe" | Select-Object -First 1
if (-not $found) { throw "压缩包里没有找到 ffmpeg.exe" }
$sourceBin = $found.Directory.FullName

New-Item -ItemType Directory -Force -Path $binDir | Out-Null
Copy-Item (Join-Path $sourceBin "ffmpeg.exe") $binDir -Force
Copy-Item (Join-Path $sourceBin "ffprobe.exe") $binDir -Force
Copy-Item (Join-Path $sourceBin "LICENSE") $binDir -Force -ErrorAction SilentlyContinue

Remove-Item -Recurse -Force $tempDir

Write-Host "==> 完成：$binDir" -ForegroundColor Green
& (Join-Path $binDir "ffmpeg.exe") -version 2>&1 | Select-Object -First 1
Write-Host ""
Write-Host "提示：ffmpeg 为 GPL 许可（gyan.dev essentials 构建）。" -ForegroundColor DarkGray
Write-Host "      如需 LGPL 版本，请自行下载并替换 bin 目录内容，程序会自动使用。" -ForegroundColor DarkGray
