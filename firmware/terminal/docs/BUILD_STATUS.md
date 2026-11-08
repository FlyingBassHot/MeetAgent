# 构建与验证状态（E0 / E1 / E2 / E3a）

日期：2026-11-08  
范围：`firmware/terminal/` only  
目标：ESP32-S3（Waveshare ESP32-S3-Touch-LCD-3.5B）  
构建方式：真实 ESP-IDF 编译，**未烧录、未接串口**

## 1. 真实固件构建

### 1.1 环境

```text
ESP-IDF : v5.4.4 (/home/perch/esp/esp-idf)
目标    : esp32s3
工具链  : xtensa-esp32s3-elf
CMake   : ESP-IDF 自带
Ninja   : ESP-IDF 自带
LVGL    : 8.4.0（Component Manager，由 idf_component.yml 的 ^8.3.0 解析）
```

执行：

```bash
source "$HOME/esp/esp-idf/export.sh"
cd /home/perch/Work/MeetAgent/firmware/terminal
idf.py set-target esp32s3
idf.py build
idf.py size
```

`set-target` 结果：

- 生成 `sdkconfig`（按 `sdkconfig.defaults` 的 16MB flash / Octal PSRAM 配置）
- 生成 `dependencies.lock`
- Component Manager 拉取 `lvgl/lvgl 8.4.0`
- 目标 `esp32s3` 配置成功

### 1.2 构建结果

```text
Project build complete.
```

- `idf.py build`：**成功**，exit 0
- 构建日志中的 `warning:` / `error:` / `undefined reference`：**0 条**
- 未因编译/链接错误修改源码、CMake 或 sdkconfig.defaults
- 未烧录、未连接串口

### 1.3 固件大小

```text
build/meetagent_terminal.bin       253,152 bytes (0x3DCE0)
build/bootloader/bootloader.bin     20,928 bytes (0x51C0)
build/partition_table/partition-table.bin 3,072 bytes
```

`idf.py size`：

```text
Total image size: 253,034 bytes
Flash Code:       130,072 bytes
Flash Data:        56,140 bytes
DIRAM:             60,715 / 341,760 bytes (17.77%)
IRAM (size tool):  16,383 / 16,384 bytes (99.99%)
```

App 分区：

```text
Smallest app partition: 0x177000 bytes
Free:                   0x139320 bytes (84%)
```

说明：

- `idf.py size` 表里的 IRAM 行 total 为 16,384，与链接脚本
  `iram0_0_seg = 0x57700` 的段长度口径不一致；实际链接的 IRAM 断言通过，
  构建未报溢出。该行只作为 size 工具输出记录，真机内存仍需后续确认。
- `sdkconfig.defaults` 中的 16MB / Octal PSRAM 仍是待实物确认配置，本次未修改。

## 2. 主机单元测试

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

纯逻辑测试覆盖 Manifest、Tokens、ViewModel、Scene、Theme Registry/Manager、
Input/Navigation、Action、Motion、Asset/Memory、Protocol/State Adapter，以及
E3a ASCII 网格/密度场/几何/六态/脉冲/Scheduler/Scene 集成。

## 3. ASCII 帧预览

```bash
cd firmware/terminal/tests/host
make preview STATE=working FRAMES=3 DELAY=300
make preview STATE=all FRAMES=1
```

输出包含状态、时间、speed/density/amplitude、frame 序号和字符帧。示例见
`docs/ASCII_RENDERER.md`。

## 4. 主机语法检查

```bash
cd firmware/terminal/tests/host
make syntax
```

结果：

```text
[syntax] OK: 33 file(s)
```

使用主机 GCC + `tests/host/stubs/`（esp_err/esp_log/esp_timer/FreeRTOS/LVGL 声明）
对所有组件与 `main/app_main.c` 做 `-fsyntax-only -Werror`。

## 5. 本次构建修复

**无。** 首次 `idf.py build` 即通过，未修改源码/CMake/组件依赖。

生成但被忽略的构建产物：

```text
build/
sdkconfig
sdkconfig.old
dependencies.lock
managed_components/
```

这些均被 `firmware/terminal/.gitignore` 忽略，不进入提交。

## 6. 未验证项 / 风险

- [ ] 未烧录、未接串口，未验证 bootloader / app 启动日志
- [ ] 未验证真实 LCD、触摸、背光、旋钮/按键（BSP 仍是 headless stub）
- [ ] 未验证 16MB flash / Octal PSRAM 与实物一致
- [ ] LVGL 实际分辨率 8.4.0 的运行时行为（本次只验证编译/链接）
- [ ] E3b：ASCII 帧 → LVGL 字符网格、字体/颜色 Token 映射、局部刷新
- [ ] E4：Wi-Fi / mDNS / 配对 / WSS / 协议 JSON / Event 增量
- [ ] 真机 FPS、CPU、内存、长时间运行
- [ ] `idf.py size` 的 IRAM 行口径需后续核对（构建未报溢出）

## 7. 已实现清单

- [x] ESP-IDF 工程骨架、`CMakeLists.txt`、`sdkconfig.defaults`
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
- [x] LVGL 8.x Label/Rect Renderer
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
- [x] 真实 ESP-IDF v5.4.4 构建通过
- [x] 架构、Theme API、ViewModel、硬件、ASCII Renderer、构建文档

## 8. 未实现清单

- [ ] E3b：ASCII 帧 → LVGL 字符网格 widget、字体/颜色 Token 映射、局部刷新
- [ ] E3 剩余：Focus / Activity / Settings 完整场景布局、打字/滚动动效
- [ ] 字体/图标资产与 LVGL 字体接入
- [ ] E4 Wi-Fi 配网、mDNS、HTTP 配对、WSS、hello/welcome/snapshot/event/command/heartbeat/重连
- [ ] Protocol JSON 解析/序列化（当前返回 `ESP_ERR_NOT_SUPPORTED`）
- [ ] Event 增量应用、Sequence 去重/补偿
- [ ] Settings Storage、亮度/夜间模式、主题持久化
- [ ] 真实 LCD/触摸/旋钮/电源 BSP
- [ ] 真机性能、内存、动画 FPS 验收
- [ ] OTA / Cloud Relay / 语音

## 9. 下一步

1. 硬件到达后先烧录当前固件，确认 bootloader/app 启动、PSRAM/Flash 初始化。
2. 依据 Waveshare 原理图实现真实 BSP，替换 `stub-headless`。
3. 先验证 `safe_fallback`，再切 `terminal_aesthetic` mock。
4. E3b 将 ASCII 帧映射到 LVGL 字符网格，测局部刷新/FPS/内存。
5. E4 接真实 Node。

## 10. 仓库约束

- 本次只修改 `firmware/terminal/`。
- 未修改 Node / Go / 协议 / `configs/` / `web/` / `test-client/` / 根文档。
- 未执行 `git commit` / `git push`。
- 未烧录、未连接串口。
