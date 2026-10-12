# VideoForge 测试报告

> 结论：**311 项自动断言全部通过（core 层 240 + job 层 71）；5 个端到端场景在真实
> ffmpeg 上全部跑通，产出输出文件并用 ffprobe 逐一校验；GUI 通过自动化截图模式
> 完成了真实点击路径的验证。过程中发现并修复 8 个真实缺陷（含 2 个必现崩溃/功能失效）。**

---

## 1. 测试环境

| 项 | 值 |
|---|---|
| 操作系统 | Windows（x64） |
| 编译器 | MinGW-w64 GCC 13.1.0（x86_64-posix-seh-rev1） |
| Qt | 6.9.3 (mingw_64)，含 Multimedia / Svg / Concurrent |
| CMake / Ninja | 4.3.1 / 1.12.1 |
| ffmpeg | 9.0.2-essentials_build（gyan.dev），ffmpeg + ffprobe |
| 显卡 | Intel(R) Iris(R) Xe Graphics（用于验证硬件编码探测） |
| 构建类型 | Release |

---

## 2. 测试策略

分三层，越往下越"真"：

| 层次 | 手段 | 覆盖什么 | 是否需要 GUI / ffmpeg |
|---|---|---|---|
| L1-a 单元测试 | `vf_tests.exe`（链接 `videoforge_core`） | 格式化、JSON 解析、命令构建、进度解析、编码器识别、体积估算、磁盘判定、路径生成 | 都不需要 |
| L1-b 单元测试 | `vf_tests_job.exe` + `vf_fake_ffmpeg.exe` | **任务状态机**：完成/失败/取消/重试、产物校验、队列调度、移除重入、字幕映射 | 不需要真实 ffmpeg |
| L2 端到端（脚本） | `tests/e2e_convert.cmake` | 合成视频 → 启动被测程序 → 真实转换 → ffprobe 校验输出 | 需要 |
| L3 GUI 自动化 | 程序内置 `--screenshot <dir> [--autoconvert]` | 真实加载、真实抽帧、真实转换、真实界面渲染 | 需要 |

**为什么要造一个"假 ffmpeg"**：job 层最容易出问题（历史上那个必现崩溃就在这里），
但真实 ffmpeg 的行为不可控（耗时、失败时机、输出内容都不确定）。
`tests/fake_ffmpeg.cpp` 编译出的测试替身可以按环境变量切换四种行为：

| `VF_FAKE_MODE` | 行为 | 用来测什么 |
|---|---|---|
| `ok` | 打印进度 → 自己写出非空文件 → 退出 0 | 正常完成、进度解析、产物校验通过 |
| `nooutput` | 打印进度 → 退出 0，**但不产出文件** | 产物校验能否抓住"退出码 0 但没结果" |
| `fail` | 打印错误 → 退出 1 | 失败路径与错误信息 |
| `slow` | 停 3 秒再退出 | 取消运行中的任务 |

这样"完成 / 失败 / 校验失败 / 取消 / 重试 / 队列重入"就全部可以确定性复现了。

---

## 3. L1 单元测试结果

```
============================================
 VideoForge 单元测试 (core 层)
============================================
 共 240 项断言，失败 0 项
 结果：全部通过 ✔

============================================
 VideoForge 单元测试 (job 层：任务状态机 / 队列 / 字幕)
============================================
 共 71 项断言，失败 0 项
 结果：全部通过 ✔
```

覆盖明细：

