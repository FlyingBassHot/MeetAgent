# MeetAgent Terminal Firmware (ESP32-S3 / ESPUI)

> 模块负责人：`pi`  
> 依据：`docs/v2.1/modules/terminal.md`、`docs/v2.1/PROTOCOL.md`、`TerminalUi.md`、`MeetAgentV2.1.md`  
> 目录边界：本工程只修改 `firmware/terminal/`，不涉及 Node / 协议 / Web / 移动端。

## 当前状态

已完成 E0 / E1 / E2 的最小框架，以及 E3a 的纯逻辑 ASCII Renderer：

- E0：ESPUI 分层、Theme API、Manifest、ViewModel、Scene Manager、HAL 边界、工程骨架
- E1：ESP-IDF 工程骨架、LVGL 渲染器接入点、HAL 接口、UI Task + 事件队列、Theme Registry、安全主题
- E2：Manifest 校验、Design Tokens、Scene Manager、Motion Scheduler、两个内置主题、主题切换与回退
- E3a：字符网格、glyph ramp 密度场、ASCII Geometry、六态 Motion、Completed 有限脉冲、Scheduler/Scene 绑定、host CLI
- E4a：Wi-Fi NVS 配网、SoftAP portal、STA/断线重连、Node `/api/v2/info` 可达性
- E4b-1：真实 Node WebSocket、pairing_code→token、welcome/snapshot、heartbeat、自动重连
- E4b-2：真实事件 apply、sequence 去重、replay 重建、4Hz ViewModel 节流、active-first session 截断
- E4b-3：command gateway、pending command 表、15s 超时、断线失败、session.list 安全刷新、action result → footer
- PHOSPHOR P0/P1/P1.1/P1.2/P1.3/P2/P3/P3.1/P3.2：审计、24×16 Glyph Frame、三轨 Orbit、2× Nucleus、18×27 Bold Atlas、密度 Ramp、Palette/Compositor、Idle/Working motion、0/1 二进制流、Outer 内侧副带、SDL2 动画/截图、ESP32 432 canvas overlay

已验证：

- **真机 bring-up 成功（2026-11-11）**：ESP-IDF v5.5.5 构建、烧录、启动通过；180° 显示旋转已修正并重新烧录
- 启动证据：16MB QIO Flash、8MB Octal PSRAM 80MHz、240MHz、ST7701 480×480、GT911 0x14、ESPUI 初始化
- 固件：`meetagent_terminal.bin` 1,273,152 bytes（SHA-256 `fec42554...e6573`）；app 分区剩余 17%
- 原厂 16MB Flash 已备份：`/home/perch/esp/backups/lcd4-factory-2026-11-11-1132.bin`
- 主机测试：`3620 checks, 0 failure(s)`（ASan/UBSan + `-Werror`）
- 主机语法检查：45 个固件 C 文件通过（board_lcd4.c / wifi_prov.c / net_client.c / phosphor_lvgl_port.c 由真实 IDF 构建验证）
- E3a 帧预览：`make preview STATE=working FRAMES=3 DELAY=300`
- 启动日志：`/tmp/opencode/esp-lcd4-boot.log`
- E4a 实测：STA 172.20.10.2，Node developer-machine/2.1.0，`/status` node_reachable=true
- E4b-1 实测：pairing→token→snapshot、token 重启、Node 重启自动重连、heartbeat RTT 59/53ms
- E4b-2 实测：真实事件 seq 53–59 apply、agent/session 状态与 completion 计数、Node 重启 replay 后 seq 46–52 继续 apply
- E4b-3 实测：session.list completed、sessions=42、action result OK；最终回归 event seq 219–225 apply
- GT911 触摸已实机确认：TL/TR/BL/BR/C 五点 + 180° 映射；临时 Touch Test overlay 已移除
- **待人工确认**：屏幕颜色/背光亮度；AP portal `/save` 需第二台设备
- 注意：正式固件仍不轮询 touch HAL，Mock UI 暂无可点击控件

当前环境：

