# PHOSPHOR CORE — P4 六态重构

日期：2026-11-11
状态：P4 六态/多维状态重构完成并真机运行；未进入 P5/P6。实屏由用户确认。

## 1. 基线保持

- 24×16 / 18×27 Bold / 432×432 / (24,24)
- 三轨 80/140/192、P3.6 水层/漂浮物、P3.4 中心减量、P3.3 厚带/缺口
- 2× `>_`、Palette/Ramp/Compositor、`*`/`?` 不再从静态 P3 frame 残留
- 不改 Wi-Fi/WS/Node/app_main 网络路径

## 2. 纯状态解析

新增 `phosphor_state.c/.h`：

```text
activity = offline | idle | working
attention = APPROVAL | ERROR | both
completion: active / elapsed / event_id
error_glitch: active / elapsed
satellites[4] stable slots + overflow
```

- Offline 最高优先级；清 attention/pulse/glitch；
- running>0 -> working；否则 idle；
- Approval/Error 可叠加在 Working；
- Completed 900ms、event_id 去重；
- Error glitch 220ms、event_id 去重；
- Approval breathing 1750ms；
- 4 slots 按 session_id 稳定；overflow 计数。

## 3. Demo

`phosphor_demo.c/.h`，30s cycle：

```text
Idle 6s → Working 6s → Working+Approval 6s → Working+Error 6s
→ Completed 2s → Offline 4s → repeat
```

Demo sessions：

```text
Working : 4 running
Approval: 2 running + 2 waiting
Error   : 2 running + 1 error + 1 idle
Completed: 4 running + completion event
Offline : node_online=false
```

ESP32 日志覆盖 `P4 demo phase=idle/working/working+approval/working+error/completed/offline`。
PC keys：1 Idle / 2 Working / 3 Approval / 4 Error / 5 Offline / 6 Completed。

## 4. 六态几何（不是只换颜色）

- **IDLE**：`IDLE` label，慢速水层，安静；
- **WORKING**：`WORKING` label，4 running satellites（`*`），水流；
- **APPROVAL**：Outer **30–40° 大缺口**（0.65 rad），缺口附近 2 个 Warning `?`，
  1750ms breathing，标签 `APPROVAL 2`；running 继续；
- **ERROR**：对应 slot 的 Outer **5 个红色断裂字符**（`!`/`x`/`-`），
  220ms 局部 glitch，标签 `ERROR 1`；其他 running 继续；
- **COMPLETED**：3 层径向字符波（16/24/32 点）+ 8 个外轨释放粒子，
  Nucleus 提亮，900ms；标签 `COMPLETED`；
- **OFFLINE**：轨道交替 cell 解体 + 其余 dim 35%，清卫星，motion 冻结；
  顶部 `NO LINK`，第二行 `LAST SYNC --`；
- 所有 visual 路径先清空 P3 静态 satellites；Error 且无 approval 不出现 `?`。

## 5. Golden / Contact Sheet

```text
docs/phosphor/p4_idle.png       3186 B  9e9ca10c...bd3346e
docs/phosphor/p4_working.png    3713 B  e2ec6ceb...3b7258fe
docs/phosphor/p4_approval.png   3352 B  ea478f33...cbe639b
docs/phosphor/p4_error.png      4015 B  6a21a6d2...33307c40
docs/phosphor/p4_offline.png    3058 B  4fe9c711...fbc9b4a3
docs/phosphor/p4_completed.png  4427 B  05f81e3f...f39245b
docs/phosphor/p4_contact.png    1464×976, 25,561 B  c8c40b8d...cbf19714
```

Contact sheet 为 3×2，顺序 Idle/Working/Approval/Error/Offline/Completed。

Pixel diff vs Working：

```text
Idle       22,897
Approval    5,144
Error       3,240
Offline    23,464
Completed  25,595
```

Approval/Error diff 集中在缺口/断裂/标签，绝对像素数小于全局变化的
Idle/Offline/Completed，但几何特征明确；不是仅颜色变化。

## 6. 真机

```text
app size : 1,281,328 B
sha256   : 1ad08cf9f6cb93e6ba8c61205beedb4a629f74c89a6300393ba73b08541993f3
app free : 0x3e2d0 (17%)
DIRAM    : 204,623 / 341,760 B (59.87%)
```

启动日志 `/tmp/phosphor-p4b-boot2.log`：

```text
P4 demo phase=idle
P4 demo phase=working
P4 demo phase=working+approval
P4 demo phase=working+error
network state: online
heartbeat RTT=61 ms
```

完整六态 phase 日志见 `/tmp/opencode/phosphor-p4b-boot.log`。
FPS 实测约 7（Idle/Working/Approval/Error）；Working 目标 10–15 仍未达到，
P5 优化；internal_free 49.4→64.9KB，PSRAM ~6,130,004 B；
无 panic/assert/OOM。

## 7. 测试

```text
make test          -> 3683 checks, 0 failures
syntax_check.sh    -> 47 files OK
```

覆盖：offline 优先、approval+working、error+working、completed dedupe/
900ms、error glitch 220ms、4 satellites/overflow/稳定 slot、P1/P2/P3.6
回归、label/atlas 扩展。

## 8. 未验证 / P5/P6

- 实屏六态视觉和 label 仍需用户确认；
- FPS/长期稳定性属 P5；
- 真实 Node/ViewModel 接入属 P6；
- Demo 输入只用于演示，不是生产数据源。
