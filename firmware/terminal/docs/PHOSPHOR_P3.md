# PHOSPHOR CORE — P3.6 Outer 水层 / 漂浮物

日期：2026-11-11
状态：P3.6 Outer 水层/漂浮物；P4 六态已在其上完成。实屏由用户确认。

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
- **P3.3**：删除所有零散外围 0/1 点；不再使用 radius 176 独立副带；
- **P3.3**：对每个主 Outer cell 写朝中心方向的相邻 cell（主轴规则，失败时
  尝试垂直候选）；字符 `0/1/+/%`，FIELD/BAND，Idle 150–170 /
  Working 190–215；不覆盖主 Outer/Middle；
- **P3.3**：Inner 对每个主 cell 写朝外相邻 cell；主 Inner 在 Middle/Outer
  之后绘制；字符 `.,:;` + 少量 `+`，强度 130–170；
- Outer warning gap 缩到 half-width **0.11 rad**；Middle gap 缩到 **0.04 rad**；
  缺口内用低亮度 `0/1/./+` 连续填充，不留大块黑；
- Inner 半径 80、96 采样；Middle 不变；
- **P3.6**：Outer 主带拆成两层，pattern struct len 20：
  - Water 12/20 = 60%：`0 1 - = 1 0 / \ 1 0 = -`，Secondary，intensity 230；
  - Objects 8/20 = 40%：`# @ + o` + `% @ # +` 两个 4-cell packet，
    Highlight，intensity 255；
  - `idx=(i+bucket)%20`，bucket Working 300ms / Idle 800ms；
- **P3.6**：Outer 内侧 band 只做水层：`01-=/`，Secondary，
  不铺 Highlight 物体；
- Middle 改为 Primary（intensity 215）；Inner 保持 Secondary/密集；
  Satellite `*` Highlight / `?` Warning 仍唯一；
- 主 Outer 仍 96 采样；副带/缺口填充连续；
- **P3.4**：core density 收窄到半径 48–64，1/17 cell 稀疏覆盖，
  最高强度约 108–120（Idle/Working）；中心 <80 不生成 band FIELD；
  Inner band 110–150，Middle 强度 215；Outer 完全保持；
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
Idle    fps=7  render_avg=38–80ms  period=142–146ms
Working fps=7  render_avg=72–110ms period=135–136ms
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
SDL_VIDEODRIVER=dummy ./build/phosphor_sim --screenshot build/p3_idle_v7.bmp --state idle --time-ms 3000
SDL_VIDEODRIVER=dummy ./build/phosphor_sim --screenshot build/p3_working_v7.bmp --state working --time-ms 3000
convert build/p3_idle_v7.bmp ../../docs/phosphor/p3_idle_v7.png
convert build/p3_working_v7.bmp ../../docs/phosphor/p3_working_v7.png
```

Golden：

```text
docs/phosphor/p3_idle_v7.png
480x480 PNG, 3,161 B
sha256 e615a7fd4e9de61bbe7bd82606a419acdfa0f4b8618587be341ef3c03a690126

docs/phosphor/p3_working_v7.png
480x480 PNG, 3,432 B
sha256 35f37393bdfd91c609016e919255404eeb86f4aba41ea6dc3d1384c4d6cfa397

历史 P3.0–P3.5 截图保留。
```

## 6. 构建 / 内存

```text
app size : 1,273,312 B (0x136de0)
sha256   : c33dd7171cbd63999ccf284799c1016afc9458db543e1b61b1b6bdb956dc05a7
app free : 0x40220 (17%)
DIRAM    : 204,423 / 341,760 B (59.81%)
IRAM     : 16,384/16,384（size 固定窗口；链接断言通过）
PSRAM canvas: 373,248 B（一次分配）
```

真机启动日志 `/tmp/phosphor-p3g-boot.log`：

```text
P3 animation started: demo 6s Idle / 6s Working
welcome memory: internal_free=49,607 psram_free=6,133,240
network state: online
```

后续统计 internal_free 55.9–65.1KB，PSRAM ~6,133,068 B；无持续下降。

## 7. Host 验证

```text
make test          -> 3624 checks, 0 failures
syntax_check.sh    -> 45 files OK
```

覆盖：

- 两套 profile 参数、phase 连续、1s 过渡、energy 低通；
- 同 time/state 可复现（含 `memcmp`）；
- Idle/Working frame 不同、非空、P2/P1.3 回归；
- LUT sin/cos 边界；
- P3.3 band cell 计数：Idle/Working Inner band=6、Outer band=23；
- changed pixels（Idle/Working）：
  - v2→v3: 1,956 / 2,915
  - v3→v4: **5,740 / 5,510**（明显更大）
- annulus 黑像素比例（v3→v4）：
  Idle Inner 83.7→76.5%，Outer 95.71→94.97%；
  Working Inner 83.93→76.7%，Outer 96.0→95.31%；
- P3.1/P3.2 atlas 0/1、outer/inner 96、Working 0.85 density/速度回归；
- 既有 ramp/palette/compositor/Nucleus/Satellite 测试。

## 8. P4 待办（不开始）

- Completed Pulse、Approval、Error、Offline；
- 六态合成、事件去重、真实 ViewModel 映射（P6）；
- P5 性能优化（Working FPS 目标）；
- 实屏动画与 FPS 人工确认。


P4 已完成：见 `docs/PHOSPHOR_P4.md`（六态 resolver、Demo、goldens）。
