# PHOSPHOR CORE — P3.2 Idle / Working 密度与二进制流

日期：2026-11-11
状态：P3.2 去掉零散外围点，改为连续 Outer 内侧副带，并提高 Inner 密度；未进入 P4。实屏由用户确认。

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
| Working | +0.25 | -0.62 | +0.36 | 5s | 0.85 | 1.00 |

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
- **P3.2**：删除所有零散外围 0/1 点；
- **P3.2**：Outer 保留半径 192 / 96 采样 / `+#%@01` / warning 缺口；
  新增连续内侧副带：同 tilt/rotation/phase，半径约 176（约 1 cell 内侧），
  96 采样、同 warning 缺口；
- 副带字符 `0/1/+/%`，index+time hash 确定性选择；FIELD 优先级，
  不覆盖主 Outer/Middle；强度 Idle 135、Working 195（低于主带）；
- **P3.2**：Inner 保持半径 80，采样 64→96；字符以 `: ;` 为主，少量 `+`，
  强度 200（180–210），Secondary/Muted；不增加第二圈；
- 0/1 主 Outer / 副带均使用 time bucket；Working 300ms（≈3.3Hz），
  Idle 800ms；主 Outer Working 比例约 45%，Idle 约 12%；
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
Idle    fps=6–7  render_avg=52–85ms  period=141–152ms
Working fps=7    render_avg=72–115ms period=134–137ms
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
SDL_VIDEODRIVER=dummy ./build/phosphor_sim --screenshot build/p3_idle_v3.bmp --state idle --time-ms 3000
SDL_VIDEODRIVER=dummy ./build/phosphor_sim --screenshot build/p3_working_v3.bmp --state working --time-ms 3000
convert build/p3_idle_v3.bmp ../../docs/phosphor/p3_idle_v3.png
convert build/p3_working_v3.bmp ../../docs/phosphor/p3_working_v3.png
```

Golden：

```text
docs/phosphor/p3_idle_v3.png
480x480 PNG, 3,163 B
sha256 8b4c85acc3672e60bed3e65e86cea9bc1df9a0433501ac6cd7b04e5e77a1d2b6

docs/phosphor/p3_working_v3.png
480x480 PNG, 3,062 B
sha256 fd0632bf84a79ce5fcac78bea37ccfbd2b6519d252e7b0e82611fa81b9cc5c0e

历史 P3.0/P3.1 截图保留。
```

## 6. 构建 / 内存

```text
app size : 1,273,152 B (0x136d40)
sha256   : fec4255465d456b6d2823912b605919cf4b00822d658388f0189d6020bfe6573
app free : 0x402c0 (17%)
DIRAM    : 204,423 / 341,760 B (59.81%)
IRAM     : 16,384/16,384（size 固定窗口；链接断言通过）
PSRAM canvas: 373,248 B（一次分配）
```

真机启动日志 `/tmp/phosphor-p3c-boot.log`：

```text
P3 animation started: demo 6s Idle / 6s Working
welcome memory: internal_free=45,207 psram_free=6,133,496
network state: online
```

后续统计 internal_free 55.9–65.1KB，PSRAM ~6,133,324 B；无持续下降。

## 7. Host 验证

```text
make test          -> 3620 checks, 0 failures
syntax_check.sh    -> 45 files OK
```

覆盖：

- 两套 profile 参数、phase 连续、1s 过渡、energy 低通；
- 同 time/state 可复现（含 `memcmp`）；
- Idle/Working frame 不同、非空、P2/P1.3 回归；
- LUT sin/cos 边界；
- P3.1/P3.2 atlas 0/1、outer 96、Inner 96、Working 0.85 density/速度回归；
- 既有 ramp/palette/compositor/Nucleus/Satellite 测试。

## 8. P4 待办（不开始）

- Completed Pulse、Approval、Error、Offline；
- 六态合成、事件去重、真实 ViewModel 映射（P6）；
- P5 性能优化（Working FPS 目标）；
- 实屏动画与 FPS 人工确认。
