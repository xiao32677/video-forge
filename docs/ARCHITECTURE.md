# VideoForge 架构说明

> 目标读者：接手维护的开发者，或想了解"AI 是怎么把这个客户端搭起来的"评审。
> 一句话：**分层清晰、核心逻辑与界面零耦合、关键路径全部可单测。**

---

## 1. 分层总览

```
┌──────────────────────────────────────────────────────────────────────┐
│  ui/   界面层（Qt Widgets）                                          │
│  MainWindow / MediaLibraryPanel / PreviewPanel / FilmstripWidget     │
│  ConvertSettingsPanel / JobQueuePanel / LogPanel / Theme / AppIcon   │
│  职责：呈现、交互、把用户意图翻译成 core 层的输入                      │
└───────────────▲──────────────────────────────┬───────────────────────┘
                │ 信号槽（异步通知）             │ 直接调用（同步纯函数）
┌───────────────┴──────────────────────────────▼───────────────────────┐
│  job/  任务层                                                        │
│  ConvertJob   —— 一个 ffmpeg 进程的完整生命周期（启动/进度/暂停/取消）  │
│  JobQueue     —— 队列与并发调度、批量控制                              │
└───────────────▲──────────────────────────────┬───────────────────────┘
                │                              │
┌───────────────┴──────────────────────────────▼───────────────────────┐
│  core/  纯逻辑层（不 include 任何 ui/ 头文件，可脱离界面单测）          │
│  Format          人类可读格式化                                        │
│  MediaInfo       数据模型                                             │
│  FfmpegLocator   ffmpeg/ffprobe 定位与校验                            │
│  MediaProbe      ffprobe JSON → MediaInfo（parseJson 是纯函数）        │
│  ThumbnailService 抽帧 / 封面挑帧 / 胶片条 / 磁盘缓存                  │
│  ConvertPreset   预设定义                                             │
│  CommandBuilder  ConvertOptions → ffmpeg 参数列表（纯函数）            │
│  ProgressParser  -progress 输出解析（纯函数）                         │
│  HwAccelDetector 硬件编码器识别（解析函数是纯函数）                     │
│  SizeEstimator   输出体积预估                                         │
└──────────────────────────────────────────────────────────────────────┘
```

**为什么这么分**：`core/` 里的东西全是"输入 → 输出"的确定性逻辑，把它们抽成
`videoforge_core` 静态库后，单元测试不需要起 GUI、不需要真视频、不需要 ffmpeg，
209 项断言在 1 秒内跑完。这是"我能证明它是对的"的基础。

---

## 2. 关键数据流

### 2.1 导入 → 信息 → 缩略图

```
用户拖入文件
   │
   ▼
MediaLibraryPanel::addPaths()
   ├─ 展开文件夹（递归，最多 4 层）
   ├─ 按扩展名过滤（fmt::isSupportedMediaFile）
   ├─ 去重（按规范化绝对路径）
   └─ 立刻插入占位项（避免界面卡住）
   │
   ▼  pathsAdded 信号
MainWindow::probeFile()  →  MediaProbe::probeAsync()
   │                          └─ QtConcurrent::run 后台线程执行 ffprobe
   ▼  probed 信号（回 UI 线程）
MainWindow::onMediaProbed()
   ├─ 写入 m_infoCache
   ├─ 刷新媒体库副标题 + 缩略图
   └─ 若是当前选中文件：
        ├─ PreviewPanel::showMedia()   填充信息表 + 显示"正在生成缩略图"
        ├─ ConvertSettingsPanel::setMediaInfo()  刷新裁剪上限与体积预估
        └─ refreshThumbnails()
             ├─ ThumbnailService::requestCover()      → pickCoverTimestamp() 挑帧
             └─ ThumbnailService::requestFilmstrip()  → 12 帧均匀抽帧（带缓存）
```

**缩略图挑帧算法**（`ThumbnailService::pickCoverTimestamp`）：
在时长的 18% / 35% / 8% / 55% / 75% 五个候选点各抽一帧，
计算灰度图的**标准差**作为"信息量"评分（纯黑/纯色接近 0），取最高分；
一旦某帧评分 > 26 就提前结束，避免不必要的解码。这解决了"封面永远是黑屏片头"的问题。

**缓存键** = `SHA1(缓存版本 | 绝对路径 | 文件 mtime | 文件大小 | 时间点 | 目标尺寸)`，
落在 `%LOCALAPPDATA%/VideoForge/cache/VideoForge/thumbnails/*.png`。
路径里的 mtime + size 保证文件被替换后缓存自动失效。

### 2.2 转换