```text
ESP-IDF  : v5.5.5 (/home/perch/esp/esp-idf-5.5.5)
目标     : esp32s3
工具链   : xtensa-esp32s3-elf
LVGL     : 8.4.0（Component Manager）
BSP      : waveshare/esp32_s3_touch_lcd_4 3.0.0
WS       : espressif/esp_websocket_client 1.8.0
构建     : idf.py build 成功
烧录     : idf.py -p /dev/ttyACM0 flash 成功
启动     : 成功，无 panic/assert/reset loop
Wi-Fi    : E4a 配网/STA/Node 可达已验证
串口     : /dev/ttyACM0 (28:84:85:ac:74:6c)
```

## 目标硬件

- Waveshare ESP32-S3-Touch-LCD-4 V4.0（近期购买确认最新版）
- 4.0 英寸 480 × 480 RGB565 方形屏
- GT911 五点电容触摸 + CH32V003F4U6 helper（背光/复位/电源/电池）
- ESP32-S3N16R8：16MB Flash + 8MB Octal PSRAM
- ESP32-S3 + ESP-IDF 5.x（当前 5.4.4）+ FreeRTOS + LVGL 8.x（当前 8.4.0）
- 官方 BSP：`waveshare/esp32_s3_touch_lcd_4 ^3.0.0`
- 触摸为主，板载 BOOT / RESET / BAT_PWR 按键辅助；无板载旋钮

**没有在本地写死 GPIO。** 真实板级适配 `main/board_lcd4.c` 直接调用官方 BSP：
`bsp_display_start()` / `bsp_display_lock/unlock` / `bsp_display_brightness_set()` /
`bsp_display_get_input_dev()`。BSP 负责 ST7701、GT911、CH32V003 复位/背光/LVGL port；
`espui_board_stub()` 仅保留给 host tests。

## 构建

需要 ESP-IDF 5.x、Python 环境、LVGL 组件（由 `idf_component.yml` 通过 Component Manager 拉取）。

```bash
source "$HOME/esp/esp-idf/export.sh"
cd firmware/terminal
idf.py set-target esp32s3
idf.py build
idf.py size
# 硬件到达后再执行：
# idf.py -p /dev/ttyACM0 flash monitor
```

本次已用 ESP-IDF v5.5.5 完成 LCD-4 BSP 真机构建/烧录/启动。Component Manager
解析到 BSP 3.0.0、LVGL 8.4.0、esp_lvgl_port 2.9.0；LVGL 固定为 `8.4.*`，
不启用 `lv_demo_*` 或额外字体。`sdkconfig`、`dependencies.lock`、
`managed_components/` 和 `build/` 均由 `.gitignore` 忽略。

烧录前已完整备份原厂 16MB Flash 到 `/home/perch/esp/backups/`。首次 `idf.py flash`
因 sudo 的 `HOME=/root` 找不到 Python 环境而失败（未接触设备），修正 `HOME` /
`IDF_TOOLS_PATH` / `IDF_PYTHON_ENV_PATH` 后一次成功。

> 已烧录并启动；触摸/180° 已确认，屏幕颜色/背光仍需人工确认。详细证据、固件 hash、
> 启动日志与风险见 `docs/BUILD_STATUS.md` / `docs/HARDWARE.md`。

## 主机测试

不需要 ESP-IDF，只依赖 gcc + make：

```bash
cd firmware/terminal/tests/host
make test      # 纯逻辑单元测试（ASan/UBSan）
make preview STATE=working FRAMES=3 DELAY=300  # 打印 ASCII 帧
make syntax    # 所有固件 C 文件的 ESP-IDF stub 语法检查
make clean
```

覆盖：

- Manifest 校验与显示兼容
- Tokens 颜色解析与默认值
- ViewModel 固定容量、增删改、Attention 计数、业务态与设备态分离
- Scene Manager 栈、Attention 优先级、有界历史
- Theme Registry / Renderer Registry / Theme Manager
- 主题切换不丢 ViewModel、非法主题回退安全主题、初始化失败回退
- Input Router、Navigation、Action Dispatcher
- Motion Scheduler 调速、暂停、追帧上限
- Asset Registry、Memory Budget
- Protocol 状态映射、Snapshot/Event → ViewModel、sequence 去重、事件计数
- ASCII Renderer：glyph ramp、密度场、几何、六态解析、Motion 缩放、Completed 脉冲、Offline 静止
- Animation Scheduler 绑定、Scene Manager → Attention 状态联动
- 多帧确定性快照、Canvas 裁剪与文本/几何分层