| 模块 | 关键用例 |
|---|---|
| `fmt::*` | 体积（B/KB/MB 边界）、时长（<1h / >1h / 负数）、码率、帧率（29.97 保留两位、30 整数化）、宽高比（16:9 / 4:3 / 9:16 识别）、时间戳双向转换（`ffmpegTimestamp` ↔ `parseTimestamp`） |
| `MediaProbe::parseJson` | 真实 ffprobe JSON（含视频/音频/字幕/章节）字段完整性；`avg_frame_rate` 分数解析（30000/1001 → 29.97）；10bit 从 `yuv420p10le` 推断出 10；空 JSON / 坏 JSON / 无流文件都必须给出非空的人类可读错误 |
| `CommandBuilder::build` | 默认 MP4/H.264 参数齐全；`-progress pipe:1` 存在；无损换壳走 `copy`；裁剪的 `-ss/-t` 数值正确（40-10=30）；缩放与帧率过滤器字符串；音频-only 走 `-vn`；硬件加速映射到 `h264_nvenc` 且不再出现 `libx264`；预设不支持硬件时给出 warning 并回退；无视频流却要转视频时 `valid=false`；`-n`/`-y` 切换；引号转义（空格加引号、纯中文不加） |
| `defaultOutputPath` | 命名规则、目录回落、重名自动加 `(2)`（**绝不覆盖已有文件**） |
| `ProgressParser` | 完整进度块解析；**半行跨块拼接**（`frame=2` + `00\n` → 200）；`progress=end`；`kbits/s` 与 `Mbits/s` 换算；脏数据不崩 |
| `HwAccelDetector` | 从 `ffmpeg -encoders` 文本提取编码器；NVENC 可用 / QSV 可用 / AMF 不可用的能力表；**关键：`buildCapabilities` 只能标记 `present`，`available` 必须经过真实试编码才置位**（防止"编码器编译进来了但本机跑不了"被误报为可用）；无硬件时 `bestAccel()` 安全返回 `None`；试编码接口在参数非法时安全返回 false 而不崩 |
| `SizeEstimator` | CRF 越小体积越大（单调性）；缩放显著减小体积；固定码率模式与手算值误差 < 2% 且标记为 `reliable`；copy 模式可靠 |
| `ConvertPreset` | 预设 id 唯一、字段非空；未知 id 回落到默认预设 |

---

## 4. L2 + L3 端到端结果

### 4.1 测试素材（`scripts/make-test-media.ps1` 合成，无版权问题）

| 文件 | 规格 | 用途 |
|---|---|---|
| `演示视频-720p.mp4` | 1280x720 30fps H.264 + AAC 12s | 主素材（含中文名，验证 Unicode） |
| `演示视频2-540p.avi` | 960x540 25fps MPEG-4 + MP3 8s | 老格式输入 → 现代化输出 |
| `演示视频3-HEVC.mkv` | 640x360 24fps H.265 + AC3 6s | 新编码 + 多容器 |
| `音乐-正弦波.mp3` | 44.1kHz MP3 10s | 纯音频输入；同时用于验证"用视频预设转音频文件"必须被拦下 |

### 4.2 场景与结果

| 场景 | 命令要点 | 退出码 | 结果 |
|---|---|---|---|
| A 批量转换（5 文件，深色，MP4/H.264） | `--screenshot shots --autoconvert <5个文件>` | **0** | 4 个视频成功，MP3 被正确拦下并给出原因 |
| B 浅色主题 + HEVC | `--theme light --preset mp4-h265` | **0** | 成功，截图捕捉到"转换中"状态 |
| C 音频提取 | `--preset mp3 <视频> <音频>` | **0** | 2 个 MP3 成功 |
| **D 便携包独立运行** | 解压 zip → 干净 PATH（仅 `system32`）下运行 | **0** | 截图 + 转换文件齐全，证明**不依赖外部 Qt / MinGW / ffmpeg** |
| **E 只转选中项** | 媒体库 5 个文件，只选中 1 个 | **0** | **只产出 1 个输出文件**（修复前会转全部 5 个） |
| **F 字幕保留** | `--preset mkv-remux --subtitles copy` | **0** | 输出 MKV 带 `subrip` 字幕轨；MP4 输出带 `mov_text` 字幕轨 |
| **G MKV 无损换壳** | `--preset mkv-remux` | **0** | 输出 5.44 MB，**与源文件大小完全一致** → 确认是真正复制而非重编码 |
| **H 小屏布局** | `--window-size 1366x740`（真实笔记本尺寸） | **0** | 布局完整无重叠、日志面板自动收起（见 `images/06-小屏适配.png`） |
| **I ffmpeg 一键安装** | `--install-ffmpeg <url>`（本地 HTTP 服务提供真实的 109.5 MB zip） | **0** | 3.3 秒完成下载→解压→安装→校验；装出 `ffmpeg.exe / ffprobe.exe / LICENSE`；**随后该程序用这份下载来的 ffmpeg 成功完成了一次真实转换** |

