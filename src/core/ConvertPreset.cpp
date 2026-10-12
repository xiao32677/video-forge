#include "core/ConvertPreset.h"

namespace vf {

QString hardwareAccelName(HardwareAccel accel)
{
    switch (accel) {
    case HardwareAccel::Nvenc:       return QStringLiteral("NVIDIA NVENC");
    case HardwareAccel::Qsv:         return QStringLiteral("Intel Quick Sync");
    case HardwareAccel::Amf:         return QStringLiteral("AMD AMF");
    case HardwareAccel::VideoToolbox:return QStringLiteral("Apple VideoToolbox");
    case HardwareAccel::None:        break;
    }
    return QStringLiteral("软件编码 (CPU)");
}

const QList<ConvertPreset> &builtinPresets()
{
    static const QList<ConvertPreset> presets = {
        // ------------------------------------------------------------ 视频
        {
            QStringLiteral("mp4-h264"), QStringLiteral("MP4 · 通用兼容（H.264 + AAC）"),
            QStringLiteral("视频"),
            QStringLiteral("最稳妥的选择：手机、电视、微信、剪辑软件全都认。"),
            QStringLiteral("mp4"), QStringLiteral("mp4"),
            QStringLiteral("libx264"), QStringLiteral("aac"),
            23, QStringLiteral("medium"), 192, true, true, true, true
        },
        {
            QStringLiteral("mp4-h265"), QStringLiteral("MP4 · 高压缩（HEVC/H.265 + AAC）"),
            QStringLiteral("视频"),
            QStringLiteral("同画质下体积约为 H.264 的 60%，但老设备可能放不动。"),
            QStringLiteral("mp4"), QStringLiteral("mp4"),
            QStringLiteral("libx265"), QStringLiteral("aac"),
            26, QStringLiteral("medium"), 192, true, true, true, true
        },
        {
            QStringLiteral("webm-vp9"), QStringLiteral("WebM · 网页友好（VP9 + Opus）"),
            QStringLiteral("视频"),
            QStringLiteral("适合网页 <video> 与开源生态，无需授权费。"),
            QStringLiteral("webm"), QStringLiteral("webm"),
            QStringLiteral("libvpx-vp9"), QStringLiteral("libopus"),
            32, QStringLiteral("good"), 128, true, true, false
        },
        {
            QStringLiteral("mkv-remux"), QStringLiteral("MKV · 无损换壳（不重编码）"),
            QStringLiteral("视频"),
            QStringLiteral("只换容器不重编码，几秒完成、画质零损失，用来换封装格式。"),
            QStringLiteral("matroska"), QStringLiteral("mkv"),
            QString(), QString(),
            0, QString(), 0, true, true, false, true
        },
        {
            QStringLiteral("mov-prores"), QStringLiteral("MOV · 剪辑友好（H.264 高码率）"),
            QStringLiteral("视频"),
            QStringLiteral("给剪辑软件（Premiere / FCP / 达芬奇）用的中间素材。"),
            QStringLiteral("mov"), QStringLiteral("mov"),
            QStringLiteral("libx264"), QStringLiteral("aac"),
            16, QStringLiteral("fast"), 256, true, true, true, true
        },
        {
            QStringLiteral("avi-xvid"), QStringLiteral("AVI · 老设备（MPEG-4）"),
            QStringLiteral("视频"),
            QStringLiteral("车载播放器、老式 DVD 机、投影仪等只认 AVI 的场景。"),
            QStringLiteral("avi"), QStringLiteral("avi"),
            QStringLiteral("mpeg4"), QStringLiteral("libmp3lame"),
            5, QString(), 192, true, true, false
        },
        {
            QStringLiteral("gif"), QStringLiteral("GIF · 动图"),
            QStringLiteral("图片"),
            QStringLiteral("做表情包/演示动图。默认 480 宽、12 fps，建议配合裁剪使用。"),
            QStringLiteral("gif"), QStringLiteral("gif"),
            QStringLiteral("gif"), QString(),
            0, QString(), 0, false, false, false
        },
        // ------------------------------------------------------------ 音频
        {
            QStringLiteral("mp3"), QStringLiteral("MP3 · 纯音频提取"),
            QStringLiteral("音频"),
            QStringLiteral("从视频里抽出声音，兼容性最好的音频格式。"),
            QStringLiteral("mp3"), QStringLiteral("mp3"),
            QString(), QStringLiteral("libmp3lame"),
            0, QString(), 192, false, true, false, false, true
        },
        {
            QStringLiteral("m4a"), QStringLiteral("M4A · 纯音频提取（AAC）"),
            QStringLiteral("音频"),
            QStringLiteral("同样体积下音质优于 MP3，苹果设备首选。"),
            QStringLiteral("ipod"), QStringLiteral("m4a"),
            QString(), QStringLiteral("aac"),
            0, QString(), 192, false, true, false, false, true
        },
        {
            QStringLiteral("wav"), QStringLiteral("WAV · 无损音频"),
            QStringLiteral("音频"),
            QStringLiteral("无压缩，体积大，用于后期处理。"),
            QStringLiteral("wav"), QStringLiteral("wav"),
            QString(), QStringLiteral("pcm_s16le"),
            0, QString(), 0, false, false, false, false, true
        },
        {
            QStringLiteral("flac"), QStringLiteral("FLAC · 无损压缩音频"),
            QStringLiteral("音频"),
            QStringLiteral("无损但体积约为 WAV 的 60%，适合音乐归档。"),
            QStringLiteral("flac"), QStringLiteral("flac"),
            QString(), QStringLiteral("flac"),
            0, QString(), 0, false, true, false, false, true
        },
        {
            QStringLiteral("m4a-copy"), QStringLiteral("M4A · 音频直接复制（最快）"),
            QStringLiteral("音频"),
            QStringLiteral("音频流不重编码，瞬间完成、音质零损失。"),
            QStringLiteral("ipod"), QStringLiteral("m4a"),
            QString(), QString(),
            0, QString(), 0, false, true, false, false, true
        },
    };
    return presets;
}

ConvertPreset presetById(const QString &id)
{
    const QList<ConvertPreset> &all = builtinPresets();
    for (const ConvertPreset &preset : all) {
        if (preset.id == id)
            return preset;
    }
    for (const ConvertPreset &preset : all) {
        if (preset.id == defaultPresetId())
            return preset;
    }
    return all.first();
}

QString defaultPresetId()
{
    return QStringLiteral("mp4-h264");
}

} // namespace vf
