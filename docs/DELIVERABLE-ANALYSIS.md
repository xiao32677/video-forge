# VideoForge 视频格式转换器 — 交付物分析

> 阶段：需求与交付物分析（编码前）
> 技术选型：**Qt 6.9.3 (mingw_64) + C++17 + CMake/Ninja**
> 形态：**Windows 原生桌面客户端（绿色便携）**

---

## 0. 一句话定义

一个 **给普通人用的 Windows 桌面客户端**：把任意常见视频/音频文件拖进来，立刻看到缩略图与完整媒体信息，
选一个预设（或自定义参数），点一下"开始转换"，就能拿到目标格式文件。全程可视化、可暂停、可批量、可离线。

---

## 1. 交付物清单（Deliverables）

| # | 交付物 | 形式 | 验收标准 |
|---|--------|------|----------|
| D1 | **可运行客户端** | `VideoForge.exe` + Qt 运行库 + `ffmpeg.exe/ffprobe.exe`，打成绿色便携 zip | 在干净的 Win10/11 x64 上解压即用，无需装 Qt / 无需装 ffmpeg |
| D2 | **完整源码** | CMake 工程（C++17，Qt6 Widgets/Multimedia/Concurrent） | 一条命令可重新构建出与 D1 一致的产物 |
| D3 | **构建/打包脚本** | `scripts/build.ps1`、`scripts/package.ps1`、`scripts/get-ffmpeg.ps1` | 开发者一条命令完成 配置→编译→部署→打包 |
| D4 | **文档** | `README.md`、`docs/SESSION_SUMMARY.md`（会话总结+决策+反馈）、`docs/ARCHITECTURE.md`、`docs/TEST_REPORT.md` | 会话总结必须含"开发者决策选择与反馈记录" |
| D5 | **自动化测试** | CTest 单元测试（信息解析/命令构建/格式化）+ 端到端转换测试脚本 | `ctest` 全绿；端到端脚本用合成视频跑通真实转换并校验产物 |
| D6 | **GitHub 仓库** | 公开仓库 + Tag `v1.0.0` + Release 附件（便携 zip） | 链接可直接访问、可下载、可复现 |

---

## 2. 需求映射（题目 7 条 → 实现方案）

### 需求 1：加载视频信息显示 + 获取缩略图显示
- **信息获取**：`ffprobe -v quiet -print_format json -show_format -show_streams -show_chapters`
  → 解析为 `MediaInfo` 结构体：容器格式、时长、总码率、文件大小、视频流（编码器/分辨率/帧率/像素格式/色彩空间/位深/码率）、
  音频流（编码器/采样率/声道数/声道布局/码率）、字幕流、章节数。
- **缩略图**：
  - 封面帧：智能定位（跳过片头黑帧：在 10%~40% 处取样，取"信息熵/亮度"合格的一帧），`ffmpeg -ss <t> -frames:v 1 -vf scale=...`
  - **胶片条**：抽 12~16 帧拼成一条时间轴缩略图（用户能一眼看出内容分布，也用于裁剪时定位）
  - **磁盘缓存**：以「文件路径+mtime+size+时间点」做 SHA1 key，缓存到 `%LOCALAPPDATA%/VideoForge/thumbcache`，二次打开秒显
  - 后台线程抽取，UI 不阻塞；抽取中显示骨架占位

### 需求 2：转换用 ffmpeg 命令行
- `QProcess` 组装并执行命令行，**不使用 ffmpeg API**（严格按要求）。
- 进度：`-progress pipe:1 -nostats` → 解析 `out_time_us / frame / fps / bitrate / speed / total_size / progress=end`
  → 换算百分比、当前帧率、实时速度、剩余时间 ETA。
- 界面提供 **"命令行" 实时日志面板**，用户可复制出完整命令自己去终端跑（工程可验证性）。

