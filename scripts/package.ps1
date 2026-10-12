# =============================================================================
#  VideoForge 绿色便携版打包脚本
#
#  产出：dist/VideoForge-<版本>-win64-portable.zip
#        解压后双击 VideoForge.exe 即可运行，无需安装 Qt、无需安装 ffmpeg。
#
#  用法：
#     powershell -ExecutionPolicy Bypass -File scripts/package.ps1
#     powershell -ExecutionPolicy Bypass -File scripts/package.ps1 -SkipFfmpeg   # 只打 Qt 依赖
#
#  说明：
#     * 用 windeployqt 收集 Qt 运行库
#     * 把 ffmpeg/ffprobe 放到包内 ffmpeg/bin（程序启动会自动找到）
#     * ffmpeg 二进制的来源与许可见 README
# =============================================================================

[CmdletBinding()]
param(
    [string]$QtDir,
    [string]$BuildDir = "build",
    [string]$DistDir = "dist",
    [string]$FfmpegBin,
    [switch]$SkipFfmpeg
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Find-FirstExisting([string[]]$candidates, [string]$label) {
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) { return (Resolve-Path $candidate).Path }
    }
    throw "找不到 $label"
}

if (-not $QtDir) {
    $QtDir = Find-FirstExisting @("C:/Qt/6.9.3/mingw_64", "D:/Qt/6.9.3/mingw_64",
                                  "C:/Qt/6.9.0/mingw_64", "D:/Qt/6.9.0/mingw_64") "Qt (mingw_64)"
}

$exe = Join-Path $BuildDir "VideoForge.exe"
if (-not (Test-Path $exe)) { throw "未找到 $exe，请先运行 scripts/build.ps1" }

$windeployqt = Join-Path $QtDir "bin/windeployqt.exe"
if (-not (Test-Path $windeployqt)) { throw "未找到 windeployqt：$windeployqt" }

# ---- 版本号：从 CMakeLists 里读 ----
$version = "1.0.0"
$cmakeText = Get-Content (Join-Path $root "CMakeLists.txt") -Raw
if ($cmakeText -match 'project\(VideoForge\s+VERSION\s+([0-9.]+)') { $version = $Matches[1] }

$packageName = "VideoForge-$version-win64-portable"
$packageDir = Join-Path $DistDir $packageName
$zipPath = Join-Path $DistDir "$packageName.zip"

Write-Host "==> 打包 $packageName" -ForegroundColor Cyan
if (Test-Path $packageDir) { Remove-Item -Recurse -Force $packageDir }
New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

Copy-Item $exe $packageDir -Force
Copy-Item (Join-Path $root "README.md") $packageDir -Force
if (Test-Path (Join-Path $root "LICENSE")) { Copy-Item (Join-Path $root "LICENSE") $packageDir -Force }
New-Item -ItemType Directory -Force -Path (Join-Path $packageDir "docs") | Out-Null
Get-ChildItem (Join-Path $root "docs") -Filter *.md -ErrorAction SilentlyContinue |
    Copy-Item -Destination (Join-Path $packageDir "docs") -Force

Write-Host "==> windeployqt 收集 Qt 运行库" -ForegroundColor Cyan
$env:Path = "$QtDir\bin;" + $env:Path
# 注意：不要加 --no-compiler-runtime。
# Qt 预编译的 qwindows.dll / ffmpegmediaplugin.dll 依赖 MinGW 运行库
# （libstdc++-6 / libgcc_s_seh-1 / libwinpthread-1），漏掉这些 DLL 会让
# 解压后的程序直接报 0xC0000135（找不到 DLL）而起不来。
& $windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw `
    (Join-Path $packageDir "VideoForge.exe")
if ($LASTEXITCODE -ne 0) { Write-Host "    windeployqt 返回 $LASTEXITCODE（继续）" -ForegroundColor Yellow }

# 兜底：万一 windeployqt 没带上 MinGW 运行时，从工具链目录补齐
foreach ($runtime in @("libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll")) {
    if (Test-Path (Join-Path $packageDir $runtime)) { continue }
    foreach ($bin in @("C:/Qt/Tools/mingw1310_64/bin", "D:/Qt/Tools/mingw1310_64/bin")) {
        $candidate = Join-Path $bin $runtime
        if (Test-Path $candidate) {
            Copy-Item $candidate $packageDir -Force
            Write-Host "    补齐运行库：$runtime" -ForegroundColor Yellow
            break
        }
    }
}
if ($LASTEXITCODE -ne 0) { Write-Host "    windeployqt 返回 $LASTEXITCODE（继续）" -ForegroundColor Yellow }

# ---- ffmpeg ----
if (-not $SkipFfmpeg) {
    if (-not $FfmpegBin) {
        $FfmpegBin = $null
        foreach ($candidate in @(
            (Join-Path $root "third_party/ffmpeg/bin"),
            (Join-Path $root "$BuildDir/ffmpeg/bin"),
            "C:/ffmpeg/bin",
            "D:/ffmpeg/bin"
        )) {
            if ($candidate -and (Test-Path (Join-Path $candidate "ffmpeg.exe"))) { $FfmpegBin = $candidate; break }
        }
    }
    if ($FfmpegBin -and (Test-Path (Join-Path $FfmpegBin "ffmpeg.exe"))) {
        Write-Host "==> 内置 ffmpeg：$FfmpegBin" -ForegroundColor Cyan
        $target = Join-Path $packageDir "ffmpeg/bin"
        New-Item -ItemType Directory -Force -Path $target | Out-Null
        Copy-Item (Join-Path $FfmpegBin "ffmpeg.exe") $target -Force
        Copy-Item (Join-Path $FfmpegBin "ffprobe.exe") $target -Force
        Copy-Item (Join-Path $FfmpegBin "LICENSE") $target -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host "==> 未找到 ffmpeg，跳出内置步骤（运行 scripts/get-ffmpeg.ps1 可自动下载）" -ForegroundColor Yellow
    }
}

# ---- 额外说明文件 ----
@"
VideoForge $version · 绿色便携版
=====================================

双击 VideoForge.exe 即可运行，无需安装。

包内目录说明：
  VideoForge.exe      主程序
  ffmpeg/bin/         内置的 ffmpeg 与 ffprobe（程序启动时自动使用）
  docs/               架构说明、测试报告、会话总结
  README.md           使用说明

如果杀毒软件误报：本项目使用 MinGW 静态链接自编译，未做数字签名，
个别引擎可能对未签名的新程序给出启发式告警，添加信任即可。

ffmpeg 许可：见 ffmpeg/bin/LICENSE（本包内置的是 gyan.dev 的 essentials 构建，
GPL 许可）。如你的使用场景不允许 GPL，请删除 ffmpeg/bin 并自行放置 LGPL 构建，
程序会自动使用你放置的版本。
"@ | Set-Content -Path (Join-Path $packageDir "使用说明.txt") -Encoding UTF8

Write-Host "==> 压缩" -ForegroundColor Cyan
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path (Join-Path $packageDir "*") -DestinationPath $zipPath -CompressionLevel Optimal

$size = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)
Write-Host "==> 完成：$zipPath ($size MB)" -ForegroundColor Green
