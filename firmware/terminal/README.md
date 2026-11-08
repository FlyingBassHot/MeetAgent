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

已验证：

- **真实 ESP-IDF 构建**：ESP-IDF v5.4.4 + esp32s3 工具链 + LVGL 8.4.0，`idf.py build` 成功
- 固件：`meetagent_terminal.bin` 502,272 bytes；app 分区剩余 67%
- 主机测试：`807 checks, 0 failure(s)`（ASan/UBSan + `-Werror`）
- 主机语法检查：33 个固件 C 文件通过（board_lcd4.c 由真实 IDF 构建验证）
- E3a 帧预览：`make preview STATE=working FRAMES=3 DELAY=300`
- **未验证**：未烧录、未接串口；未验证真实 LCD/触摸/旋钮/PSRAM

当前环境：

```text
ESP-IDF  : v5.4.4 (/home/perch/esp/esp-idf)
目标     : esp32s3
工具链   : xtensa-esp32s3-elf
LVGL     : 8.4.0（Component Manager）
BSP      : waveshare/esp32_s3_touch_lcd_4 3.0.0
构建     : idf.py build 成功
烧录     : 未执行（硬件未到）
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

本次已用 ESP-IDF v5.4.4 完成 LCD-4 BSP 真实构建。Component Manager 解析到
BSP 3.0.0、LVGL 8.4.0、esp_lvgl_port 2.9.0；LVGL 固定为 `8.4.*`，不启用
`lv_demo_*` 或额外字体。`sdkconfig`、`dependencies.lock`、`managed_components/`
和 `build/` 均由 `.gitignore` 忽略。

> 未烧录、未接串口。构建大小、内存与限制详见 `docs/BUILD_STATUS.md`。

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
- Protocol 状态映射、Snapshot → ViewModel、事件接口 stub
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
│   └── networking/         # Wi-Fi / WSS / Command 客户端接口 + stub
├── tests/host/             # 主机测试与语法检查
└── docs/
```

## 文档

- `docs/ARCHITECTURE.md`：分层、数据流、线程模型、边界与切换流程
- `docs/THEME_API.md`：Theme API / Manifest / Tokens / Renderer 规范
- `docs/VIEWMODEL.md`：ViewModel 维度、容量、所有权与状态适配
- `docs/HARDWARE.md`：HAL 契约、stub 板、待确认硬件项
- `docs/ASCII_RENDERER.md`：E3a 字符网格、几何、六态 Motion、CLI 与集成
- `docs/BUILD_STATUS.md`：本次构建/测试结果、阻塞与下一步

## 未实现（明确不在本次范围）

- E3b：把 ASCII 帧接到 LVGL 字符网格 widget、字体/颜色 Token 映射、局部刷新
- E3 剩余：Focus / Activity / Settings 完整场景布局、文本打字/滚动动效、真机 FPS/内存验收
- E4 真实 Node：Wi-Fi 配网、mDNS、HTTP 配对、WSS、hello/welcome/snapshot/event/command、心跳与重连
- E5：主题设置持久化、亮度/夜间模式、低功耗、OTA
- 语音 / STT、本地 AI、完整聊天、文件浏览、长历史
- 真机 LCD / 触摸 / 旋钮驱动与引脚定义

## 设计约束

- Theme 不访问 Node / 网络 / 协议 JSON；只消费 ViewModel。
- Theme 不直接调用 LVGL；只通过 Renderer API。
- 网络/状态任务不操作 LVGL；只投递事件到 UI 队列。
- UI Core 只处理抽象 Intent，不直接执行 Agent 命令。
- 所有 UI 数据使用固定上限，不保存无界历史。
- 主题加载失败必须回退安全主题；ViewModel 不因切换主题而丢失。
