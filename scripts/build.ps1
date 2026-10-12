# =============================================================================
#  VideoForge 一键构建脚本
#
#  用法：
#     powershell -ExecutionPolicy Bypass -File scripts/build.ps1
#     powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -Clean
#     powershell -ExecutionPolicy Bypass -File scripts/build.ps1 -QtDir C:/Qt/6.9.3/mingw_64
#
#  说明：脚本会自动寻找 Qt / CMake / Ninja / MinGW，找不到才报错。
# =============================================================================

[CmdletBinding()]
param(
    [string]$QtDir,
    [string]$BuildDir = "build",
    [ValidateSet("Release", "Debug", "RelWithDebInfo")]
    [string]$Config = "Release",
    [switch]$Clean,
    [switch]$RunTests
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

function Find-FirstExisting([string[]]$candidates, [string]$label) {
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path $candidate)) { return (Resolve-Path $candidate).Path }
    }
    throw "找不到 $label。请安装后重试，或用参数显式指定。"
}

Write-Host "==> 定位工具链" -ForegroundColor Cyan

if (-not $QtDir) {
    $QtDir = Find-FirstExisting @(
        "C:/Qt/6.9.3/mingw_64",
        "C:/Qt/6.9.2/mingw_64",
        "C:/Qt/6.9.0/mingw_64",
        "D:/Qt/6.9.3/mingw_64",
        "D:/Qt/6.9.0/mingw_64"
    ) "Qt (mingw_64)"
}

$cmake = (Get-Command cmake -ErrorAction SilentlyContinue).Source
if (-not $cmake) { $cmake = Find-FirstExisting @("D:/CMake/bin/cmake.exe", "C:/Program Files/CMake/bin/cmake.exe") "CMake" }

$ninja = (Get-Command ninja -ErrorAction SilentlyContinue).Source
if (-not $ninja) { $ninja = Find-FirstExisting @("D:/Qt/Tools/Ninja/ninja.exe", "C:/Qt/Tools/Ninja/ninja.exe") "Ninja" }

$mingwBin = Find-FirstExisting @(
    "C:/Qt/Tools/mingw1310_64/bin",
    "D:/Qt/Tools/mingw1310_64/bin",
    "C:/Qt/Tools/mingw1120_64/bin"
) "MinGW 工具链"

Write-Host "    Qt      : $QtDir"
Write-Host "    CMake   : $cmake"
Write-Host "    Ninja   : $ninja"
Write-Host "    MinGW   : $mingwBin"

$env:Path = "$mingwBin;$QtDir\bin;" + $env:Path

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "==> 清理旧的构建目录" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

Write-Host "==> 配置 CMake ($Config)" -ForegroundColor Cyan
& $cmake -S . -B $BuildDir -G Ninja `
    "-DCMAKE_BUILD_TYPE=$Config" `
    "-DCMAKE_PREFIX_PATH=$QtDir" `
    "-DCMAKE_C_COMPILER=$mingwBin/gcc.exe" `
    "-DCMAKE_CXX_COMPILER=$mingwBin/g++.exe" `
    "-DCMAKE_MAKE_PROGRAM=$ninja"
if ($LASTEXITCODE -ne 0) { throw "CMake 配置失败" }

Write-Host "==> 编译" -ForegroundColor Cyan
& $cmake --build $BuildDir --parallel
if ($LASTEXITCODE -ne 0) { throw "编译失败" }

$exe = Join-Path $BuildDir "VideoForge.exe"
Write-Host "==> 构建完成：$exe" -ForegroundColor Green

if ($RunTests) {
    Write-Host "==> 运行单元测试" -ForegroundColor Cyan
    $tests = Join-Path $BuildDir "vf_tests.exe"
    if (Test-Path $tests) {
        & $tests
        if ($LASTEXITCODE -ne 0) { throw "单元测试未通过" }
    } else {
        Write-Host "    (未找到 vf_tests.exe，跳过)" -ForegroundColor Yellow
    }
}

Write-Host ""
Write-Host "下一步：打包绿色便携版 -> scripts/package.ps1" -ForegroundColor DarkGray