### 4.3 输出文件校验（ffprobe 实测）

```
演示视频-720p_转换后.mp4      4.07 MB | 视频: h264,1280,720,30/1 | 音频: aac,48000,1 | 容器: mov,mp4,... | 12.000000s
演示视频-720p_转换后(2).mp4   3.52 MB | 视频: hevc,1280,720,30/1 | 音频: aac,48000,1 | 容器: mov,mp4,... | 12.000000s
演示视频-720p_转换后.mkv      5.44 MB | 视频: h264 (copy)        | 音频: aac (copy)  | 容器: matroska   | 12.021000s   ← 与源同大小 = 真·无损换壳
演示视频2-540p_转换后.mp4     0.23 MB | 视频: h264,960,540,25/1  | 音频: aac,44100,1 | 容器: mov,mp4,... |  8.045714s
演示视频3-HEVC_转换后.mp4     0.12 MB | 视频: h264,640,360,24/1  | 音频: aac,48000,1 | 容器: mov,mp4,... |  6.000000s
演示视频4-带字幕_转换后.mkv    3.92 MB | 视频: h264 + aac + **subrip 字幕**               | 容器: matroska   |  9.021000s
演示视频4-带字幕_转换后.mp4    1.63 MB | 视频: h264 + aac + **mov_text 字幕**            | 容器: mov,mp4    |  9.002667s
演示视频-720p_转换后.mp3      0.28 MB | 视频: (无)              | 音频: mp3,48000,1 | 容器: mp3        | 12.000000s
音乐-正弦波_转换后.mp3        0.23 MB | 视频: (无)              | 音频: mp3,44100,1 | 容器: mp3        | 10.000000s
```

**校验要点**：分辨率/帧率/采样率与原文件一致（说明没有意外降采样）；
时长与源一致（说明没有截断/丢帧）；HEVC 预设确实产出 `hevc` 而不是回退成 h264；
音频预设产出的是 `mp3` 且**不含视频流**；**无损换壳输出与源文件字节数一致**；
**字幕轨在两种容器里都被保留**（MKV 复制原轨、MP4 转 mov_text）。

### 4.4 界面验证（截图）

| 截图 | 验证内容 |
|---|---|
| `docs/images/01-主界面-深色.png` | 媒体库 4 个文件均显示缩略图与信息摘要；胶片条 12 帧正确渲染；信息表填充；状态栏显示 ffmpeg 9.0.2 |
| `docs/images/03-转换中.png` | 队列行显示"转换中 55% / 3.25x / 约 00:00"；日志面板打印出真实的 ffmpeg 命令行；右下角总进度条 58%；按钮变为"正在转换…"且"取消全部任务"可用 |
| `docs/images/02-批量转换完成.png` | 3 个任务全部"已完成 100%"，日志逐条 ✅，状态栏"全部任务已结束：成功 3 个" |
| `docs/images/04-浅色主题.png` / `05-音频提取.png` | 浅色主题渲染正确；预设切换为音频后视频相关控件按预期禁用 |

---

## 5. 缺陷记录（真实发现并修复）

### 缺陷 1（严重·必现崩溃）：队列移除任务时重入导致越界访问

