#pragma once

// =============================================================================
//  VideoForge - 界面主题
//
//  一套 QSS + QPalette 组合，提供现代扁平风格的深色/浅色两套皮肤。
//  深色为默认（视频工具的主流审美，也更适合长时间盯着画面）。
// =============================================================================

#include <QColor>
#include <QString>

class QApplication;

namespace vf {
namespace theme {

enum class Mode {
    Dark,
    Light,
};

/// 应用主题（设置 QPalette + 全局 QSS）
void apply(QApplication &app, Mode mode);

/// 当前主题
Mode currentMode();
void setCurrentMode(Mode mode);

/// 在深/浅色之间切换，返回切换后的模式
Mode toggle();

/// 主色调（强调色）
QColor accent();
/// 次要文字颜色（当前主题下）
QColor mutedText();
/// 卡片背景色
QColor cardBackground();
/// 成功/警告/错误语义色
QColor successColor();
QColor warningColor();
QColor dangerColor();

/// 生成样式表（mode 显式传入，便于测试）
QString styleSheet(Mode mode);

} // namespace theme
} // namespace vf
