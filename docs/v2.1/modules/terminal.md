# ESP32 Terminal / ESPUI 模块任务书（负责人：pi）

> 依据：`MeetAgentV2.1.md`、`TerminalUi.md`、`docs/v2.1/EXECUTION.md`、`docs/v2.1/PROTOCOL.md`
> 目录：只允许修改 `firmware/terminal/`
> 不要修改 Node/Go/协议/configs/web/test-client/根文档；不要 commit/push。

## 目标

建立 MeetAgent Terminal 固件工程与 ESPUI 基础设施。首版是**局域网常驻终端**，不是手机缩小版：

- 低信息密度、快速感知
- 旋钮/按键为主，触摸为辅助
- 状态、活跃 Session、关键事件、Interrupt、审批摘要
- 不实现完整 Prompt 聊天、文件浏览、长历史、本地 AI

目标硬件：**Waveshare ESP32-S3-Touch-LCD-4 V4**（近期购买确认最新版）。

- 4.0 英寸 480 × 480 RGB565 电容触摸屏
- GT911 五点触控，支持中断
- ESP32-S3N16R8：16MB Flash + 8MB Octal PSRAM
- RGB 并口 LCD
- V4 使用 CH32V003F4U6 helper（I2C）控制背光、LCD/触摸复位、蜂鸣器和电源/电池 ADC；早期版本为 TCA9554
- 输入以触摸为主，板载 BOOT / RESET / BAT_PWR 按键辅助；无板载旋钮
- ESP-IDF >= 5.2.5（当前 5.4.4），FreeRTOS，LVGL 8.x
- 官方 BSP：`waveshare/esp32_s3_touch_lcd_4 ^3.0.0`（V4）

## 当前环境与进度

- ESP-IDF v5.4.4 与 esp32s3 工具链已安装，`idf.py build` 已通过（当前仍是 320×480 headless stub）
- E0/E1/E2 + E3a 已完成；host tests 805/805，syntax 33/33
- 尚未接真实 LCD/触摸/背光；480×480 布局与 V4 BSP 待实施
- 实物为 V4.0；旧版 V1 的 I2C/TCA9554 映射不适用
- 不要假设 GPIO/引脚；以 V4 原理图、官方 `06_lvgl_demo_v8` 和 BSP 组件为准

## 开发顺序：BSP First

1. 优先接入官方 BSP `waveshare/esp32_s3_touch_lcd_4 ^3.0.0`，确认其 LVGL 8.4 / ESP-IDF 5.4.4 兼容性。
2. 用 BSP 完成 LCD 点亮、颜色/方向、GT911 触摸坐标、CH32V003 背光/复位。
3. 将 BSP 封装进现有 `espui_board_hal_t`，保持 Theme/UI Core 不变。
4. 将布局从 320×480 竖屏适配为 480×480 方形。
5. 最后接 Wi-Fi、mDNS、配对、WSS 和 Client Protocol（E4）。

## 第一阶段（本次交付）

### E0：ESPUI 架构定义

- `docs/` 或 `README.md` 记录分层、Theme API、Manifest、ViewModel、Scene Manager
- 目录骨架：

```text
firmware/terminal/
├── CMakeLists.txt
├── sdkconfig.defaults
├── main/
│   └── app_main.c
├── components/
│   ├── espui/
│   │   ├── core/          # ViewModel / navigation / input router / action dispatcher
│   │   ├── runtime/       # theme manager / scene manager / animation scheduler / assets
│   │   ├── theme_api/     # espui_theme.h / manifest / scene / tokens
│   │   ├── themes/terminal_aesthetic/
│   │   └── hal/           # display / touch / input / power 接口
│   ├── protocol/          # V2.1 client protocol 类型与解析（先 stub）
│   ├── state/             # ViewModel / state adapter
│   └── networking/        # wifi / http / websocket（先 stub）
└── tests/
```

### E1：基础 UI Runtime

- ESP-IDF 工程初始化、LVGL 接入点
- ESP32-S3-Touch-LCD-4 V4 板级 BSP：优先官方组件；RGB LCD、GT911、CH32V003 背光/复位
- HAL 接口：display/touch/encoder/button/power（真实 BSP + headless stub）
- UI Task + 事件队列（网络回调不能直接操作 LVGL）
- Theme Registry + 内置安全主题
- 一个可运行的 Mock ViewModel + 简单状态页面

### E2：主题框架（最小）

- Theme Manifest 结构、Design Tokens、Scene Manager、Motion Profile 类型
- 至少两个 Mock Theme 注册/切换（可用编译期注册）
- 主题加载失败回退安全主题
- 切换主题不丢 ViewModel

首版框架已完成，E3a 纯逻辑 ASCII Renderer 也已补齐；下一阶段是在 ESP32-S3-Touch-LCD-4 上完成真实 BSP、480×480 布局和 E3b 字符网格。

## 后续（等主会话下发）

- E3b：480×480 方形布局、ASCII 帧 → LVGL 字符网格、字体/颜色 Token、局部刷新
- E3 Terminal Aesthetic：六态动画、Ambient/Focus/Activity/Attention
- E4 接真实 Node：Snapshot/Event/Command
- E5 配置、亮度、夜间模式、长时间运行

## 验收/汇报要求

1. 先报告实物版本与屏幕方向；未确认前不写死 GPIO。
2. BSP 阶段汇报：LCD 点亮结果、颜色/方向、GT911 触摸坐标、背光、串口日志。
3. 480×480 适配阶段汇报：截图/照片、主机测试、`idf.py build`、固件大小。
4. 未烧录或未验证的项必须明确标注。

## 协议

- 阅读 `docs/v2.1/PROTOCOL.md` 与 `pkg/clientproto/protocol.go`
- Terminal 首版只连 LAN；`ws://<node>:8787/ws`
- 客户端类型 `terminal`；capabilities：`ui.compact`、`display.touch`、`input.button`、`input.encoder`、`notification.local`
- 后续接 Node 时实现 hello/welcome/snapshot/event/command/command.result/heartbeat

## 验收与汇报

完成 E0/E1/E2 最小框架后停下汇报：

1. 目录树 + 关键头文件/源文件
2. 架构说明：Theme API、ViewModel、Scene、HAL 边界
3. 是否安装 ESP-IDF/能否编译；不能编译的具体阻塞
4. 已实现/未实现清单
5. 引脚/硬件假设（如有）
6. 下一步建议

不要自行开始 E3 动画或真实 Node 联调。