- **现象**：批量导入 4 个文件（含 1 个纯音频）点击"开始转换"后，进程以 `0xC0000005`
  （访问违规）崩溃，且**一个输出文件都没生成**。
- **定位过程**：先用 `--screenshot` 复现（截图 02 缺失 → 说明崩溃发生在开始转换后 4 秒内），
  再读代码定位到 `JobQueue::removeJob()`。
- **根因**：
  ```
  removeJob(id)
    └─ job->cancel()                       // 对"排队中"的任务是同步的
         └─ emit finished(...)             // 同步发出
              └─ 信号处理器 → removeJob(id) // 重入！
                   └─ m_jobs.removeAt(i)   // 第一次移除
    └─ m_jobs.removeAt(i)                  // 第二次移除同一位置 → 越界
  ```
  触发条件是"用视频预设转换纯音频文件"——`CommandBuilder` 判定不可行，
  于是入队后立刻被移除，正好走到这条重入路径。
- **修复**：`removeJob` 改为**先 `takeAt` 把任务摘出列表，再 `cancel()`**，
  重入时列表里已经找不到这个 id，直接返回。同时把"不可能完成的任务"改成
  **入队前预检**（`CommandBuilder::build` 先跑一遍），从源头避免创建即销毁的任务。
- **验证**：修复后场景 A 退出码 0，3 个输出文件全部生成。

### 缺陷 2（中等·界面）：右侧设置面板被长文本撑宽导致裁切

- **现象**：设置面板右侧内容（浏览按钮、质量数值框）被窗口边缘裁掉。
- **定位过程**：不靠肉眼猜，给程序加了一个 `layoutDiagnostics()`，
  在截图模式下把"最小宽度需求最大的控件"输出成文本：
  ```
  设置面板宽度 = 360，最小宽度需求 = 403
  GroupBox: 画质与尺寸        min=397
  CheckBox: 直接复制视频流（不重编码，秒完成、零损失）  min=298
  ...
  ```
  一眼看到是超长复选框文本 + 不换行 QLabel（其 `minimumSizeHint` 等于整段文本宽度）。
- **修复**：① 复选框文案收短，细节移入 tooltip；② 路径类 QLabel 设
  `QSizePolicy::Ignored` + 有上限的 `elidedText`；③ 下拉框改
  `AdjustToMinimumContentsLengthWithIcon`；④ 裁剪的两个时间输入框由横排改竖排。
- **验证**：最小宽度需求 403 → **281**，面板宽度 360，截图确认无裁切。

### 缺陷 3（轻微·工具链）：自测模式缺 `--autoconvert` 时不会退出

- **现象**：自动化脚本里只截图不转换时，程序会一直挂着。
- **修复**：非 `--autoconvert` 分支补上 1.5 秒后的 `quit()` 定时器。

### 缺陷 4（阻塞交付·打包）：绿色便携包启动即报 0xC0000135（找不到 DLL）

- **现象**：`scripts/package.ps1` 打出 zip 后，在干净环境解压运行，进程立刻以
  `0xC0000135`（STATUS_DLL_NOT_FOUND）退出，连窗口都没出来。
- **定位过程**：用 `objdump -p` 逐个查依赖，发现 Qt 自带的插件
  `platforms/qwindows.dll` 与 `multimedia/ffmpegmediaplugin.dll` 依赖
  `libstdc++-6.dll` / `libgcc_s_seh-1.dll` / `libwinpthread-1.dll`：
  ```
  platforms\qwindows.dll          → libstdc++-6.dll
  multimedia\ffmpegmediaplugin.dll → libstdc++-6.dll, libgcc_s_seh-1.dll, libwinpthread-1.dll
  ```
  而打包脚本里给 `windeployqt` 传了 `--no-compiler-runtime`，把这些运行库跳过了。
