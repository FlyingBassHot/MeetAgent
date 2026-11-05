# 硬件与 HAL（E0 / E1 / E2）

## 1. 目标硬件

| 项 | 值 | 状态 |
|---|---|---|
| 开发板 | Waveshare ESP32-S3-Touch-LCD-3.5B | 任务书指定 |
| SoC | ESP32-S3 | 任务书指定 |
| 屏幕 | 320 × 480 竖屏，RGB565 | 任务书指定，待实物确认 |
| 触摸 | 电容触摸（具体控制器待确认） | **未假设** |
| 旋钮 / 按键 | 数量与 GPIO 待确认 | **未假设** |
| PSRAM | N16R8 / Octal 待确认 | `sdkconfig.defaults` 暂按 Octal |
| Flash | 16 MB 待确认 | `sdkconfig.defaults` 暂按 16 MB |
| 背光 | 控制方式待确认 | HAL 仅保留接口 |

**本工程没有写死任何 GPIO、LCD 控制器型号、触摸控制器型号、I2C/SPI 地址或背光 PWM 通道。** 这些必须来自开发板原理图、官方示例和实测。

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

当前实现：`espui_board_stub()`（headless，320×480，无真实 flush/触摸/输入）。

## 3. 为什么先做 stub 板

- 没有原理图确认前固定引脚会制造错误假设。
- E0/E1/E2 的目标是验证分层、主题、状态流和回退，不依赖面板点亮。
- stub 仍提供完整 DisplayInfo，使 Manifest 校验、主题布局、UI Task 可以运行。
- 真机 BSP 只需替换 `hal/` 与 `main/app_main.c` 的 board bundle，不触碰 Theme / ViewModel / Protocol。

## 4. LVGL 接入约定

- LVGL 版本：8.3（`components/espui/idf_component.yml` 固定 `^8.3.0`）。
- ESPUI 的 Renderer 只使用 LVGL v8 API。
- `lv_timer_handler()` 由板级 LVGL port task 调用；ESPUI UI Task 不负责刷新，只负责状态/主题对象更新。
- 所有 ESPUI 对 LVGL 的调用都在 display HAL lock 内；板级 port 必须在同一把锁内调用 `lv_timer_handler`。
- 如果使用 `esp_lvgl_port`：
  - `display.lock` → `lvgl_port_lock(timeout)`
  - `display.unlock` → `lvgl_port_unlock()`
  - 屏幕/触摸注册仍由 BSP 完成。

## 5. 待确认清单

1. LCD 控制器型号、SPI/RGB 接口、初始化和旋转。
2. 触摸控制器型号、I2C 地址、中断/复位引脚、坐标映射。
3. 背光控制方式（GPIO/PWM/恒流芯片）。
4. 编码器 A/B/按键 GPIO 与上拉；独立 Back / Interrupt 键是否存在。
5. PSRAM 封装（Octal/Quad）与 Flash 容量。
6. 电源路径、USB 检测、电池 ADC。
7. 官方 BSP/示例版本与 LVGL 版本是否匹配。

在以上确认前，不添加 `board_*.c` 真实实现，也不在 `sdkconfig.defaults` 写死外设参数。

## 6. 真机接入步骤（建议）

1. 依据原理图实现 `hal/display_*.c`、`hal/touch_*.c`、`hal/input_*.c`、`hal/power_*.c`。
2. 组装 `espui_board_hal_t`，替换 `espui_board_stub()`。
3. 在 `app_main` 先完成 BSP 初始化，再调用 `espui_init()`。
4. 确认 LVGL port 的锁与 `lv_timer_handler` 任务。
5. 用 `esptool.py` / 串口日志确认 PSRAM、Flash、面板方向。
6. 点亮后先验证 `safe_fallback`，再切 `terminal_aesthetic` mock。
7. 最后进入 E3 的 ASCII Renderer。
