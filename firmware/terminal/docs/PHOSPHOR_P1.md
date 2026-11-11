# PHOSPHOR CORE — P1.3 静态 Core（轨道可辨性）

日期：2026-11-11
状态：P1.3 静态 Core 完成，真机启动通过；实屏可读性由用户确认。
P2 已完成（Glyph Renderer/颜色/密度/合成）；P3 已完成 Idle/Working 动画。

## 1. 反馈与 P1.3 修正

用户反馈：字体大小可以（保持 24×16 / 18×27 / Bold），但三条轨道看不清。

P1.3 保留字体/网格/canvas，只调整轨道几何、颜色和背景密度：

| 项目 | P1.2 | P1.3（当前） |
|---|---:|---:|
| Grid / Cell | 24×16 / 18×27 | 24×16 / 18×27 |
| Canvas | 432×432 @ (24,24) | 432×432 @ (24,24) |
| Atlas | 18×27 Bold + dilate | 18×27 Bold + dilate |
| Inner radius | 78 | **80** |
| Middle radius | 126 | **140** |
| Outer radius | 168 | **192** |
| Ambient radial points | 14 | **0** |

P1.3 不再缩小字体；Nucleus 2×、Satellite 固定位置保持不变。

## 2. 轨道参数

| Orbit | Radius | Tilt | Rotation | Phase | Character / Color |
|---|---:|---:|---:|---:|---|
| Inner | 80 px | 1.00 rad | -0.55 rad | 0.10 rad | `. , : ;` / Secondary（后侧 Muted，≥185） |
| Middle | 140 px | 0.25 rad | 0.85 rad | 1.90 rad | `- = / \` / Secondary 235 |
| Outer | 192 px | 1.30 rad | -0.65 rad | 3.80 rad | `+ # % @` / Primary 250 |

- Outer 距离中心 192；半 cell 约 13.5 px，留约 10.5 px 安全边。
- Outer warning 缺口保留；Middle 新增独立缺口，中心角约 `5.85 rad`，
  半宽约 `0.23 rad`（与 warning 卫星 3.75 rad 不同）；Inner 完整。
- 后侧 cell 不再整段切 Muted/125：
  - Inner 后侧 Muted，intensity 185；
  - Middle 后侧 Secondary，intensity 175；
  - Outer 后侧 Primary，intensity 185。
- 轨道源 64 点 + DDA 连续段；无 4 邻域 halo。
- Ambient radial field 已移除；没有额外噪点。

## 3. 核心 / Satellite

- Nucleus `>_` 保持 2×，4×2 cells，2× rasterizer second pass；
  bbox 中心 (216,216)。
- Satellite：`*` Running Highlight / `?` Warning，固定角度，位置稳定。
- Core shell 仍为稀疏 `:`/`.`，不参与轨道密度。
- 背景 #080C0A。

## 4. PC SDL2

```bash
cd firmware/terminal/tools/phosphor_sim
make
./build/phosphor_sim                          # normal window, ESC
SDL_VIDEODRIVER=dummy ./build/phosphor_sim --screenshot build/p1_v4.bmp
convert build/p1_v4.bmp ../../docs/phosphor/p1_static_v4.png
```

历史截图保留：

```text
docs/phosphor/p1_static.png      # P1.0
docs/phosphor/p1_static_v2.png   # P1.1
docs/phosphor/p1_static_v3.png   # P1.2
```

P1.3 新截图：

```text
path   : firmware/terminal/docs/phosphor/p1_static_v4.png
size   : 480x480 PNG
bytes  : 3,002
sha256 : b581b0fff2adfebd253adc08e727edf135c3140f990dcfd8b35de85d313f76fc
```

## 5. 真机结果

```text
app size : 1,267,232 B (0x135620)
sha256   : b9156df11333db94a7bf042cf31295614c15177e5745259bf0e9ce768d2d0ef5
app free : 0x419e0 (17%)
DIRAM    : 200,255 / 341,760 B (58.6%)
```

启动日志（`/tmp/phosphor-p1d-boot.log`）：

```text
phosphor_lvgl: P1 static canvas ready: 432x432 at (24,24), PSRAM=373248 bytes
meet_net: welcome memory: internal_free=64771 psram_free=6134568
meet_net: snapshot applied: agents=1 sessions=8/42 seq=757
app_main: network state: online
heartbeat RTT=53 ms
```

- Wi-Fi/Node/WS 在线；
- PSRAM canvas 仅一次 373,248 B；
- 无 panic/assert/OOM；
- 180° 旋转与 GT911 未改；
- 实屏轨道分离度/颜色/笔画仍需用户确认。

## 6. Host 验证

```text
make test          -> 2013 checks, 0 failures
syntax_check.sh    -> 41 files OK
```

覆盖三轨半径 80/140/192、tilt/rotation/phase、Nucleus 2×、18×27 atlas、
Satellite、Palette、Scene/Rasterizer、1×1 裁剪。

## 7. 后续（P2 已完成）

- P2：Glyph Ramp、中心密度场、Palette/Compositor（见 `PHOSPHOR_P2.md`）；
- P3 待办：Motion Scheduler / Idle / Working；
- P4：Completed / Approval / Error / Offline；
- P5：FPS/内存/长时间稳定性；
- 后续再决定 overlay 是否转为正式 Theme/Renderer 组件。
