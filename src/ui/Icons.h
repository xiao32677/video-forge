#pragma once

// =============================================================================
//  VideoForge - 矢量图标库
//
//  为什么不用图片资源：
//    * 图标需要跟随主题换色（深色/浅色/强调色/危险色），图片资源做不到
//    * 需要在高 DPI 下保持锐利，位图会糊
//    * 用 QPainter 现场绘制，任意尺寸都清晰，且不增加打包体积
//
//  所有图标都在 24x24 的逻辑坐标系里设计，绘制时按目标尺寸缩放。
// =============================================================================

#include <QColor>
#include <QIcon>
#include <QPixmap>

namespace vf {
namespace icons {

enum class Glyph {
    Add,          ///< 加号
    Folder,       ///< 文件夹
    FolderOpen,   ///< 打开的文件夹
    Stack,        ///< 一叠（清空列表）
    Trash,        ///< 垃圾桶（移除）
    Play,         ///< 播放
    PlayCircle,   ///< 圆形播放（开始转换）
    Pause,        ///< 暂停
    Stop,         ///< 停止
    Gear,         ///< 设置
    Film,         ///< 胶片（视频）
    Music,        ///< 音符（音频）
    Picture,      ///< 图片
    Check,        ///< 对勾
    Cross,        ///< 叉
    ChevronDown,  ///< 下拉箭头
    ChevronRight, ///< 右箭头
    Sun,          ///< 太阳
    Moon,         ///< 月亮
    Info,         ///< 信息
    Sparkle,      ///< 星芒（优化/智能）
    Download,     ///< 下载
    Refresh,      ///< 刷新
    Clock,        ///< 时钟
    Gauge,        ///< 仪表（速度）
    Scissors,     ///< 剪刀（裁剪）
    Shield,       ///< 盾牌（隐私）
    Rocket,       ///< 火箭（硬件加速）
    Copy,         ///< 复制
    List,         ///< 列表（队列）
    Volume,       ///< 音量
    Monitor,      ///< 显示器（输出）
    Convert,      ///< 转换（双箭头）
    Drive,        ///< 硬盘（输出目录）
    Terminal,     ///< 终端（命令行）
    Sliders,      ///< 滑块（参数）
    Bolt,         ///< 闪电
    Warning,      ///< 警告
};

/// 生成指定颜色的图标（同时提供 1x / 2x 位图，适配高 DPI）
QIcon icon(Glyph glyph, const QColor &color, int size = 20);

/// 生成像素图
QPixmap pixmap(Glyph glyph, const QColor &color, int size = 20, qreal devicePixelRatio = 1.0);

} // namespace icons
} // namespace vf
