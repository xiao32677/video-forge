#pragma once

// =============================================================================
//  VideoForge - 自定义样式（QProxyStyle）
//
//  QSS 能覆盖大部分绘制，但有两类控件 QSS 表达不出来：
//    * 下拉框的小箭头（QSS 的 down-arrow 需要图片资源）
//    * 复选框的对勾（QSS 画不出勾，只能画色块）
//  这两处用代理样式接管，就能做到"矢量箭头 + 真正的对勾"，
//  而且一次性作用于全部控件，不用逐个替换成自定义子类。
// =============================================================================

#include <QProxyStyle>

namespace vf {

class UiStyle : public QProxyStyle
{
public:
    UiStyle();

    void drawPrimitive(PrimitiveElement element, const QStyleOption *option,
                       QPainter *painter, const QWidget *widget = nullptr) const override;

    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
};

} // namespace vf