- **根因（更深一层）**：主程序当初用 `-static-libgcc -static-libstdc++ -static` 链接，
  只让"我自己的 exe"不依赖运行库，但**管不到 Qt 预编译的插件 DLL**。
  结果是"主程序静态、插件动态"——同一个进程里存在两套 C++ 运行时，
  这本身就是跨模块异常/RTTI 的经典隐患。
- **修复**：① 去掉主程序的 `-static*` 链接选项，统一走共享运行时；
  ② 打包脚本移除 `--no-compiler-runtime`，并增加一段"从工具链目录补齐运行库"的兜底逻辑。
- **验证**：重新打包（99.1 MB）后，在 `PATH` 只保留 `C:\Windows\system32;C:\Windows`
  （**不含 Qt / MinGW / ffmpeg**）的环境下解压运行 → **退出码 0**，
  产出 3 张截图 + 2 个真实转换文件。这是"解压即用"最直接的证据。

### 缺陷 5（工具链·脚本）：PowerShell 5.1 读无 BOM 的 UTF-8 脚本把中文拆坏

- **现象**：`powershell -File scripts/package.ps1` 报一堆语法错误，
  报错信息里的中文显示为 `瀹屾垚锛?zipPath` —— 典型的"UTF-8 无 BOM 被按 GBK 解码"。
- **修复**：所有 `.ps1` 脚本保存为 **UTF-8 with BOM**（Windows PowerShell 5.1 与
  PowerShell 7 都能正确识别）。已在 README 的构建说明中提示：如果用编辑器改动脚本，
  请保持 UTF-8 BOM 编码。

### 缺陷 6（正确性·误报）：硬件加速能力"假阳性"

- **现象**：界面胶囊显示「硬件加速 · NVIDIA NVENC」，但这台机器只有
  **Intel Iris Xe Graphics**，根本没有 NVIDIA 显卡。
- **根因**：`HwAccelDetector` 只解析 `ffmpeg -encoders` 的输出。
  编码器出现在这个列表里只说明**它被编译进了 ffmpeg**，
  并不代表**当前这台机器真的能跑**（缺显卡/驱动时一跑就失败）。
- **修复**：新增 `verifyEncoder()`，用 lavfi 合成 8 帧黑图，真的调用该硬件编码器编一遍，
  退出码为 0 才算 `available`；否则保持不可用并在提示里说明原因。
- **实测验证**（手工执行，非推断）：
  ```
  h264_nvenc   退出码 = -1            → 正确判定为不可用
  h264_qsv     退出码 = 0             → 正确判定为可用
  h264_amf     退出码 = -1313558101   → 正确判定为不可用
  libx264      退出码 = 0             → 软件编码始终可用
  显卡：Intel(R) Iris(R) Xe Graphics   → 与 QSV 可用的结论一致
  ```
- **单元测试**同步更新：断言 `buildCapabilities` 只标记 `present`、`available` 必须为 false，
  并覆盖非法参数下的安全返回。

### 缺陷 7（界面·浅色主题）：预览画面被裁成一条

- **现象**：切换到浅色主题后，预览区的视频/封面只显示成左上角一小块黑色区域。
- **根因**：圆角遮罩在 `PreviewPanel::resizeEvent()` 里计算，而首次触发时画面控件还没被
  布局撑开，遮罩就按"当时的小尺寸"固定下来了；控件之后变大，遮罩没有跟着更新。
  深色主题下因为截图时机较晚而恰好没暴露，属于**时序相关的偶发问题**。
- **修复**：改为监听画面控件**自身**的 resize 事件（`installEventFilter`）来更新遮罩。
- **验证**：深色 / 浅色两种主题下重新截图，画面区都正确铺满且四角为圆角。

### 缺陷 8（功能失效·严重）：MKV 无损换壳其实一直没有生效

- **发现方式**：补 job 层测试时，给"remux 必须同时映射并复制视频流"加断言，测试直接失败。
- **现象**：选了「MKV · 无损换壳（不重编码）」，程序并不会复制视频流，
  而是**悄悄用默认编码器重新编码了一遍**——用户以为几秒钟无损完成，实际等了很久还掉了画质。