### 需求 3：自主优化点（详见 `SESSION_SUMMARY.md` 决策表）
1. **批量队列 + 最多 N 路并发**（默认 2，可调），每任务独立进度/状态
2. **预设系统**：MP4(H.264/AAC)、MP4(HEVC)、MKV 无损封装/Remux、WebM(VP9/Opus)、AVI、MOV、GIF、纯音频(MP3/AAC/WAV/FLAC)
3. **硬件加速自动探测**（NVENC / QSV / AMF）一键切换软/硬编码，并标注可用性
4. **裁剪 + 预览定位**：起始/结束时间 + 胶片条点击定位 + 该帧缩略图预览
5. **输出体积预估**：按 CRF/码率/时长/分辨率估算，开始前就看到大概多大
6. **任务可控**：暂停（Windows `NtSuspendProcess` / 取消 / 重试）、完成后自动打开输出目录、系统通知
7. **零配置依赖引导**：程序启动自动定位 ffmpeg（内置 → PATH → 常见路径 → 手动选择 → **引导一键下载**）
8. **体验细节**：拖拽文件/文件夹入窗、剪贴板粘贴路径、深色/浅色主题切换、配置持久化(QSettings)、
   窗口/列宽记忆、任务栏进度条、重名自动加序号、中文/Unicode/长路径安全、异常情况明确报错文案
9. **一键对比**：转换完成后可直接播放输出文件校验结果

### 需求 4：会话总结 md
`docs/SESSION_SUMMARY.md`：需求理解 → 技术选型对比 → **逐条决策记录（决策/备选/理由/代价）** →
遇到的问题与解决 → 测试与验证证据 → 已知限制 → **开发者（用户）反馈与我的响应记录**。

### 需求 5：GitHub
公开仓库 `video-forge`，含 README（带界面截图）、源码、脚本、文档、Tag、Release（便携 zip）。

### 需求 6：界面选型必须与"客户端"强关联
**选型：Qt 6 Widgets 原生桌面客户端。** 客户端要素对标清单：

| 客户端特征 | 本项目的落地 |
|---|---|
| 原生窗口 | Win32 原生窗口、菜单栏/工具栏/状态栏、可缩放、DPI 感知 |
| 桌面集成 | 系统拖放（文件/文件夹）、剪贴板、系统托盘、任务栏进度条、默认程序可关联 |
| 本地能力 | 直接读写本地文件系统、调用本地 ffmpeg 进程、本地磁盘缓存 |
| 离线可用 | 全程离线，无浏览器、无网络依赖 |
| 可分发 | `windeployqt` 打包成绿色便携 zip，双击即用，无需安装运行时 |
| 原生播放 | Qt Multimedia 内嵌播放器预览源视频与成品 |

**被排除的选项及理由**（审核重点）：
- 纯 Web 页面 / 在线转换站 → 与"客户端"关联度 **0**，直接出局。
- Electron / Tauri → 本质是浏览器壳或 WebView 承载，客户端属性弱，且分发体积大、依赖 Node。
- Python + GUI 库 → 解释器/虚拟环境依赖重、分发体验差，不符合"给人用"的目标。
- Qt Quick(QML) → 也是 Qt 客户端，但 Widgets 更适合桌面工具类密集表单与列表交互，且本机环境即带该模块。

### 需求 7：给人用 + 必须自测
- 交付前必须**真实运行**：加载真实视频 → 显示信息与缩略图 → 跑完一次真实转换 → 校验输出文件可播放。
- 产出 `docs/TEST_REPORT.md`：测试环境、用例表、截图证据、失败用例与修复记录。
- 所有错误路径都要有**人话文案**（例如"找不到 ffmpeg"要给出解决动作，而不是抛异常）。

---

## 3. 技术架构

