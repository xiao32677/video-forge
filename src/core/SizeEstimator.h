#pragma once

// =============================================================================
//  VideoForge - 输出体积预估
//
//  目的：让用户在点"开始转换"之前就知道大概会生成多大的文件，
//  这是普通用户最关心、也最容易踩坑的一点（"我怎么转完变 4 个 G 了"）。
// =============================================================================

#include "core/CommandBuilder.h"
#include "core/ConvertPreset.h"
#include "core/MediaInfo.h"

#include <QString>

namespace vf {

struct SizeEstimate
{
    qint64 bytes = 0;
    bool reliable = false;      ///< true=按码率精确计算；false=按经验模型推算
    QString explanation;        ///< 给用户看的解释，例如"按 5.0 Mb/s × 00:03:21 估算"
    QString comparison;         ///< 与源文件的对比，例如"约为原文件的 62%"
};

class SizeEstimator
{
public:
    /// 估算一次转换的输出体积
    static SizeEstimate estimate(const MediaInfo &info,
                                 const ConvertPreset &preset,
                                 const ConvertOptions &options);

    /// 人类可读的一行描述（可直接放到界面上）
    static QString describe(const SizeEstimate &estimate);

private:
    /// CRF -> 目标码率的经验模型（基于 1080p / 中低运动量画面的拟合）
    static double crfToBitrateMbps(int crf, const QString &codec, double fps);
};

} // namespace vf
