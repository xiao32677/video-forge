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

/// 磁盘空间判定结果
struct FreeSpaceVerdict
{
    bool enough = true;
    qint64 requiredBytes = 0;   ///< 带安全余量后的需求量
    qint64 freeBytes = 0;
    qint64 missingBytes = 0;    ///< 还差多少（enough 为 true 时是 0）
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

    /// 判断"剩余空间够不够"。
    /// 估算本身有误差（CRF 模式是经验模型），所以额外留一个安全系数；
    /// 但报给用户的差额是净差额，不虚报。
    /// 抽成纯函数是为了能确定性测试——真去把磁盘写满显然不现实。
    static FreeSpaceVerdict evaluateFreeSpace(qint64 estimatedBytes, qint64 freeBytes,
                                             double safetyFactor = 1.10);

private:
    /// CRF -> 目标码率的经验模型（基于 1080p / 中低运动量画面的拟合）
    static double crfToBitrateMbps(int crf, const QString &codec, double fps);
};

} // namespace vf