```
用户点"开始转换"
   │
   ▼
MainWindow::onStartConversion()
   ├─ 校验 ffmpeg 可用（否则引导用户设置路径）
   ├─ 遍历媒体库文件
   │    ├─ 缺 MediaInfo 的同步补一次 probeSync（保证命令构建正确）
   │    ├─ CommandBuilder::build()   ← 预检！不可能完成的任务在这里就拦下
   │    │     └─ plan.valid == false → 计入 skipped + 人话错误原因
   │    └─ JobQueue::enqueue(info, preset, options)
   ▼
JobQueue::startScheduling() → pump()
   └─ 按 maxConcurrent（默认 2）派发 Queued 任务
   ▼
ConvertJob::start()
   ├─ 建输出目录
   ├─ QProcess::start(ffmpeg, args)    ← CreateProcess 带 CREATE_NO_WINDOW，不弹黑框
   └─ readyReadStandardOutput → ProgressParser::feed()
        └─ 每变化 > 0.2% 才发 progressChanged（避免刷爆事件循环）
   ▼
JobQueuePanel::updateJob()（自绘进度条代理）+ LogPanel::appendLog()
   ▼
退出码 0 → Completed ／ 非 0 → Failed（错误翻译成人话）／ 用户取消 → Canceled
   ▼
全部结束 → MainWindow::onAllJobsFinished() → 托盘通知 + 状态栏汇总
```

**命令构建要点**（`CommandBuilder::build`）：
1. 全局加 `-progress pipe:1 -nostats -nostdin -hide_banner`，这是进度可视化的前提
2. `-ss` 放在 `-i` **之前**，走关键帧快速定位（长视频上快一个数量级）
3. `-map 0:v:0? -map 0:a:0?` 只取第一条视频/音频，避免多音轨带来的意外
4. 硬件加速时把 `libx264` 映射成 `h264_nvenc` 并改用 `-rc vbr -cq`；
   预设不支持硬件时**回退软编码并给出提示**，绝不因为勾了硬件就失败
5. 10bit / yuv444 源在 H.264 输出时补 `-pix_fmt yuv420p`，防止老设备黑屏
6. 用户自定义参数里禁止出现 `-i / -progress / -y / -n`，保护关键控制流

### 2.3 线程模型

| 执行体 | 位置 | 说明 |
|---|---|---|
| UI 主线程 | 全部 `ui/` | 只做渲染与轻量状态更新 |
| QThreadPool | `MediaProbe::probeAsync`、`ThumbnailService` | 探测与抽帧，避免界面卡顿 |
| QProcess 子进程 | `ConvertJob` | 每个任务一个 ffmpeg 进程，天然隔离；崩溃不影响主程序 |
| 挂起/恢复 | `NtSuspendProcess` / `NtResumeProcess` | Windows 下暂停 ffmpeg 进程（含其线程） |

`ThumbnailService` 用一个原子 `generation` 计数器做**结果作废**：
用户快速切换文件时，旧任务即使跑完也不会污染当前界面。

---

## 3. 界面结构与视觉规范

### 3.1 布局

- 顶部两行（**自定义容器**，不是 QToolBar）：
  第一行 = 品牌（logo + 名称 + 副标题）+ 右侧两个状态胶囊（硬件加速能力 / 队列统计）
  第二行 = 操作按钮（每个 QAction 包成 QToolButton，用 `setDefaultAction` 与菜单保持同步）
- 主体 = 三栏 `QSplitter`：媒体库 · 预览 · 转换设置
- 下部 = 任务队列表 + 运行日志/命令行，整体是纵向 `QSplitter`
- 窗口几何、分栏比例、主题、输出目录、并发数全部持久化到 `QSettings`

> 为什么不用 QToolBar：实测它内部的 `Expanding` 弹簧不生效，
> 右侧状态胶囊会紧贴品牌、右边留一大片空白。换成自定义 `QFrame` + `QHBoxLayout` 后完全可控。

### 3.2 视觉系统的四个层次

"界面单调"的根因通常是这四件事没做，本项目逐个解决：

| 层次 | 做法 |
|---|---|
| **背景层次** | 四级背景：`window`（窗口）/ `surface`（卡片）/ `elevated`（悬停·抬升）/ `base`（输入）。面板之间靠背景差异 + 圆角卡片边框区分，而不是全靠分割线 |
| **图标** | 自建矢量图标库 `ui/Icons.h`（37 个图标）。图标是**代码画出来的**，因此可以跟随主题换色、任意 DPI 清晰、零资源体积。带缓存，避免每帧重画 |
| **颜色语义** | 强调色（蓝→紫渐变，用于主按钮/选中/进度）、成功（绿）、警告（黄）、危险（红），在状态胶囊、进度条、日志级别、队列状态上语义一致 |
| **文字层级** | 英雄标题 17pt/800 → 面板标题 14pt/700 → 卡片标题 13pt/700 → 正文 13pt → 提示 11.5pt |

