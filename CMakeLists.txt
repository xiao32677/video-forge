cmake_minimum_required(VERSION 3.21)

project(VideoForge
    VERSION 1.0.0
    DESCRIPTION "VideoForge - 视频格式转换器桌面客户端 (Qt6 + C++17)"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(VF_ENABLE_PLAYER "Enable embedded Qt Multimedia player preview" ON)
option(VF_BUILD_TESTS  "Build unit tests" ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets Concurrent Svg)
if(VF_ENABLE_PLAYER)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Multimedia MultimediaWidgets)
endif()

qt_standard_project_setup()

# ---------------------------------------------------------------- 核心静态库
# core/ 与 job/ 不依赖任何界面代码，抽成静态库后：
#   * 主程序链接它
#   * 单元测试也链接它（不需要起 GUI）
qt_add_library(videoforge_core STATIC
    src/core/MediaInfo.cpp
    src/core/FfmpegLocator.cpp
    src/core/MediaProbe.cpp
    src/core/ThumbnailService.cpp
    src/core/ConvertPreset.cpp
    src/core/CommandBuilder.cpp
    src/core/ProgressParser.cpp
    src/core/HwAccelDetector.cpp
    src/core/SizeEstimator.cpp
    src/core/Format.cpp
    src/job/ConvertJob.cpp
    src/job/JobQueue.cpp
)

target_include_directories(videoforge_core PUBLIC src)
target_link_libraries(videoforge_core PUBLIC Qt6::Core Qt6::Gui Qt6::Concurrent)
if(WIN32)
    target_compile_definitions(videoforge_core PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

set(VF_UI_SOURCES
    src/ui/Theme.cpp
    src/ui/MainWindow.cpp
    src/ui/MediaLibraryPanel.cpp
    src/ui/PreviewPanel.cpp
    src/ui/FilmstripWidget.cpp
    src/ui/ConvertSettingsPanel.cpp
    src/ui/JobQueuePanel.cpp
    src/ui/LogPanel.cpp
    src/ui/AppIcon.cpp
)

qt_add_executable(VideoForge WIN32
    src/main.cpp
    ${VF_UI_SOURCES}
    resources.qrc
    assets/videoforge.rc
)

target_link_libraries(VideoForge PRIVATE
    videoforge_core
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Concurrent Qt6::Svg
)

if(VF_ENABLE_PLAYER)
    target_link_libraries(VideoForge PRIVATE Qt6::Multimedia Qt6::MultimediaWidgets)
    target_compile_definitions(VideoForge PRIVATE VF_ENABLE_PLAYER=1)
else()
    target_compile_definitions(VideoForge PRIVATE VF_ENABLE_PLAYER=0)
endif()

if(WIN32)
    target_compile_definitions(VideoForge PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(VideoForge PRIVATE shell32 ole32 psapi)
    # 注意：这里刻意 *不* 使用 -static-libstdc++ / -static。
    # Qt 预编译的插件 DLL（qwindows.dll、ffmpegmediaplugin.dll）依赖共享的
    # libstdc++-6 / libgcc_s_seh-1 / libwinpthread-1，如果主程序静态链接而插件动态链接，
    # 就会出现"两套 C++ 运行时跨模块传递异常/RTTI"的隐患。
    # 统一走共享运行时，并由 scripts/package.ps1 里的 windeployqt 一并收集这些 DLL。
endif()

# ---------------------------------------------------------------- 单元测试
if(VF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------------------------------------- 安装/部署
install(TARGETS VideoForge RUNTIME DESTINATION .)
