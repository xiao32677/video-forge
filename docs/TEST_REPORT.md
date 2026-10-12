# VideoForge 测试报告

> 结论：**209 项单元断言全部通过；3 个端到端场景（批转视频 / HEVC / 音频提取）在真实
> ffmpeg 上全部跑通，产出 6 个输出文件并用 ffprobe 逐一校验；GUI 通过自动化截图模式
> 完成了真实点击路径的验证。过程中发现并修复 3 个真实缺陷（含 1 个必现崩溃）。**

---

## 1. 测试环境

| 项 | 值 |
|---|---|
| 操作系统 | Windows（x64） |
| 编译器 | MinGW-w64 GCC 13.1.0（x86_64-posix-seh-rev1） |
| Qt | 6.9.3 (mingw_64)，含 Multimedia / Svg / Concurrent |
| CMake / Ninja | 4.3.1 / 1.12.1 |
| ffmpeg | 9.0.2-essentials_build（gyan.dev），ffmpeg + ffprobe |
| 构建类型 | Release |

---

## 2. 测试策略

分三层，越往下越"真"：

| 层次 | 手段 | 覆盖什么 | 是否需要 GUI / ffmpeg |
|---|---|---|---|
| L1 单元测试 | `vf_tests.exe`（自研极简断言框架，链接 `videoforge_core`） | 格式化、JSON 解析、命令构建、进度解析、编码器识别、体积估算、路径生成 | 都不需要 |
| L2 端到端（脚本） | `tests/e2e_convert.cmake` | 合成视频 → 启动被测程序 → 真实转换 → ffprobe 校验输出 | 需要 |
| L3 GUI 自动化 | 程序内置 `--screenshot <dir> [--autoconvert]` | 真实加载、真实抽帧、真实转换、真实界面渲染 | 需要 |

**为什么把 `--screenshot` 做进程序里**：GUI 程序最难自动化。与其依赖外部的 UI 自动化框架
（不稳定、依赖注入），不如让程序自己提供一个"自测模式"：启动 → 载入指定文件 → 等缩略图
就绪 → `QWidget::grab()` 截图 → 触发一次真实转换 → 再截图 → 退出，并输出退出码。
这样每次改完代码都能用**一条命令**验证整条链路，而且产出的截图就是文档要用的图。

---

## 3. L1 单元测试结果

```
============================================
 VideoForge 单元测试 (core 层)
============================================
...
============================================
 共 209 项断言，失败 0 项
 结果：全部通过 ✔
============================================
```

覆盖明细：

| 模块 | 关键用例 |
|---|---|
| `fmt::*` | 体积（B/KB/MB 边界）、时长（<1h / >1h / 负数）、码率、帧率（29.97 保留两位、30 整数化）、宽高比（16:9 / 4:3 / 9:16 识别）、时间戳双向转换（`ffmpegTimestamp` ↔ `parseTimestamp`） |
| `MediaProbe::parseJson` | 真实 ffprobe JSON（含视频/音频/字幕/章节）字段完整性；`avg_frame_rate` 分数解析（30000/1001 → 29.97）；10bit 从 `yuv420p10le` 推断出 10；空 JSON / 坏 JSON / 无流文件都必须给出非空的人类可读错误 |
| `CommandBuilder::build` | 默认 MP4/H.264 参数齐全；`-progress pipe:1` 存在；无损换壳走 `copy`；裁剪的 `-ss/-t` 数值正确（40-10=30）；缩放与帧率过滤器字符串；音频-only 走 `-vn`；硬件加速映射到 `h264_nvenc` 且不再出现 `libx264`；预设不支持硬件时给出 warning 并回退；无视频流却要转视频时 `valid=false`；`-n`/`-y` 切换；引号转义（空格加引号、纯中文不加） |
| `defaultOutputPath` | 命名规则、目录回落、重名自动加 `(2)`（**绝不覆盖已有文件**） |
| `ProgressParser` | 完整进度块解析；**半行跨块拼接**（`frame=2` + `00\n` → 200）；`progress=end`；`kbits/s` 与 `Mbits/s` 换算；脏数据不崩 |
| `HwAccelDetector` | 从 `ffmpeg -encoders` 文本提取编码器；NVENC 可用 / QSV 可用 / AMF 不可用的能力表；无硬件时 `bestAccel()` 安全返回 `None` |
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
| A 批量转换（4 文件，深色，MP4/H.264） | `--screenshot shots --autoconvert <4个文件>` | **0** | 3 个视频成功，MP3 被正确拦下并给出原因 |
| B 浅色主题 + HEVC | `--theme light --preset mp4-h265` | **0** | 成功，且截图恰好捕捉到"转换中 55% · 3.25x · 总进度 58%" |
| C 音频提取 | `--preset mp3 <视频> <音频>` | **0** | 2 个 MP3 成功 |
| **D 便携包独立运行** | 解压 zip → 干净 PATH（仅 `system32`）下运行 | **0** | 3 张截图 + 2 个转换文件，证明**不依赖外部 Qt / MinGW / ffmpeg** |

### 4.3 输出文件校验（ffprobe 实测）

```
演示视频-720p_转换后.mp4      4.07 MB | 视频: h264,1280,720,30/1 | 音频: aac,48000,1 | 容器: mov,mp4,... | 12.000000s
演示视频-720p_转换后(2).mp4   3.52 MB | 视频: hevc,1280,720,30/1 | 音频: aac,48000,1 | 容器: mov,mp4,... | 12.000000s
演示视频2-540p_转换后.mp4     0.23 MB | 视频: h264,960,540,25/1  | 音频: aac,44100,1 | 容器: mov,mp4,... |  8.045714s
演示视频3-HEVC_转换后.mp4     0.12 MB | 视频: h264,640,360,24/1  | 音频: aac,48000,1 | 容器: mov,mp4,... |  6.000000s
演示视频-720p_转换后.mp3      0.28 MB | 视频: (无)              | 音频: mp3,48000,1 | 容器: mp3        | 12.000000s
音乐-正弦波_转换后.mp3        0.23 MB | 视频: (无)              | 音频: mp3,44100,1 | 容器: mp3        | 10.000000s
```

**校验要点**：分辨率/帧率/采样率与原文件一致（说明没有意外降采样）；
时长与源一致（说明没有截断/丢帧）；HEVC 预设确实产出 `hevc` 而不是回退成 h264；
音频预设产出的是 `mp3` 且**不含视频流**。

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

### 过程性教训

- `PowerShell` 启动 WIN32 GUI 程序**不会等待**，必须 `Start-Process -Wait`，
  否则子进程会被连同父任务一起干掉——第一次自测"只截到第一张图、没有转换产物"就是这个原因，
  一度被误判为程序 bug。
- 自测模式必须显式重置界面状态（本项目用环境变量 `VF_RESET_LAYOUT=1` 忽略上次保存的
  窗口布局），否则截图不可复现——QSettings 会记住上一次的窗口/分栏尺寸。

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
# 1) 依赖
powershell -File scripts/get-ffmpeg.ps1
Copy-Item third_party\ffmpeg\bin\* build\ffmpeg\bin\ -Recurse   # 让程序能自找到 ffmpeg

# 2) 构建 + 单元测试
powershell -File scripts/build.ps1 -RunTests

# 3) 生成测试素材
powershell -File scripts/make-test-media.ps1

# 4) GUI 端到端（真实转换 + 截图）
.\build\VideoForge.exe --screenshot shots --autoconvert testdata\演示视频-720p.mp4

# 5) 或用 CTest 跑端到端脚本
ctest --test-dir build -R vf_e2e --output-on-failure
```
