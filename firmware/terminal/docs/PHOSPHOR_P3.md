# PHOSPHOR CORE — P3 Idle / Working 动画

日期：2026-11-11
状态：P3 Idle/Working 动效实现并真机运行；未进入 P4。实屏动画由用户确认。

## 1. 保持 P1.3/P2 基线

- 24×16 grid / 18×27 Bold / 432×432 / (24,24)
- 三轨 80/140/192、颜色/字符/缺口、2× `>_`、`*`/`?` Satellite
- Glyph Ramp、Palette、FIELD/FRONT/BACK/SATELLITE/NUCLEUS 优先级
- 无 orbit halo、无密集 ambient

## 2. Pure Motion

新增 `phosphor_motion.c/.h`、`phosphor_lut.c/.h`：

| Profile | Inner rad/s | Middle rad/s | Outer rad/s | Breath | Density | Brightness |
|---|---:|---:|---:|---:|---:|---:|
| Idle | +0.08 | -0.045 | +0.025 | 8s | 0.30 | 0.82 |
| Working | +0.22 | -0.55 | +0.32 | 5s | 0.75 | 1.00 |

- `mode_mix` 0..1，状态切换 1.0s 平滑；
- `activity_energy` 450ms 低通；
- `density`/`brightness` 随 mode_mix 插值；
- phase/breath 在 0..2π 内 wrap；
- `delta` 截断 200ms；
- 同一 state/time 序列完全可复现；
- sin/cos 使用 1024 点 LUT，不在每帧调用 `sinf/cosf`；
- 无每帧 malloc，无 LVGL 对象创建。

## 3. Scene

`phosphor_scene_build_frame_motion(frame, motion)`：

- orbit phase 叠加 motion 相位；
- Idle 慢速、低密度；Working 中轨快速、核心密度提高；
- Working 添加 2–7 个固定相位 `+`/`%` 活动粒子（不使用 `*`，保持 Satellite 唯一）；
- Satellite 位置固定，不随机换位；
- Nucleus 2× 固定中心，亮度随呼吸 235–255；
- 静态 API `phosphor_scene_build_frame()` 保留为 P2 回归/基准路径。

## 4. ESP32 刷新

`phosphor_lvgl_port.c` 新增单任务：

- Demo：**6s Idle ↔ 6s Working**（临时演示；P6 再接真实 ViewModel）；
- Idle 目标 7 FPS，Working 目标 12 FPS；
- display lock 内更新 PSRAM canvas + `lv_obj_invalidate(s_canvas)`；
- 不调用全屏 `scene_commit`，不创建新 LVGL 对象；
- 每 5s 日志：mode/frames/fps/render_avg/loop/period/delay/internal/PSRAM；
- 无 heap 增长；网络/WS 任务未被修改。

### 实测（真机）

```text
Idle    fps=7  render_avg=32–76ms  period=141–146ms
Working fps=7  render_avg=76–106ms period=132–135ms
```

- **Idle 达到 6–8 FPS 目标。**
- **Working 未达到 10–15 FPS 目标**；在 432×432 canvas 全量渲染 + LVGL 刷新
  下实测约 7 FPS。此阶段不做 P5 优化，不虚报达标。
- 网络/Node 在线；heartbeat 56–308ms 可见（网络负载波动）。
- 无 panic/assert/OOM。

## 5. PC SDL

```bash
cd firmware/terminal/tools/phosphor_sim
make
./build/phosphor_sim                         # 动画窗口；1=Idle 2=Working ESC=退出
SDL_VIDEODRIVER=dummy ./build/phosphor_sim --screenshot build/p3_idle.bmp --state idle --time-ms 3000
SDL_VIDEODRIVER=dummy ./build/phosphor_sim --screenshot build/p3_working.bmp --state working --time-ms 3000
convert build/p3_idle.bmp ../../docs/phosphor/p3_idle.png
convert build/p3_working.bmp ../../docs/phosphor/p3_working.png
```

Golden：

```text
docs/phosphor/p3_idle.png
480x480 PNG, 2,902 B
sha256 f06bb976e77e2aad7c114ba42887f2fefa2ea82f021d9a7ff075358393e6d0fc

docs/phosphor/p3_working.png
480x480 PNG, 3,073 B
sha256 d8be9dc3f26705da8505eddae61f3cf386981c63b378c1365fa5f5a2b7f86099
```

## 6. 构建 / 内存

```text
app size : 1,271,872 B
sha256   : b7c7bec7e6972b97c41a320ed2631cba8646214aafafe4f1d1fa9a7e240777f8
app free : 0x40890 (17%)
DIRAM    : 204,423 / 341,760 B (59.81%)
IRAM     : 16,384/16,384（size 固定窗口；链接断言通过）
PSRAM canvas: 373,248 B（一次分配）
```

真机启动日志 `/tmp/phosphor-p3-boot6.log`：

```text
P3 animation started: demo 6s Idle / 6s Working
welcome memory: internal_free=53,927 psram_free=6,133,656
network state: online
```

后续统计 internal_free 55.9–65.1KB，PSRAM ~6,133,484 B；无持续下降。

## 7. Host 验证

```text
make test          -> 3613 checks, 0 failures
syntax_check.sh    -> 45 files OK
```

覆盖：

- 两套 profile 参数、phase 连续、1s 过渡、energy 低通；
- 同 time/state 可复现（含 `memcmp`）；
- Idle/Working frame 不同、非空、P2/P1.3 回归；
- LUT sin/cos 边界；
- 既有 ramp/palette/compositor/Nucleus/Satellite 测试。

## 8. P4 待办（不开始）

- Completed Pulse、Approval、Error、Offline；
- 六态合成、事件去重、真实 ViewModel 映射（P6）；
- P5 性能优化（Working FPS 目标）；
- 实屏动画与 FPS 人工确认。
