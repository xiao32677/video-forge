#pragma once

// =============================================================================
//  VideoForge - 应用图标
//  优先使用资源里的 .ico / .png；资源缺失时用代码现场画一个，保证永远不会"没图标"。
// =============================================================================

#include <QIcon>
#include <QPixmap>

namespace vf {
namespace appicon {

/// 应用/窗口图标（多尺寸）
QIcon applicationIcon();

/// 指定尺寸的 logo（用于关于对话框、欢迎页）
QPixmap logo(int size);

} // namespace appicon
} // namespace vf
