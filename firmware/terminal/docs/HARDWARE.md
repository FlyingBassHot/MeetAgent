# 硬件与 HAL（LCD-4 V4 / E0–E3a）

## 1. 目标硬件

| 项 | 值 | 状态 |
|---|---|---|
| 开发板 | Waveshare ESP32-S3-Touch-LCD-4 **V4.0** | 用户确认 |
| 屏幕 | 4.0" 480×480 RGB565（ST7701） | 按 BSP/官方示例 |
| 触摸 | GT911，电容，硬件支持 5 点 | 按 BSP/官方示例 |
| 模组 | ESP32-S3N16R8：16MB Flash + 8MB Octal PSRAM | 按用户确认 |
| helper | CH32V003F4U6，I2C（背光/复位/电源/电池） | 按 BSP/官方示例 |
| 按键 | BOOT / RESET / BAT_PWR（无板载旋钮） | 按 BSP/官方示例 |
| BSP | `waveshare/esp32_s3_touch_lcd_4 ^3.0.0`（实际 3.0.0） | 已接入 |
| LVGL | 8.4.0（`8.4.*` 固定） | 已构建 |
| ESP-IDF | v5.5.5（5.4.4 保留） | 已构建并启动 |

## 2. 官方 BSP API 结论

已阅读 `managed_components/waveshare__esp32_s3_touch_lcd_4` 的
`bsp/esp32_s3_touch_lcd_4.h`、`bsp/display.h`、`bsp/touch.h` 与
官方 `examples/esp-idf/06_lvgl_demo_v8`。

| BSP API | ESPUI 映射 | 结论 |
|---|---|---|
| `bsp_display_start()` | `display.init` | 初始化 CH32V003 IO expander、ST7701 RGB 面板、GT911、esp_lvgl_port task，返回 LVGL display |
| `bsp_display_lock(timeout)` / `unlock` | display HAL `lock/unlock` | 运行时在 Theme/Renderer 调用前后加锁，与 BSP LVGL task 串行 |
| `bsp_display_brightness_init()` / `set(percent)` | `display.set_backlight` | CH32 PWM 背光，百分比 0–100 |
| `bsp_display_backlight_on/off` | `power.set_screen_on` | 屏幕开关 |
| `bsp_display_get_input_dev()` | touch HAL `init` | 返回 BSP 注册的 LVGL touch indev |
| `bsp_display_new()` 内部 | 不直接调用 | 由 `bsp_display_start()` 完成 CH32 复位与面板初始化 |
| `bsp_touch_new()` | 不直接调用 | BSP 已创建一个 GT911 实例，避免重复 I2C 实例 |
| `bsp_display_rotate()` | `display.init` 内调用 | `bsp_display_start()` 后持锁执行 `LV_DISP_ROT_180`；触摸随 LVGL indev 变换 |

补充：

- `bsp_display_start()` 没有 stop/deinit API。`display.deinit` 只关背光，LVGL port
  资源不释放；停屏/重启流程需后续设计。
- BSP 源码未复制到仓库，依赖由 `main/idf_component.yml` 拉取。
- 官方示例在 `bsp_display_start()` 前做 I2C bus recovery（SDA=15/SCL=7，来自 BSP
  头文件），本适配层已照做，用于快速复位后 CH32 未同步复位的情况。
- 官方示例推荐 ESP-IDF 5.5.5/6.0.2；本工程已用 5.5.5 完成编译、烧录和启动。

## 3. 板级适配层

文件：`main/board_lcd4.c` / `main/board_lcd4.h`

```text
ESPUI HAL                 board_lcd4.c                 Waveshare BSP
display.init       ─────► I2C recover + start     ─────► bsp_display_start()
display.lock       ─────► bsp_display_lock()      ─────► lvgl_port_lock()
display.set_backlight ──► bsp_display_brightness_set()
touch.read         ─────► LVGL indev primary point
power.screen_on    ─────► bsp_display_backlight_on/off()
```

- `espui_board_stub()` 仍保留在 `components/espui/hal/`，仅供 host tests/CI；
  已更新为 480×480 信息。
- 真实板：`app_main` 使用 `espui_board_lcd4()`。
- `main/CMakeLists.txt` 依赖 `waveshare__esp32_s3_touch_lcd_4`。

## 4. 触摸限制

GT911 硬件支持 5 点，但 BSP 的 `esp_lvgl_port 2.9` touch read 只注册 1 个点：

```c
uint16_t touchpad_x[1];  // esp_lvgl_port_touch.c
```

因此当前 HAL 通过 LVGL indev 返回**主触点**（单点）。ESPUI 的输入/主题目前也只需
单点。多点手势（缩放/多指）需要后续替换 BSP touch port 或使用原始 GT911 handle，
本次未实现。

## 5. 480×480 布局

- Safe fallback：全宽标题/场景行 + 中部 448×320 面板 + 底部 footer。
- Terminal Aesthetic mock：header/scene、448×224 的 core 面板、448×96 的
  info 面板、footer；不是把 320×480 简单横向拉伸。
- Manifest：`orientation=any`、`min_width=min_height=480`。
- ASCII Renderer 默认网格改为 **60×30**，对应 480×480 / 约 8×16 字体。
- 显示颜色/触摸方向未在真机验证；不能仅凭构建结果认定。

## 6. 性能配置

`sdkconfig.defaults` 采用官方 V4 示例的性能项，但不启用 demo/font 全家桶：

