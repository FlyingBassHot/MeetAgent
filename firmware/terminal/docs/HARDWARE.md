# 硬件与 HAL（E0 / E1 / E2）

## 1. 目标硬件

目标硬件已从旧的 3.5B 切换为 **Waveshare ESP32-S3-Touch-LCD-4 V4.0**（近期购买确认最新版）。

| 项 | 值 | 状态 |
|---|---|---|
| 开发板 | Waveshare ESP32-S3-Touch-LCD-4 V4.0 | 已确认 |
| SoC | ESP32-S3（240MHz） | 官方规格 |
| 屏幕 | 4.0 英寸，480 × 480，RGB565 | 官方规格 |
| 显示接口 | RGB 并口 LCD | 官方 BSP 可用 |
| 触摸 | GT911，I2C，五点触控，支持中断 | 官方 BSP 可用 |
| Helper | CH32V003F4U6（I2C）：背光、复位、蜂鸣器、电源/电池 ADC | V4 实物 |
| Flash / PSRAM | 16MB Flash + 8MB Octal PSRAM（N16R8） | `sdkconfig.defaults` 暂按 Octal |
| 板载输入 | BOOT / RESET / BAT_PWR；**无旋钮** | 触摸为主，按键辅助 |
| 背光 | 经 CH32V003 helper 控制 | 以 V4 BSP 为准 |
| 其他 | CAN、RS485、RTC、SD、蜂鸣器、电池 | 首版 Terminal 不要求全部使用 |

**本工程没有写死任何 GPIO、LCD 控制器型号、触摸控制器型号、I2C/SPI 地址或背光控制通道。** 这些必须来自 V4 原理图、官方 `06_lvgl_demo_v8` 示例和官方 BSP 实测。

### 官方 BSP 与版本差异

- V4 官方 BSP：`waveshare/esp32_s3_touch_lcd_4 ^3.0.0`
- 官方示例：`examples/esp-idf/06_lvgl_demo_v8`（LVGL 8.4）
- V4 使用 CH32V003 helper，**不是** TCA9554；旧版 V1 的 I2C/TCA9554 映射不适用
- 已确认实物为 V4，不再按 V1/V2/V3 兼容设计；以后需要时再加版本分支

## 2. HAL 接口

### Display

`hal/espui_display.h`

```c
espui_display_info_t {
    panel_name, width, height, orientation, color_depth, supports_touch
}

espui_display_hal_t {
    init, deinit, get_info,
    lock(timeout_ms), unlock,      // 可选，建议映射 esp_lvgl_port_lock/unlock
    set_backlight(percent)         // 可选
}
```

- `width/height` 是旋转后的逻辑尺寸。
- `get_info()` 在 `init()` 后调用。
- `lock/unlock` 为 NULL 时，runtime 假定单线程访问（仅适合 headless/主机检查）。
- 真实 BSP 必须提供 LVGL display driver、draw buffer、flush 和 `lv_timer_handler` 驱动方式。

### Touch

`hal/espui_touch.h`

```c
read(points, max_points, &count)
```

- 坐标为目标逻辑像素，已由板级完成旋转/校准。
- 无触摸时返回 `ESP_OK, count=0`。

### Input

`hal/espui_input.h`

```c
read_event(event, timeout_ms)
```

- 板级输入任务把 PCNT/GPIO 事件转换成 `espui_input_event_t`。
- 编码器方向、按键消抖、长按阈值属于板级；ESPUI 只处理抽象事件。
- 输入任务不得操作 LVGL。

### Power

`hal/espui_power.h`

- 电池百分比（未知用 255）、充电/USB 状态、屏幕开关。
- E5 再接入亮度/休眠策略。

### Board

`hal/espui_board.h`

```c
espui_board_hal_t {
    board_id, display, touch, input, power
}
```

当前实现：`espui_board_stub()`（headless，**仍为旧的 320×480**，无真实 flush/触摸/输入）。  
LCD-4 目标为 480×480；stub 与两个主题布局将在 480×480 适配阶段一起更新。

## 3. 为什么先做 stub 板

- 没有原理图确认前固定引脚会制造错误假设。
- E0/E1/E2 的目标是验证分层、主题、状态流和回退，不依赖面板点亮。
- stub 仍提供完整 DisplayInfo，使 Manifest 校验、主题布局、UI Task 可以运行。
- 真机 BSP 只需替换 `hal/` 与 `main/app_main.c` 的 board bundle，不触碰 Theme / ViewModel / Protocol。

## 4. LVGL 接入约定

- LVGL 版本：`^8.3.0`（当前 Component Manager 解析到 8.4.0）。
- ESPUI 的 Renderer 只使用 LVGL v8 API。
- `lv_timer_handler()` 由板级 LVGL port task 调用；ESPUI UI Task 不负责刷新，只负责状态/主题对象更新。
- 所有 ESPUI 对 LVGL 的调用都在 display HAL lock 内；板级 port 必须在同一把锁内调用 `lv_timer_handler`。
- 如果使用 `esp_lvgl_port`：
  - `display.lock` → `lvgl_port_lock(timeout)`
  - `display.unlock` → `lvgl_port_unlock()`
  - 屏幕/触摸注册仍由 BSP 完成。

## 5. 待确认清单

1. 实物 V4.0 丝印/包装已确认（近期购买）。
2. RGB LCD 的 R/G/B、HSYNC、VSYNC、DE、PCLK、LCD_CS/SPI 初始化引脚（以官方 BSP 为准）。
3. CH32V003 helper 的 I2C 地址/寄存器与背光/复位映射（以官方 BSP 为准）。
4. GT911 I2C 地址、INT/RST、坐标方向与 5 点触控配置。
5. 背光亮度控制与有效电平。
6. PSRAM 封装（Octal）与 Flash 容量（16MB）实机确认。
7. 电源路径、USB 检测、电池 ADC（首版可延后）。
8. 官方 BSP 与 ESP-IDF 5.4.4 / LVGL 8.4 的兼容性。

优先使用官方 BSP；在 BSP 验证前不写死外设参数。只有官方 BSP 与 LVGL 8.4 / ESP-IDF 5.4.4 不兼容时，才实现最小自定义 BSP。

## 6. 真机接入步骤（建议）

1. 在 `components/espui/idf_component.yml` 或工程依赖中加入 `waveshare/esp32_s3_touch_lcd_4 ^3.0.0`。
2. 用官方 `bsp_display_start()` / `bsp_display_lock()` 完成 LCD、GT911、CH32V003 初始化和 LVGL port 注册。
3. 把 BSP 的显示信息、触摸读取和背光控制封装进 `espui_board_hal_t`，替换 `espui_board_stub()`。
4. 在 `app_main` 先完成 BSP 初始化，再调用 `espui_init()`。
5. 用串口日志确认 PSRAM、Flash、面板方向和 GT911 坐标。
6. 点亮后先验证 `safe_fallback`，再切 `terminal_aesthetic` mock。
7. 完成 480×480 方形布局适配，再进入 E3b 的 ASCII → LVGL 字符网格。