## 目录

```text
firmware/terminal/
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   ├── app_main.c
│   ├── board_lcd4.c        # 官方 LCD-4 BSP → ESPUI HAL
│   └── board_lcd4.h
├── components/
│   ├── espui/
│   │   ├── theme_api/      # Theme API / Manifest / Tokens / Renderer Registry / 公共类型
│   │   ├── core/           # ViewModel / Scene 导航 / Input Router / Action Dispatcher / UI Task
│   │   ├── runtime/        # Theme Manager / Scene Manager / Animation / Assets / Memory / LVGL Renderer
│   │   ├── themes/
│   │   │   ├── safe/       # 安全回退主题
│   │   │   └── terminal_aesthetic/
│   │   │       ├── terminal_theme.c         # E2 Mock（E3b 接 LVGL 字符网格）
│   │   │       └── renderers/               # E3a 纯逻辑 ASCII Renderer + Canvas
│   │   └── hal/            # Display / Touch / Input / Power / Board 接口与 headless stub
│   ├── protocol/           # V2.1 协议 C 类型 + JSON stub
│   ├── state/              # Snapshot → ViewModel 适配器 + Mock 状态源
│   └── networking/         # Wi-Fi / WSS / Command table + Node 客户端
├── tests/host/             # 主机测试与语法检查
└── docs/
```

## 文档

- `docs/ARCHITECTURE.md`：分层、数据流、线程模型、边界与切换流程
- `docs/THEME_API.md`：Theme API / Manifest / Tokens / Renderer 规范
- `docs/VIEWMODEL.md`：ViewModel 维度、容量、所有权与状态适配
- `docs/HARDWARE.md`：HAL 契约、stub 板、待确认硬件项
- `docs/ASCII_RENDERER.md`：E3a 字符网格、几何、六态 Motion、CLI 与集成
- `docs/NETWORK.md`：E4a Wi-Fi 配网、E4b-1 WebSocket、E4b-2 事件 apply/replay、E4b-3 命令/审批、NVS 清理、portal 验证
- `docs/PHOSPHOR_AUDIT.md`：P0 依赖/屏幕/内存/集成审计
- `docs/PHOSPHOR_P1.md`：P1/P1.1/P1.2/P1.3 静态 Core、18×27 Bold、三轨可辨性、模拟器、截图、真机结果
- `docs/PHOSPHOR_P2.md`：P2 Glyph Ramp、核心密度场、Palette、前后合成、模拟器/真机
- `docs/PHOSPHOR_P3.md`：P3/P3.1/P3.2 Idle/Working motion、0/1 二进制流、外圈副带、FPS/内存、SDL golden
- `docs/BUILD_STATUS.md`：构建/测试结果、阻塞与下一步

## 未实现（明确不在本次范围）

- E3b：把 ASCII 帧接到 LVGL 字符网格 widget、字体/颜色 Token 映射、局部刷新
- E3 剩余：Focus / Activity / Settings 完整场景布局、文本打字/滚动动效、真机 FPS/内存验收
- E4b-4：interrupt/permission 真机压力、无效 token/pairing 重试、sequence gap 补偿
- E5：主题设置持久化、亮度/夜间模式、低功耗、OTA
- 语音 / STT、本地 AI、完整聊天、文件浏览、长历史
- 生产 UI 触摸交互 / 旋钮驱动 / 显示颜色校准

## 设计约束

- Theme 不访问 Node / 网络 / 协议 JSON；只消费 ViewModel。
- Theme 不直接调用 LVGL；只通过 Renderer API。
- 网络/状态任务不操作 LVGL；只投递事件到 UI 队列。
- UI Core 只处理抽象 Intent，不直接执行 Agent 命令。
- 所有 UI 数据使用固定上限，不保存无界历史。
- 主题加载失败必须回退安全主题；ViewModel 不因切换主题而丢失。