- **根因**：预设用 `isAudioOnly()` 来表示"纯音频预设"，而它的实现是
  `return videoCodec.isEmpty();`。
  但 remux 预设的 `videoCodec` **本来就是空的**（空字符串的意思是"原样复制"，
  不是"没有视频"）。于是 remux 被误判成纯音频预设：
  ```
  wantVideoTrack = !preset.isAudioOnly() && info.hasVideo();   // false → 整段视频逻辑被跳过
  ```
  既不 `-map 0:v:0?`，也不 `-c:v copy`。音频那边因为走到另一条分支，
  恰好产生了 `-c:a copy`，所以旧测试里 `arguments.contains("copy")` **照样通过**——
  这就是为什么 212 条断言也没抓住它。
- **修复**：预设结构体新增明确的 `bool audioOnly` 字段，`isAudioOnly()` 改为返回该字段，
  并在 5 个音频预设上显式置位；`videoCodec 为空` 不再承担"是不是音频预设"的语义。
- **验证**：
  - 单元测试新增"remux 必须出现 `-map 0:v:0?` 且 `copy` 出现两次（视频+音频）"；
  - 端到端实测：`mkv-remux` 转换 5.44 MB 的源 → 输出 **5.44 MB**，与源完全一致，
    确认是真正复制而非重编码。

### 缺陷 9（回归·第二轮引入）：导入文件时自动选中第一个，导致"只转选中项"只转 1 个

- **现象**：批量转换 5 个文件，结果只产出 1 个输出文件。
- **发现方式**：**看截图**。截图里媒体库标题写着"媒体库 5 · 选 1"——一眼就不对。
- **根因**：上一步给"导入文件后自动预览第一个"用的 `QListWidget::setCurrentItem()`，
  它在 Qt 里会**连带改变选中状态**。而 P1 刚把"选中"定义为"只转换这些文件"，
  于是每导入一批文件，就自动变成"只转第一个"。**两个功能单独看都对，凑在一起就错了。**
- **修复**：改用 `setCurrentItem(item, QItemSelectionModel::NoUpdate)` ——
  只改"当前项"（用于预览），不动选中集。测试钩子的 `selectMatching` 反过来显式使用
  `SelectCurrent`，因为它就是要选中。
- **教训**：这条如果只跑单元测试和"退出码是不是 0"是发现不了的——退出码确实是 0，
  转换也确实成功了，只是**少转了 4 个文件**。**必须看界面**。

### 缺陷 10（界面·小屏）：1366×768 笔记本上控件互相重叠

- **现象**：窗口调到 1024×680 / 1280×700 时，胶片条与信息表的标签栏重叠，预览区被压扁。
- **根因**：三个纵向面板 + 富预览的最小高度之和超过了矮屏可用高度，
  布局无法满足最小尺寸时子控件会画出自己的矩形范围，视觉上就是重叠。
- **修复**（三步）：
  1. 降低各处最小高度（视频区 220→90、信息表 200→90、队列 140→90、日志 120→80、胶片条 76→44）；
  2. 矮屏（< 780px）**自动收起日志面板**，用 780/840 双阈值做迟滞，用户手动切换过就不再干预；
  3. 最小窗口尺寸从 1180×780 降到 1000×680。
- **踩到的坑**：第一版把"是否矮屏"写成用 `height()` 判断，但 `restoreSettings()` 执行时
  窗口还没 `resize()`/`show()`，拿到的是默认高度 → 判断永远为"高屏"。
  最终改成**按屏幕可用高度判断默认值**，再挂到 `showEvent`/`resizeEvent` 上按实际窗口高度动态调整。
- **验证**：`--window-size 1366x740` 实测截图，布局完整、无重叠、日志已自动收起
  （见 `docs/images/06-小屏适配.png`）。

