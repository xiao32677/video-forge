#pragma once

// =============================================================================
//  VideoForge - QProcess 通用小工具
// =============================================================================

#include <QProcess>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace vf {

/// 让子进程不弹出黑色控制台窗口。
/// Qt 在 GUI 程序中默认可能给控制台程序分配窗口，ffmpeg 一秒弹一个黑框会非常难看。
inline void suppressConsoleWindow(QProcess &process)
{
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier(
        [](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
            args->startupInfo->dwFlags &= ~STARTF_USESHOWWINDOW;
            args->startupInfo->dwFlags |= STARTF_USESHOWWINDOW;
            args->startupInfo->wShowWindow = SW_HIDE;
        });
#else
    Q_UNUSED(process);
#endif
}

/// 跨平台隐藏控制台（阻塞式场景：QProcess::start 之后调用同样有效）
inline void hideConsole(QProcess &process)
{
    suppressConsoleWindow(process);
}

} // namespace vf