```
VideoForge/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── core/                     # 与 UI 无关、可单测的纯逻辑
│   │   ├── FfmpegLocator.{h,cpp}     # 定位/校验 ffmpeg & ffprobe，缺失时引导下载
│   │   ├── MediaProbe.{h,cpp}        # ffprobe JSON → MediaInfo
│   │   ├── MediaInfo.h               # 数据结构 + 人类可读格式化
│   │   ├── ThumbnailService.{h,cpp}  # 封面帧/胶片条抽取 + 磁盘缓存 + 后台队列
│   │   ├── CommandBuilder.{h,cpp}    # 转换参数 → ffmpeg 参数列表（可单测）
│   │   ├── ConvertPreset.{h,cpp}     # 预设定义与序列化
│   │   ├── ProgressParser.{h,cpp}    # -progress 输出解析（可单测）
│   │   ├── HwAccelDetector.{h,cpp}   # NVENC/QSV/AMF 探测
│   │   └── SizeEstimator.{h,cpp}     # 输出体积预估
│   ├── job/
│   │   ├── ConvertJob.{h,cpp}        # 单任务：QProcess 生命周期/暂停/取消/重试
│   │   └── JobQueue.{h,cpp}          # 队列调度 + 并发上限
│   └── ui/
│       ├── MainWindow.{h,cpp}        # 主窗口骨架、菜单、状态栏、拖放
│       ├── MediaInfoPanel.{h,cpp}    # 左侧：缩略图 + 胶片条 + 信息表
│       ├── PlayerPreview.{h,cpp}     # Qt Multimedia 内嵌播放器
│       ├── JobListPanel.{h,cpp}      # 中间：任务队列表 + 进度
│       ├── ConvertSettingsPanel.{h,cpp} # 右侧：预设与参数
│       ├── LogPanel.{h,cpp}          # 命令行与日志
│       ├── Theme.{h,cpp}             # 深/浅色 QSS 主题
│       └── widgets/                  # 通用控件（卡片、时间轴、徽标）
├── assets/  (图标/样式)
├── tests/   (CTest 单元测试 + 端到端脚本)
├── scripts/ (build/package/get-ffmpeg)
└── docs/    (DELIVERABLE-ANALYSIS / SESSION_SUMMARY / ARCHITECTURE / TEST_REPORT)
```

**线程模型**：UI 主线程；`MediaProbe` 与 `ThumbnailService` 在 `QThreadPool` 后台执行；
每个 `ConvertJob` 持有独立 `QProcess`，由 `JobQueue` 按并发上限调度；进度经信号槽回主线程刷新 UI。

**关键数据流**：
```
拖入文件 → (后台) ffprobe → MediaInfo → 信息面板 + 体积预估
                     └→ (后台) ffmpeg 抽帧 → 缩略图/胶片条 → 缓存并显示
点开始 → CommandBuilder(预设+参数) → JobQueue → ConvertJob(QProcess)
        └→ -progress 流 → ProgressParser → 进度条/速度/ETA → 完成 → 通知 + 可选播放校验
```

---

## 4. 三日里程碑

| 阶段 | 目标 | 产出 |
|---|---|---|
| Day 1 | 工程骨架 + 信息面板 + 缩略图 | 可编译运行，拖入视频能看到信息与缩略图 |
| Day 2 | 转换内核 + 队列 + 进度 + 预设 | 真实跑通一次转换，进度/暂停/取消可用 |
| Day 3 | 优化点 + 打包 + 测试 + 文档 + GitHub | 便携 zip、测试报告、会话总结、仓库链接 |

---

## 5. 风险与对策

| 风险 | 对策 |
|---|---|
| 运行环境无 ffmpeg | 随包分发 + 启动自动定位 + 引导一键下载（`scripts/get-ffmpeg.ps1` 同逻辑内置） |
| ffmpeg 许可（GPL/LGPL） | README 明确标注来源与许可，不修改二进制；提供自行替换说明 |
| 中文路径 / 长路径 / Unicode | 全程 `QString`/UTF-16 处理，QProcess 按参数列表传参（不拼 shell 字符串），避免转义问题 |
| 大文件抽取缩略图卡顿 | 后台线程 + 磁盘缓存 + 抽取期间骨架占位 |
| 硬编码器不可用导致失败 | 探测失败自动回退软编码，并在界面明确提示 |
| 分发体积（Qt+ffmpeg 约 80~150MB） | 提供"精简包"（仅核心 DLL）与"完整包"两种打包开关 |

---

## 6. 待确认事项（编码前）

1. **工具链**：Qt 6.9.3 + MinGW 13.1 + CMake/Ninja（本机已就绪，无需安装）— 是否有异议？
2. **内嵌播放器**：使用 Qt Multimedia（需一并分发其 av*.dll；也可关闭，仅用缩略图/胶片条预览）。
3. **打包形式**：绿色便携 zip（免安装）为主；是否需要额外做安装包（NSIS/Inno）？
4. **界面语言**：默认中文；是否需要中英双语切换？
5. **GitHub 推送凭据**：本机无 `gh`、无 token，需要提供 Personal Access Token，或我先完成本地仓库+提交，由你执行推送。
