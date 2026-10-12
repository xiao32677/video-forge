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
        # ffmpeg 是 GPL 许可：分发二进制必须带上许可证文本
        foreach ($doc in @("LICENSE", "README.txt")) {
            if (Test-Path (Join-Path $FfmpegBin $doc)) {
                Copy-Item (Join-Path $FfmpegBin $doc) $target -Force
            } elseif (Test-Path (Join-Path (Split-Path $FfmpegBin -Parent) $doc)) {
                Copy-Item (Join-Path (Split-Path $FfmpegBin -Parent) $doc) $target -Force
            }
        }
        if (-not (Test-Path (Join-Path $target "LICENSE"))) {
            Write-Host "    注意：没有找到 ffmpeg 的 LICENSE 文件，将写入一份说明代替" -ForegroundColor Yellow
        }
    } else {
        Write-Host "==> 未找到 ffmpeg，跳出内置步骤（运行 scripts/get-ffmpeg.ps1 可自动下载）" -ForegroundColor Yellow
    }
}

# ---- 第三方许可：分发 Qt DLL 与 ffmpeg 二进制必须一并给出许可与来源说明 ----
$licenseDir = Join-Path $packageDir "LICENSES"
New-Item -ItemType Directory -Force -Path $licenseDir | Out-Null

if (Test-Path (Join-Path $root "LICENSE")) {
    Copy-Item (Join-Path $root "LICENSE") (Join-Path $licenseDir "VideoForge-MIT.txt") -Force
}
$ffmpegLicense = Join-Path $packageDir "ffmpeg/bin/LICENSE"
if (Test-Path $ffmpegLicense) {
    Copy-Item $ffmpegLicense (Join-Path $licenseDir "ffmpeg-GPL-3.0.txt") -Force
}

@"
Qt 6.9.3 —— 许可与合规说明
========================================

本程序使用 Qt 6.9.3（The Qt Company，https://www.qt.io）构建，
并以动态链接方式使用了以下 Qt 模块的官方预编译库：

    Qt6Core / Qt6Gui / Qt6Widgets / Qt6Concurrent / Qt6Network / Qt6Svg
    Qt6Multimedia / Qt6MultimediaWidgets
    以及 platforms、imageformats、styles、tls、multimedia 等插件

Qt 在开源使用场景下采用 GNU Lesser General Public License v3 (LGPL-3.0)。
LGPL-3.0 完整文本见 https://www.gnu.org/licenses/lgpl-3.0.html

为满足 LGPL-3.0 的要求，特别说明：
  1. 本程序**未修改** Qt 的任何源代码或二进制文件；
  2. Qt 以**动态链接库（DLL）**形式使用，因此你可以用自己编译的、
     接口兼容的 Qt 库替换本目录下的同名 DLL 来重新组合本程序；
  3. 本程序自身源码以 MIT 许可发布（见 VideoForge-MIT.txt），
     可自由获取与修改，从而可以重新链接 Qt；
  4. 如果你需要 Qt 的完整源码，请从 https://download.qt.io/ 获取。

如果你的使用场景不接受 LGPL-3.0，可以向 The Qt Company 购买商业许可，
或改用其它 GUI 框架重新实现界面层。
"@ | Set-Content -Path (Join-Path $licenseDir "Qt-LGPL-3.0-说明.txt") -Encoding UTF8

@"
第三方组件清单
========================================

组件        版本                              许可            用途
------------------------------------------------------------------------------------
Qt          6.9.3 (mingw_64)                  LGPL-3.0        界面框架、播放器、网络
ffmpeg      9.0.2-essentials (gyan.dev)       GPL-3.0         音视频转换与信息读取（子进程调用）
MinGW-w64   GCC 13.1.0                        GPL/LGPL 例外   编译器与运行库
VideoForge  1.0.0                             MIT             本项目自身代码

ffmpeg 的使用方式说明：
    本程序**不修改也不链接** ffmpeg 代码，而是以**独立子进程**方式调用
    ffmpeg.exe / ffprobe.exe 的命令行接口。因此本程序自身的 MIT 许可
    不受 ffmpeg 的 GPL 许可影响。

    但请注意：随包分发的 ffmpeg 二进制本身是 GPL-3.0 许可的，
    其许可证全文见 LICENSES/ffmpeg-GPL-3.0.txt。
    如需替换为 LGPL 构建：删除 ffmpeg/bin 目录，把你自己的 ffmpeg.exe 与
    ffprobe.exe 放进同一位置即可，程序会自动使用你提供的版本。
"@ | Set-Content -Path (Join-Path $licenseDir "第三方组件清单.txt") -Encoding UTF8

# ---- 额外说明文件 ----
@"
VideoForge $version · 绿色便携版
=====================================

双击 VideoForge.exe 即可运行，无需安装。

包内目录说明：
  VideoForge.exe      主程序
  ffmpeg/bin/         内置的 ffmpeg 与 ffprobe（程序启动时自动使用）
  LICENSES/           第三方许可与合规说明（Qt LGPL、ffmpeg GPL、组件清单）
  docs/               架构说明、测试报告、会话总结
  README.md           使用说明

关于许可（重要）：
  本项目自身代码以 MIT 许可发布，见 LICENSES/VideoForge-MIT.txt。
  程序以**子进程**方式调用 ffmpeg，不修改也不链接 ffmpeg 代码；
  但随包分发的 ffmpeg 二进制本身是 GPL-3.0，许可证全文见
  LICENSES/ffmpeg-GPL-3.0.txt。若你的场景不允许 GPL，删除 ffmpeg/bin
  并放入自己的 LGPL 构建即可，程序会自动使用你提供的版本。
  界面框架 Qt 6.9.3 以 LGPL-3.0 动态链接方式使用，
  说明见 LICENSES/Qt-LGPL-3.0-说明.txt。

如果杀毒软件误报：本项目未做数字签名，个别引擎可能对未签名的新程序
给出启发式告警，添加信任即可。
"@ | Set-Content -Path (Join-Path $packageDir "使用说明.txt") -Encoding UTF8

Write-Host "==> 压缩" -ForegroundColor Cyan
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path (Join-Path $packageDir "*") -DestinationPath $zipPath -CompressionLevel Optimal

$size = [math]::Round((Get-Item $zipPath).Length / 1MB, 1)
Write-Host "==> 完成：$zipPath ($size MB)" -ForegroundColor Green
