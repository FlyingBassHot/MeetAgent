# PHOSPHOR CORE — P0 审计

日期：2026-11-11
范围：`firmware/terminal/`，仅 P0 审计 + P1 静态 Core。
本审计基于 E4b-3 基线；P1 不升级依赖、不改 BSP 旋转/触摸。

## 1. 软件基线

| 项目 | 版本 / 配置 |
|---|---|
| ESP-IDF | 5.5.5 |
| LVGL | 8.4.0 |
| Waveshare BSP | `waveshare/esp32_s3_touch_lcd_4` 3.0.0 |
| esp_lvgl_port | 2.9.0 |
| esp_websocket_client | 1.8.0（E4b-3，保持不变） |
| SDL2 | 2.32.74（`pkg-config sdl2`） |

依赖锁定来自 `firmware/terminal/dependencies.lock`：
`idf 5.5.5`、`lvgl 8.4.0`、`esp_lvgl_port 2.9.0`、`waveshare 3.0.0`。
P1 不新增 LVGL/BSP 依赖，也不升级 LVGL 主版本。

## 2. 屏幕、方向与触摸

- 逻辑分辨率：480×480。
- 面板：ST7701 RGB565，`LV_COLOR_DEPTH_16=y`，
  `CONFIG_LV_COLOR_16_SWAP` 未设置（0）。
- 显示初始化：`main/board_lcd4.c` → `bsp_display_start()`。
- 方向：项目物理安装为 180°；`display_init()` 在
  `bsp_display_lock()` 内调用 `bsp_display_rotate(s_display, LV_DISP_ROT_180)`。
  P1 不修改旋转参数。
- 触摸：BSP 的 GT911 LVGL indev，`bsp_display_get_input_dev()`；
  LVGL 8.4 的 `indev_pointer_proc()` 自动应用 180° 变换。
- ESPUI display lock 暴露为 `display->lock(timeout_ms)` /
  `display->unlock()`，P1 overlay 也使用这条路径，不直接调用 BSP 锁。
- RGB/BGR：当前 `LV_COLOR_16_SWAP=0`；P1 palette 直接写 RGB565，
  真机首次点亮时需要人工确认颜色方向（P1 报告记录）。

## 3. 内存与 Flash（E4b-3 基线）

```text
app binary      : 1,255,696 B
app partition   : 0x177000 (1.5 MB)
app free        : 0x446f0 (18%)
DIRAM           : 200,247 / 341,760 B (58.59%)
IRAM            : 16,384 / 16,384 B（idf.py size 固定窗口；链接断言通过）
internal free   : ~64,823 B（真机 welcome 后日志）
PSRAM           : 8 MB Octal，80 MHz
PSRAM free      : ~6,514,984 B
```

P1 额外需求：

- Core Canvas 288×288 RGB565 = **165,888 B**，一次性 PSRAM 分配；
- Glyph Frame 36×24×4 B = 3,456 B（栈/内部 RAM，一帧一次）；
- 8×12 1bpp atlas：27 glyph × 12 B = **324 B** Flash rodata；
- 无每帧 malloc、无每帧 LVGL 对象创建、无动画定时器。

结论：P1 没有 Flash/IRAM/内部 RAM 硬阻塞。PSRAM 余量足够容纳单 canvas；
仍需在真机确认 PSRAM 分配的 LVGL canvas 经 esp_lvgl_port 正常刷新。

## 4. 现有 ESPUI 架构

```text
board_lcd4.c
  → ESPUI HAL (display/touch/input/power)
  → LVGL renderer / theme manager
  → ESPUI UI Task + fixed event queue (4)
  → ViewModel (fixed 8 agents / 8 sessions / 6 attention)
  → terminal_aesthetic theme (safe fallback: safe_fallback)
```

- UI Task 唯一操作 LVGL；网络任务只 post ViewModel/action result。
- `espui_post_view_model()` 是非阻塞队列投递。
- 网络/状态路径保持 E4b-3：WebSocket、snapshot、event apply、heartbeat、
  command gateway；P1 不触碰这些模块。
- 主题切换/场景仍由原 Theme Manager 管理；P1 overlay 不修改 Theme API。

## 5. P1 集成选择

**选择：临时 top-layer overlay。**

- 在 `espui_start()` 之后创建一个 full-screen black overlay（`lv_layer_top()`）；
- 在其上创建单个 288×288 LVGL canvas，位置 `(96, 72)`；
- canvas buffer 放 PSRAM，只分配一次；P1 只渲染一次静态帧；
- 旧 `terminal_aesthetic` 仍在底层初始化并保留为 fallback；P1 overlay
  只是临时 P1 展示层，P2/P3 再决定是否变成正式 theme 组件；
- 不改 BSP 旋转/触摸、不改 E4 网络/WS/ViewModel 流程；
- 如果 PSRAM/canvas 创建失败，overlay 不启动，旧主题继续显示，不 panic。

不采用“新建完整 theme”的原因：P1 只验证几何/atlas/像素路径；overlay 能把
LVGL 生命周期与现有主题解耦，失败可回退，且不会把 P1 临时实现固化成正式
Theme API。

## 6. P1 模块边界

纯 C（不 include LVGL/ESP-IDF/cJSON）：