### 过程性教训

- `PowerShell` 启动 WIN32 GUI 程序**不会等待**，必须 `Start-Process -Wait`，
  否则子进程会被连同父任务一起干掉——第一次自测"只截到第一张图、没有转换产物"就是这个原因，
  一度被误判为程序 bug。
- 自测模式必须显式重置界面状态（本项目用环境变量 `VF_RESET_LAYOUT=1` 忽略上次保存的
  窗口布局），否则截图不可复现——QSettings 会记住上一次的窗口/分栏尺寸。
- **"断言存在某个参数"是弱断言**。缺陷 8 逃过 200 多条断言，就是因为
  `arguments.contains("copy")` 对"视频复制"和"音频复制"一视同仁。
  改成"数一数 `copy` 出现了几次"之后它立刻暴露。写断言要盯着**语义**，不是盯着**字符串**。

---

## 6. 未覆盖 / 已知限制

| 项 | 说明 |
|---|---|
| 硬件加速实机验证 | 本次测试机器上 `ffmpeg -encoders` 无 NVENC/QSV/AMF 可用项，因此只验证了**能力探测与参数映射**（单元测试），未做真机硬件编码。程序在硬件不可用时自动回退软编码，风险可控。 |
| 超大文件（>10GB） | 未实测。`ffprobe` 给了 30 秒超时，抽帧给了 20 秒超时，理论上够用但未验证。 |
| 暂停/继续 | 依赖 Windows `NtSuspendProcess`，已在代码中做失败回退（提示改用取消），但未在自动化里覆盖点击路径。 |
| 网络路径 / 移动硬盘拔插 | 未覆盖。 |
| 多显示器 DPI 缩放 | 未在 125%/150% 缩放下逐项核对像素对齐。 |
| 拖放 / 托盘 / 剪贴板粘贴 | 属于交互路径，自动化截图模式未模拟鼠标事件，靠人工在开发过程中验证。 |

---

## 7. 复现方式

```powershell
# 1) 依赖（只有这一步需要下载 115MB；不装也能跑单元测试）
powershell -File scripts/get-ffmpeg.ps1
Copy-Item third_party\ffmpeg\bin\* build\ffmpeg\bin\ -Recurse   # 让程序能自找到 ffmpeg

# 2) 构建 + 单元测试（ctest 会跑 core 层与 job 层两个测试）
powershell -File scripts/build.ps1 -RunTests
ctest --test-dir build --output-on-failure

# 3) 生成测试素材
powershell -File scripts/make-test-media.ps1

# 4) GUI 端到端（真实转换 + 截图）
.\build\VideoForge.exe --screenshot shots --autoconvert testdata\演示视频-720p.mp4

# 5) 其他自测开关
.\build\VideoForge.exe --screenshot shots --theme light --window-size 1366x740 <文件>   # 小屏布局
.\build\VideoForge.exe --screenshot shots --preset mkv-remux --subtitles copy <文件>    # 字幕保留
.\build\VideoForge.exe --screenshot shots --select <子串> <若干文件>                     # 只转选中项
.\build\VideoForge.exe --install-ffmpeg <zip 的 http 地址>                              # 无界面安装 ffmpeg

# 6) 端到端 CTest 脚本（默认 DISABLED，需要真实 ffmpeg 与图形环境）
ctest --test-dir build -R vf_e2e --output-on-failure
```

### 当前 CI 状态

`.github/workflows/build.yml` 会在 `windows-latest` 上安装 Qt 6.9.3 + MinGW、
配置、编译、跑 `ctest`。**该工作流已写好并通过 YAML 语法校验，但尚未在 GitHub 上实际运行过**
（本地无法执行 GitHub Actions），首次推送后需要看一眼运行结果。
单元测试不依赖真实 ffmpeg，所以 CI 不需要下载 115MB 的二进制。
