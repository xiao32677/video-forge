#pragma once

// =============================================================================
//  VideoForge - ffmpeg / ffprobe 可执行文件定位与校验
//
//  查找顺序（"给人用"的关键：用户永远不该看到"找不到 ffmpeg"却不知道怎么办）：
//    1. 用户显式配置过的路径（QSettings 持久化）
//    2. 应用自带目录：<exe目录>/ffmpeg/bin/、<exe目录>/ffmpeg/、<exe目录>/tools/ffmpeg/bin/
//    3. 应用目录下 bin/
//    4. 系统 PATH
//    5. 常见安装位置（winget / scoop / chocolatey / C:\ffmpeg）
//    6. 通过注册表读取用户手动选择过的路径
// =============================================================================

#include <QObject>
#include <QString>
#include <QStringList>

namespace vf {

struct FfmpegTools
{
    QString ffmpegPath;
    QString ffprobePath;
    QString version;        ///< 形如 "9.0.2"
    QString buildConfig;    ///< 形如 "gcc 16.2.0 (Rev3, Built by MSYS2 project)"
    bool valid() const { return !ffmpegPath.isEmpty() && !ffprobePath.isEmpty(); }
};

class FfmpegLocator : public QObject
{
    Q_OBJECT

public:
    explicit FfmpegLocator(QObject *parent = nullptr);

    /// 同步查找（启动时调用，耗时 < 200ms）；找到后缓存结果
    FfmpegTools locate();

    /// 使用指定的 ffmpeg 可执行文件路径（同目录下推断 ffprobe）
    bool useExecutable(const QString &ffmpegPath);

    /// 已定位到的工具（未定位则为空对象）
    const FfmpegTools &tools() const { return m_tools; }

    /// 上次查找失败的人类可读原因 + 建议动作
    QString failureReason() const { return m_failureReason; }

    /// 候选查找目录（用于界面提示与诊断）
    static QStringList candidateDirectories();

    /// 允许用户配置的持久化路径
    static QString configuredPath();
    static void setConfiguredPath(const QString &path);

    /// 校验某个可执行文件是否真的能跑起来，返回版本号（失败返回空串）
    static QString probeVersion(const QString &executablePath);

signals:
    void toolsChanged();

private:
    bool validateDirectory(const QString &binDirectory);
    FfmpegTools m_tools;
    QString m_failureReason;
    bool m_located = false;
};

} // namespace vf