```text
phosphor_types.h
phosphor_palette.c/.h
glyph_frame.c/.h
glyph_atlas.c/.h
glyph_rasterizer.c/.h
orbit_geometry.c/.h
radial_field.c/.h
satellite_layout.c/.h
phosphor_scene.c/.h
```

平台适配（唯一 include LVGL）：

```text
phosphor_lvgl_port.c/.h
```

PC SDL2 只替换显示适配：

```text
tools/phosphor_sim/main.c
```

## 7. 字体 Atlas 方案

- 8×12 1bpp，27 个实际使用 ASCII：空格、ramp、方向、`>_ ? ! o *` 等；
- 来源：JetBrains Mono Regular（`JetBrainsMonoNerdFont-Regular.ttf`），
  SIL Open Font License 1.1；用 ImageMagick 在 12 pt 生成 8×12 黑白位图，
  50% threshold；
- 只读数据放 Flash rodata（324 B），PC/ESP32 共用同一份 `glyph_atlas.c`；
- 字体二进制不随固件分发，只分发生成的 bitmap 数据。

## 8. 风险 / P1 待确认

- 165,888 B PSRAM canvas 经 esp_lvgl_port 刷新的实际耗时/是否 DMA 兼容；
- 全屏 top-layer 对旧主题触摸/场景的影响（P1 无交互，需 P2/P3 设计）；
- 180° 旋转下 overlay 的 `(96,72)` 是否与 PC 坐标一致（逻辑坐标应一致）；
- RGB565 颜色方向（R/B 是否颠倒）需真机人工确认；
- 字体 8×12 在 480×480 实屏上的可读性；
- P1 只做静态帧，不验证 FPS；P2/P5 再测动画与性能。

## 9. P0 结论

依赖版本、屏幕路径、PSRAM 和 Flash 余量均满足 P1 静态 Core；
无硬阻塞。按 P1 静态 Core 实施，不进入 P2/P3/P4/P5/P6。


## 10. P1.1 可读性迭代偏差（2026-11-11）

P0 的 288×288 / 8×12 估算作为 P1.0 基线保留；P1.1 根据实机反馈调整为：

- cell 12×18；canvas 432×432；位置 (24,24)；中心 (216,216)；
- PSRAM canvas 变为 373,248 B；PSRAM 仍有余量；
- 36×24 网格和 Nucleus/Satellite/三轨结构不变；
- P1.1 仍无动画/定时器，不进入 P2。

详见 `docs/PHOSPHOR_P1.md` 和 `/tmp/opencode/phosphor-p1b-report.md`。


## 11. P1.2 字体可读性偏差（2026-11-11）

- 网格 24×16、cell 18×27、canvas 432×432 保持；(24,24) 不变；
- atlas 改为 JetBrains Mono Bold 18×27，45% threshold + 1px dilation；
- Nucleus 2×（4×2 cells）；主轨道使用 Primary，Inner Secondary；
- 这是 P1.1 实机“字体细/小”反馈的修正，不是 P2 状态/动画工作。


## 12. P1.3 轨道可辨性偏差（2026-11-11）

- 保持 24×16 / 18×27 Bold / 432×432 / (24,24)；
- 半径 80/140/192，倾角/旋转/相位进一步拉开；
- Inner Secondary、Middle Secondary、Outer Primary，后侧不低于 175–185；
- Middle 5.85 rad 缺口 + Outer warning 缺口；ambient field 移除；
- 这是静态 P1.3 视觉迭代，不进入 P2。


## 13. P2 Glyph Renderer / 颜色 / 合成（2026-11-11）

- P1.3 视觉基线不变；P2 只加 ramp、中心密度场、Palette/Compositor 验证；
- `glyph_ramp` 为纯模块；P2 密度场限制在中心 30–78，不进轨道/无 halo；
- Palette RGB565 精确值已测试；`LV_COLOR_16_SWAP=0` 编译期检查；
- P2 仍是静态，不含 P3 动画。


## 14. P3 Idle/Working（2026-11-11）

- 纯 motion/LUT；Idle/Working profile、1s 过渡、固定 phase 可复现；
- 平台单任务 demo 6s Idle/6s Working；
- 实测 Idle 7 FPS，Working 7 FPS（目标 10–15 未达成，P5 再优化）；
- P3 未进入 P4。


## 15. P3.1 二进制流/密度（2026-11-11）

- Atlas 增加 0/1；Outer 96 采样，字符 `+ # % @ 0 1`；
- 0/1 使用 index/time hash，Working 300ms/Idle 800ms bucket；
- 8 个不规则稀疏外围 0/1 点，无整圈 halo；
- 实测 Working 7 FPS，仍低于目标；P5 优化。


## 16. P3.2 外圈副带/Inner 密度（2026-11-11）

- 删除外围稀疏 0/1 点；Outer 连续内侧副带 radius 176、FIELD、warning 缺口；
- Inner 80 半径不变，采样 96，提高 `: ;` 密度；
- 不进入 P4；Working FPS 仍待 P5 优化。


## 17. P3.3 相邻 cell 加厚（2026-11-11）

- Outer/Inner 使用相邻 cell band 替代 radius-176 副带；
- warning gap 0.11 / middle gap 0.04，低亮度填充；
- v3→v4 changed pixels 5,740/5,510；Inner/Outer band cells 6/23；
- P3 仍静态，P4 未开始。
