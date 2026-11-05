# 构建与验证状态（E0 / E1 / E2 / E3a）

日期：2026-11-05  
范围：`firmware/terminal/` only

## 1. 环境检查

```text
gcc       : /usr/bin/gcc
make      : /usr/bin/make
cmake     : missing
ninja     : missing
idf.py    : missing
IDF_PATH  : <empty>
```

结论：**无法执行 ESP-IDF 配置、编译、链接和烧录**。本模块没有自行安装 ESP-IDF。

## 2. 已完成的验证

### 2.1 主机单元测试

```bash
cd firmware/terminal/tests/host
make test
```

结果：

```text
MeetAgent Terminal host tests
[ RUN ] test_ascii_renderer_run
[ RUN ] test_builtin_themes_run
[ RUN ] test_manifest_run
[ RUN ] test_motion_profiles_run
[ RUN ] test_runtime_helpers_run
[ RUN ] test_scene_manager_run
[ RUN ] test_state_adapter_run
[ RUN ] test_theme_manager_run
[ RUN ] test_tokens_run
[ RUN ] test_view_model_run

805 checks, 0 failure(s)
```

编译选项：`-std=c11 -Wall -Wextra -Werror -O1 -g -fsanitize=address,undefined`。

纯逻辑测试覆盖 Manifest、Tokens、ViewModel、Scene、Theme Registry/Manager、Input/Navigation、Action、Motion、Asset/Memory、Protocol/State Adapter，以及 E3a ASCII 网格/密度场/几何/六态/脉冲/Scheduler/Scene 集成。

### 2.2 ASCII 帧预览

```bash
cd firmware/terminal/tests/host
make preview STATE=working FRAMES=3 DELAY=300
make preview STATE=all FRAMES=1
```

输出包含状态、时间、speed/density/amplitude、frame 序号和字符帧。示例见
`docs/ASCII_RENDERER.md`。

### 2.3 全固件 C 文件语法检查

```bash
cd firmware/terminal/tests/host
make syntax
```

结果：

```text
[syntax] OK: 33 file(s)
```

使用主机 GCC + `tests/host/stubs/`（esp_err/esp_log/esp_timer/FreeRTOS/LVGL 声明）对所有组件与 `main/app_main.c` 做 `-fsyntax-only -Werror`。

这只能证明 C 语法、类型和头文件依赖自洽；**不能替代 `idf.py build`**，不能证明：

- ESP-IDF 组件依赖解析正确
- LVGL 8.3 实际 API 链接正确
- 真实 BSP 的 LVGL port 正确
- 二进制大小、内存、任务栈、显示效果
- 真机运行

## 3. 阻塞项

| 阻塞 | 影响 | 处理建议 |
|---|---|---|
| 未安装 ESP-IDF（无 `idf.py`/CMake/Ninja） | 无法编译固件 | 由主会话统一安装/指定 IDF 环境 |
| LVGL 未获取 | 无法链接 `builtin.label` renderer | ESP-IDF Component Manager 联网拉取或预置组件 |
| 无真实 BSP/引脚确认 | 只能 headless stub，不能点亮屏幕/触摸/旋钮 | 依据 Waveshare 原理图实现 HAL |
| 无目标板 | 无法验收显示、动画、输入 | 主会话安排真机或由负责人提供 BSP |

## 4. 已实现清单

- [x] ESP-IDF 工程骨架：`CMakeLists.txt`、`sdkconfig.defaults`、`main/`、`components/`
- [x] `components/espui` 分层目录
- [x] Theme API（生命周期、上下文访问器）
- [x] Theme Manifest（C 常量 + JSON 镜像 + 校验）
- [x] Design Tokens（安全/终端两套 + 颜色解析）
- [x] ViewModel（固定容量、业务态/设备态分离、revision）
- [x] Scene Manager（栈、Attention 优先级、有界历史）
- [x] Input Router / Navigation / Action Dispatcher
- [x] Animation Scheduler（调速、暂停、追帧上限）
- [x] Asset Registry / Memory Budget
- [x] Theme Registry / Renderer Registry / Theme Manager
- [x] 主题切换不丢 ViewModel
- [x] 非法主题/初始化失败回退安全主题
- [x] 两个内置主题：`safe_fallback`、`terminal_aesthetic` mock
- [x] LVGL 8.3 Label/Rect Renderer（编译期检查，未链接真 LVGL）
- [x] HAL 接口 + headless stub board（320×480）
- [x] UI Task + 事件队列 + 线程边界
- [x] Protocol V2.1 C 类型 + Snapshot 校验 + JSON stub
- [x] State Adapter（Snapshot → ViewModel）+ Mock 状态任务
- [x] Networking/Command 接口 + stub
- [x] E3a 纯逻辑 ASCII Renderer：字符网格、glyph ramp 密度场
- [x] ASCII Geometry：核心环、轨道、线框、扫描线、辐条、粒子
- [x] 六态视觉状态与 Motion（speed/density/amplitude）映射
- [x] Completed 有限脉冲、Offline 静止
- [x] Animation Scheduler 绑定、Scene Manager → Attention 联动
- [x] Host ASCII CLI 与多帧/确定性测试
- [x] 主机测试与语法检查脚本
- [x] 架构、Theme API、ViewModel、硬件、ASCII Renderer、构建文档

## 5. 未实现清单

- [ ] E3b：ASCII 帧 → LVGL 字符网格 widget、字体/颜色 Token 映射、局部刷新
- [ ] E3 剩余：Focus / Activity / Settings 完整场景布局、打字/滚动动效、真机 FPS/内存验收
- [ ] 字体/图标资产与 LVGL 字体接入
- [ ] E4 Wi-Fi 配网、mDNS、HTTP 配对、WSS、hello/welcome/snapshot/event/command/heartbeat/重连
- [ ] Protocol JSON 解析/序列化（当前返回 `ESP_ERR_NOT_SUPPORTED`）
- [ ] Event 增量应用、Sequence 去重/补偿
- [ ] Settings Storage、亮度/夜间模式、主题持久化
- [ ] 真实 LCD/触摸/旋钮/电源 BSP
- [ ] 真机性能、内存、动画 FPS 验收（E3a 仅主机逻辑）
- [ ] OTA / Cloud Relay / 语音

## 6. 安装 ESP-IDF 后的验证顺序

```bash
cd firmware/terminal
idf.py set-target esp32s3
idf.py build
```

建议先处理：

1. Component Manager 是否成功获取 LVGL 8.3。
2. `main` 组件能否链接 `espui` / `state` / `protocol` / `networking`。
3. `sdkconfig.defaults` 的 PSRAM/Flash 配置是否与实物一致。
4. 串口日志是否出现 `ESPUI initialized: theme=terminal_aesthetic ...`。
5. 切换 initial theme 为 `safe_fallback`，确认保底主题可启动。
6. 接入真实 BSP 后再验证屏幕方向、颜色、触摸坐标。
7. E3b 将 `espui_ascii_frame_t` 映射到 LVGL 字符网格，再测 FPS/局部刷新。

## 7. 仓库约束

- 本次只修改 `firmware/terminal/`。
- 未修改 Node / Go / 协议 / `configs/` / `web/` / `test-client/` / 根文档。
- 未执行 `git commit` / `git push`。
