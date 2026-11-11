# PHOSPHOR P5：性能优化与稳定性

状态：dirty-cell rasterizer + partial LVGL invalidate 已烧录；
P6 real-ViewModel 映射保持；2h soak 已启动，报告时尚未跑满。

## 1. 基线

P6 real ViewModel（`state=node`）：

```text
Working baseline log: render_avg≈132 ms, 6–7 FPS
Idle user baseline:    render_avg≈34 ms, 6–7 FPS
```

原路径每帧：

1. 全量 432×432 RGB565 背景填充（186,624 像素）；
2. 全帧 glyph 重绘；
3. 每帧 `lv_obj_invalidate(s_canvas)` 全 canvas；
4. delay 按目标 FPS 计算。

## 2. Dirty-cell rasterizer

新增 `glyph_dirty.c/.h`：

- 保存上一帧 `glyph_frame_t`；
- 逐 cell 比较 `glyph/color/intensity/flags/scale`；
- 把变化的 **cell footprint** 标脏：
  - 普通 cell：18×27；
  - Nucleus：18×27 × scale（2× = 36×54）；
- 清除 dirty cell 的 18×27 区域；
- 只重绘与 dirty 区域相交的当前 cell；
- 普通 cell 先、Nucleus 后，保持原 full renderer 的层叠顺序；
- 空 cell 会清掉旧 glyph，不产生 ghost；
- 输出 changed bbox；首次/尺寸/调色板变化走 full。

## 3. Partial invalidate + 背景

- `lv_obj_invalidate_area(canvas, bbox)`；
- bbox 面积 ≥ canvas 70% 时回退 `lv_obj_invalidate(canvas)`；
- 不再每帧全 canvas invalidate；
- 全量背景填充改为 32-bit 两像素写入；
- dirty 清屏只清 dirty cell，不做全屏背景。

## 4. 节流

- Idle 目标 6–8（实测 7）；
- Working 目标 10–15（real P6 10–11，见下）；
- delay 按 `frame_ms - render_ms`，不空转；
- 没有降低字符密度/几何；R2.1 视觉保持。

## 5. Host 验证

`tests/host`：**3966 checks, 0 failures**。

新增 dirty 测试：

- 首次 full / 空背景；
- 随机 80 帧 full vs dirty 像素完全一致；
- 空 cell ghost 清除；
- 单 cell bbox（5,4 → 90,108,18,27）；
- 2× Nucleus bbox（36×54）；
- 右下边界 clipping；
- no-op frame 不产生 invalidation；
- R2.1/state/motion/P6 回归保持。

PC 固定时间回归：`p5_working/approval/error.png` 与 `r2b_*` pixel diff = **0**。

## 6. 实机结果

### Idle（final P6 real state=node activity=0）

```text
fps=7
render_avg 15.9–21.1 ms
build_avg  3.9–4.1 ms
raster_avg 11.9–16.9 ms
invalidate_avg 0.07 ms
dirty_changed 58–95; bbox 306–324 px; full=0
stack_hwm 1352 / 6144
internal_free 52.9→62.1 KB; psram_free 6,129,512 B
```

### Working（final P6 real-ViewModel，soak 中 activity=1）

```text
fps=10–11
render_avg 19.5–24.8 ms
build_avg  4.4–4.9 ms
raster_avg 15.0–19.8 ms
invalidate_avg 0.07 ms
dirty_changed 41–50; bbox 306–360; full=0
stack_hwm 1176 / 6144
period 87–90 ms, delay 71–72 ms
```

Working 达到目标下限 **10–11 FPS**；没有达到 12–15 的高端。
临时 demo 诊断同样为 10 FPS（render 19.1ms）；
最终固件是 `state=node` real-ViewModel，未被诊断 demo 替换。

### Approval / Error 诊断

Working+Approval：11 FPS，render 21.5ms；Working+Error：11 FPS，render 18.8ms。
R2.1 全屏 `?`/`!` 视觉未改。

## 7. 烧录

- IDF 5.5.5 build 通过；
- app-flash 0x10000 / NVS preserved；
- app **1,289,984 B**，free 0x3c100 (16%)；
- app SHA-256
  `9af8a962d0b9ce1b1838dd030251e4f192bf996612f32e828f11190a81ad3928`；
- DIRAM **207,419 / 341,760 (60.69%)**；IRAM 16,384/16,384。

## 8. 2h soak

- 已启动后台串口 capture：`/tmp/opencode/phosphor-p5-soak.log`；
- 报告时已运行约 2.5min，`state=node`，Idle 7、Working 11 FPS，无 panic；
- 2 小时完整结果待后台继续观察，尚未完成，不宣称通过；
- 需要重点看 internal/PSRAM 曲线、heartbeat、panic、UI 卡死。

## 9. 未验证

- 完整 2h soak 尚未结束；
- LVGL flush/DMA 的独立耗时未拆出（invalidate 仅 0.07ms，主要时间在 raster）。
