# PHOSPHOR CORE — P2 Glyph Renderer / 颜色 / 密度 / 前后合成

日期：2026-11-11
状态：P2 静态渲染层完成，真机启动通过；实屏最终视觉由用户确认。未进入 P3。

## 1. 保持 P1.3 基线

P2 不改变 P1.3 已接受的可读性基线：

- 24×16 grid / 18×27 Bold atlas / 432×432 canvas / (24,24)
- 半径 80/140/192，三轨 tilt/rotation/phase 与缺口保持
- Inner/Middle/Outer 颜色与字符集保持
- 2× `>_` Nucleus；`*` / `?` Satellite
- 不再加 orbit halo 或密集 ambient

## 2. Glyph Ramp

新增 `glyph_ramp.c/.h`：

```c
ramp = " .,:-=+*#%@"
index = intensity * 10 / 255
```

| intensity | ramp index | glyph |
|---:|---:|---|
| 0 | 0 | space |
| 1–25 | 0–0 | space |
| 26 | 1 | `.` |
| 102 | 4 | `=` |
| 255 | 10 | `@` |

- 单调不减，0→space，255→`@`；
- 所有非空 ramp glyph 都在 18×27 atlas 中；
- PC/ESP32 共用同一实现。

## 3. 核心密度场

`phosphor_scene.c` 中新增 `draw_core_density()`：

- 只在中心半径 **30–78 px** 的范围内采样；
- 使用 `phosphor_smooth_falloff(distance, 78)`；
- `intensity = 170 * falloff`，经 `glyph_ramp_char()` 映射；
- 颜色 Secondary，`GLYPH_FLAG_FIELD`；
- 密度层被关闭在 inner orbit（80）以内，不贴轨道形成 halo；
- 不添加 ambient random/speckle，静态可复现；
- 2× Nucleus 最后由 rasterizer second pass 清空 footprint 并覆盖。

这样 P2 截图有中心连续密度层次，同时三轨仍独立。

## 4. Palette

`phosphor_palette.c` / `phosphor_palette.h` 统一 Token：

| Token | HEX | RGB565 |
|---|---|---:|
| Background | #080C0A | 0x0861 |
| Surface | #101813 | 0x10C2 |
| Primary | #A7F36B | 0xA78D |
| Secondary | #62C79B | 0x6633 |
| Muted | #587364 | 0x5B8C |
| Warning | #F2B75E | 0xF5AB |
| Error | #F06C67 | 0xF36C |
| Highlight | #D9FFC3 | 0xDFF8 |

- `phosphor_scale565()` 线性缩放 RGB565 通道，0→0，255→原色，单调；
- `phosphor_lvgl_port.c` 增加编译期检查：
  `#if LV_COLOR_16_SWAP != 0 #error`；
- 实屏 R/B 方向仍需用户确认，PC 截图不能代替实屏验收。

## 5. 前后层级合成

`glyph_frame_layer_priority()` 明确定义：

```text
NUCLEUS (5) > SATELLITE (4) > FRONT (3) > BACK (2) > FIELD (1)
```

- 同层按 intensity 决定是否替换；
- 低优先级不能在重叠 cell 覆盖高优先级；
- Rasterizer：
  - 普通 cell 每 cell 绘制一次；
  - Nucleus 使用第二 pass，先清空 2× footprint，再绘制；
  - 所有像素写入前做 width/height 裁剪；
- 新增测试覆盖 FIELD→BACK→FRONT→SATELLITE→NUCLEUS 覆盖顺序、
  scale 2、scale 3 拒绝、2× 边缘裁剪。

## 6. PC 截图

```bash
cd firmware/terminal/tools/phosphor_sim
make
SDL_VIDEODRIVER=dummy ./build/phosphor_sim --screenshot build/p2.bmp
convert build/p2.bmp ../../docs/phosphor/p2_static.png
```

P2 截图：

```text
path   : firmware/terminal/docs/phosphor/p2_static.png
size   : 480x480 PNG
bytes  : 2,873
sha256 : 9d9fc20abfcad1e3f0dddd215faf069d421f676f5153029135e8077763384117
```

P1.3 截图保留：`docs/phosphor/p1_static_v4.png`。

## 7. 真机

```text
app size : 1,267,472 B (0x135710)
sha256   : f18b6350c5262ed7571422675b5d46121b8ce725259c55a7e192681df6f10f34
app free : 0x418f0 (17%)
DIRAM    : 200,255 / 341,760 B (58.6%)
```

启动日志 `/tmp/phosphor-p2-boot.log`：

```text
phosphor_lvgl: P1 static canvas ready: 432x432 at (24,24), PSRAM=373248 bytes
meet_net: welcome memory: internal_free=62887 psram_free=6134552
meet_net: snapshot applied: agents=1 sessions=8/42 seq=866
app_main: network state: online
heartbeat RTT=101 ms
```

- Wi-Fi/Node/WS 在线；
- PSRAM canvas 一次 373,248 B；
- 无 panic/assert/OOM；
- 180° 旋转/GT911 未改；
- 实屏连续密度层次、颜色和轨道分离度待用户确认。

## 8. Host 验证

```text
make test          -> 3541 checks, 0 failures
syntax_check.sh    -> 42 files OK
```

新增覆盖：

- ramp 0..255 单调、0/255 边界、所有 glyph 在 atlas；
- 8 个 Palette RGB565 精确值、scale565 单调/边界；
- compositor 优先级五级顺序、2× Nucleus、scale 3 拒绝、边缘裁剪；
- P1.3 三轨参数/Nucleus/Satellite 回归。

## 9. P3 待办（不开始）

- Motion Scheduler / Idle / Working 连续相位；
- 动态密度、Completed/Approval/Error/Offline；
- ViewModel 状态适配；
- FPS/长时间稳定性测量。