### 3.3 QSS + QProxyStyle + 自绘，三者的分工

这是本项目的界面实现中最值得说明的一点——**不是所有东西都能用 QSS 画**：

| 需求 | 用什么实现 | 为什么 |
|---|---|---|
| 圆角、悬停、选中、渐变、间距 | **QSS**（`ui/Theme.cpp`） | 声明式，改起来快 |
| 下拉箭头、复选框对勾 | **QProxyStyle**（`ui/UiStyle.cpp`） | QSS 画不出箭头和对勾（`down-arrow` / `indicator:checked` 需要图片资源）。代理样式能一次性作用于所有控件，不用把 20 处控件换成自定义子类 |
| 媒体库列表项、队列状态胶囊、进度条、胶片条、图标 | **自绘**（`QStyledItemDelegate` / `paintEvent` / `QPainter`） | 默认绘制在深色下就是方块，做不出"客户端"该有的质感 |
| 卡片式设置分组 | **SectionCard**（`ui/UiWidgets.h`） | QGroupBox 的标题放不下图标，观感很素 |

细节与坑：
- QSS 的 `border-radius` **不会裁剪子控件**，所以视频画面区额外套了一层圆角遮罩
  （`QRegion` + `QPainterPath`），并且必须监听控件**自身**的 resize 来更新，
  否则首次布局未完成时算出的遮罩会把画面永久裁小
- 主题色板集中在一个 `Palette` 结构体里，深/浅两套；QSS 替换用**长 key 优先**策略
  （`%ACCENT_TEXT%` 必须先于 `%ACCENT%` 和 `%TEXT%` 替换，否则会被拆坏）
- 自绘控件一律用 `theme::*()` 取色，因此主题切换后**无需重建控件**，
  只需 `update()`；只有 QIcon 需要重新生成（`MainWindow::refreshIcons()`）


---

## 4. 依赖与构建

| 依赖 | 版本 | 用途 |
|---|---|---|
| Qt | 6.9.3 (mingw_64) | Core / Gui / Widgets / Concurrent / Svg / Multimedia / MultimediaWidgets |
| MinGW | 13.1.0 (x86_64-posix-seh) | 编译器（与 Qt 官方 MinGW 构建 ABI 一致） |
| CMake | 3.21+ | 构建系统 |
| Ninja | 任意 | 生成器 |
| ffmpeg | 9.0.2 (gyan.dev essentials) | 运行期外部依赖，随包分发 |

构建产物：
- `libvideoforge_core.a` —— 纯逻辑静态库
- `VideoForge.exe` —— GUI 可执行（`-static-libgcc -static-libstdc++ -static`，减少运行库依赖）
- `vf_tests.exe` —— 单元测试

**关于 `-static`**：完全静态链接 MinGW 运行库可以避免用户机器缺
`libstdc++-6.dll / libgcc_s_seh-1.dll / libwinpthread-1.dll`。
注意 `-static` 与 Qt DLL 混用需谨慎（异常/内存跨模块边界），
本项目所有跨边界对象都通过 Qt 自己的容器与信号槽传递，未发现异常。

---

## 5. 容易踩的坑（已处理）

| 坑 | 处理方式 |
|---|---|
| ffmpeg 进度不好拿 | 用 `-progress pipe:1` 而不是解析 stderr 的 `\r` 刷屏行 |
| 每转一个文件弹一个黑色控制台窗口 | `QProcess::setCreateProcessArgumentsModifier` + `CREATE_NO_WINDOW` + `STARTF_USESHOWWINDOW/SW_HIDE` |
| 中文路径 / 空格 | 全程 `QString` + 参数列表传参（不拼 shell 字符串）；展示命令时单独做引号转义 |
| 快速切换文件导致缩略图错乱 | 原子 generation 计数器作废过期结果 |
| 队列取消引发重入崩溃 | `removeJob` 先从列表 `takeAt` 再 `cancel()`（详见 TEST_REPORT 的崩溃复盘） |
| QComboBox 按最长项要宽度，撑破窄侧栏 | `AdjustToMinimumContentsLengthWithIcon` |
| 长路径 QLabel 撑破布局 | 中间省略 `elidedText` + tooltip 显示完整路径 |
| 关机/关窗时有任务在跑 | 弹窗三选：继续（最小化到托盘）/ 取消任务并退出 / 返回 |