```text
CPU 240 MHz
Flash QIO，16 MB（esptool 仍以 dio 写 bootloader，随后升级到 QIO）
Octal PSRAM 80 MHz
PSRAM fetch instructions + rodata
Data cache line 64 B
FreeRTOS 1000 Hz
LVGL RGB565，刷新周期 15 ms，heap 48 KB，FAST_MEM 使用 IRAM
LVGL 仅默认 Montserrat 14，无 lv_demo_*、无额外字体
```

注意：

- `CONFIG_ESPTOOLPY_FLASHMODE_QIO=y` 时，esptool 命令行仍显示 `--flash_mode dio`；
  这是 ESP-IDF 对 QIO bootloader 的烧录策略，运行时会升级到 QIO。
- BSP 默认 LVGL draw buffer 高度 100 → 480×100×2 ≈ 96 KB。切换全局
  `CONFIG_BSP_DISPLAY_LVGL_BUF_HEIGHT` 前需实测 PSRAM/内部 RAM 和刷新性能。
- `idf.py size` 的 IRAM 行显示 16,384/16,384（100%），与链接脚本
  `iram0_0_seg = 0x57700` 的口径不一致；实际 `.iram0.text` 81,547 bytes，
  链接断言通过。

## 7. 真机 bring-up 结果（2026-11-11）

已连接 `/dev/ttyACM0`，完成 16MB 原厂 Flash 备份、ESP-IDF 5.5.5 安装、
构建、烧录和 15 秒启动日志抓取。

关键启动证据：

```text
I boot: ESP-IDF v5.5.5 2nd stage bootloader
I boot.esp32s3: SPI Mode : QIO
I boot.esp32s3: SPI Flash Size : 16MB
I esp_psram: Found 8MB PSRAM device
I esp_psram: Speed: 80MHz
I esp_psram: SPI SRAM memory test OK
I cpu_start: cpu freq: 240000000 Hz
I ESP32-S3-Touch-LCD-4: Touch 0x14 found
I GT911: TouchPad_ID:0x39,0x31,0x31
I GT911: TouchPad_Config_Version:79
I board_lcd4: display rotated 180 degrees (touch transform follows LVGL indev)
I board_lcd4: LCD-4 BSP started: 480x480 RGB565
I espui: ESPUI initialized: theme=terminal_aesthetic
           display=waveshare-esp32-s3-touch-lcd-4-v4 480x480
I app_main: terminal scaffold running (LCD-4 BSP, mock state)
I main_task: Returned from app_main()
```

- 未出现 panic / assert / reset loop。
- 非致命 warning：
  - `i2c.master` 提示检查上拉电阻；GT911 仍成功识别。
  - ST7701 `36h` / `3Ah` 命令被 BSP 外部初始化序列覆盖。
- 启动日志：`/tmp/opencode/esp-lcd4-boot.log`（初次）
  / `/tmp/opencode/esp-lcd4-rotate-boot.log`（180° 修正后）
- 原厂备份：`/home/perch/esp/backups/lcd4-factory-2026-11-11-1132.bin`
  （16,777,216 bytes，SHA-256 `c996b103...f963e2c7`）

### 180° 旋转修正

`main/board_lcd4.c` 在 `bsp_display_start()` 成功后、持 `bsp_display_lock()`
调用：

```c
bsp_display_rotate(s_display, LV_DISP_ROT_180);
```

LVGL 8.4 的 `indev_pointer_proc()` 对 `LV_DISP_ROT_180` 自动做
`x = hor_res - x - 1`、`y = ver_res - y - 1`，所以 `espui_touch_hal_t.read()`
通过 `lv_indev_get_point()` 得到的已是旋转后的坐标。启动日志已确认
`display rotated 180 degrees`，无 panic/assert；实际屏幕方向与触摸命中仍需
人工确认。

### 期望画面

设备应停留在 480×480 Terminal Aesthetic Mock：

```text
MEETAGENT // TERMINAL
SCENE: AMBIENT
┌──────────────────────────────┐
│       ASCII CORE（静态）      │
└──────────────────────────────┘
┌──────────────────────────────┐
│ SYSTEM ACTIVE / IDLE / ...   │
│ AGENTS 01  SESSIONS 01 ...   │
└──────────────────────────────┘
E2 MOCK // ASCII RENDERER E3
```

Mock 状态每 3 秒循环：ACTIVE → APPROVAL → COMPLETED → OFFLINE。
期望颜色为近黑背景、磷光绿、琥珀、红色；180° 修正后画面应正立。**实际颜色/方向/触摸命中需人工确认**。

## 8. 待确认清单

1. 480×480 实际颜色顺序、旋转修正后的 ST7701 方向、背光亮度曲线（初始化已通过）。
2. GT911 坐标方向、镜像、单点触摸命中（检测已通过：0x14 / ID 39:31:31）。
3. CH32V003 电池 ADC 与电量百分比映射（BSP v3.0.0 未提供，当前返回未知 255）。
4. ~~16MB Flash / Octal PSRAM 与实物丝印一致~~（启动日志已确认 16MB / 8MB 80MHz）。
5. LVGL draw buffer 的分配位置（内部 RAM vs PSRAM）与真实余量（启动成功，未测余量）。
6. ~~BSP 在 ESP-IDF 5.5.5 上的运行时稳定性~~（已启动，长时间运行未测）。
7. ~~烧录后的启动日志、背光、触摸和 480×480 布局~~（启动已通过；画面/触摸仍人工确认）。
