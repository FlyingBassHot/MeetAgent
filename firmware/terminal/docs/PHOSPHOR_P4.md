# PHOSPHOR CORE — P4 六态与多维状态组合

日期：2026-11-11
状态：P4 demo/六态实现并真机运行；未进入 P5/P6。实屏由用户确认。

## 1. 保持基线

- 24×16 / 18×27 Bold / 432×432 / (24,24)
- 三轨 80/140/192、P3.6 水层/漂浮物、Inner/Middle/Outer
- 2× `>_`、P3.4 中心减量、P3.3 厚带/缺口填充、P2 ramp/palette/compositor
- 不改 Wi-Fi/WS/Node/app_main 网络路径

## 2. 纯状态解析器

新增 `phosphor_state.c/.h`：

```text
input:
  node_online
  sessions[] (<=8): session_id + status (idle/running/waiting/completed/error)
  approval_count / error_count
  completed_event_id / error_event_id

resolver:
  activity = offline | idle | working
  attention = APPROVAL | ERROR | both
  completion_active + elapsed + event_id
  error_glitch_active + elapsed
  satellites[4] with stable slot
  overflow_count
```

规则：

- `node_online=false` 最高优先级：activity=offline，清 attention/脉冲/glitch；
- 有 running session → working；无 running → idle；
- approval/error 可同时叠加在 working 上，不是单一 enum；
- completion 按 event_id 去重，900ms 有限脉冲；
- error glitch 按 event_id 去重，300ms 一次性；
- satellite 按 session_id 稳定绑定 slot；最多4，多余计入 overflow；
- approval breathing 周期 1750ms，确定性 LUT。

## 3. Demo

`phosphor_demo.c/.h`：

```text
Idle 4s → Working 4s → Working+Approval 4s → Working+Error 4s
→ Completed pulse 2s → Offline 4s → repeat (22s cycle)
```

- 每个 cycle 的 completed/error event_id 唯一；
- ESP32 日志输出 `P4 demo phase=...` 和每5s FPS/内存；
- PC SDL 键：`1` Idle、`2` Working、`3` Working+Approval、
  `4` Working+Error、`5` Offline、`6` Completed；ESC 退出。

## 4. 六态视觉

- **Approval**：Outer 缺口扩大，Warning `?` Satellite 按 1.75s 呼吸；
  running 动画继续；
- **Error**：红色 `!` Satellite，300ms 局部 glitch（相邻 `!`），
  不停止其他 running；
- **Completed**：Nucleus 提亮 + 径向字符波，900ms 后消失；
- **Offline**：整体 dim 35%，卫星移除，motion 冻结（demo 不更新相位）；
- **Idle/Working**：P3.6 动效/密度不变；
- transitions 使用 existing 1s mode_mix；无随机。

## 5. Golden 截图（480×480）

```text
docs/phosphor/p4_idle.png      3,113 B  7385ef68...1daef7
docs/phosphor/p4_working.png   3,498 B  f711179e...5e0d04
docs/phosphor/p4_approval.png  3,426 B  5b827df9...c57ca
docs/phosphor/p4_error.png     3,515 B  168a83a6...f455e3
docs/phosphor/p4_offline.png   3,246 B  3300b835...754c9
docs/phosphor/p4_completed.png 3,524 B  3f8f5e1a...38222
```

命令：

```bash
cd firmware/terminal/tools/phosphor_sim
make
for s in idle working approval error offline; do
  SDL_VIDEODRIVER=dummy ./build/phosphor_sim --screenshot build/p4_$s.bmp --state $s --time-ms 3000
  convert build/p4_$s.bmp ../../docs/phosphor/p4_$s.png
done
SDL_VIDEODRIVER=dummy ./build/phosphor_sim --screenshot build/p4_completed.bmp --state completed --time-ms 300
convert build/p4_completed.bmp ../../docs/phosphor/p4_completed.png
```

## 6. 真机

```text
app size : 1,276,752 B
sha256   : cf2ac8130407ff27cada6ea257b1d92578c405f758efa15675ceaa74c1da7e78
app free : 0x3f4b0 (17%)
DIRAM    : 204,623 / 341,760 B (59.87%)
```

启动日志 `/tmp/phosphor-p4-boot3.log`：

```text
P4 demo phase=idle
P4 demo phase=working
P4 demo phase=working+approval
P4 demo phase=working+error
P4 demo phase=completed
P4 demo phase=offline
network state: online
```

FPS 实测约7 FPS（working/approval/error/offline）；Working 目标 10–15
仍未达到，P5 优化；internal_free 49.4→64.9KB，PSRAM ~6,132,908 B；
无 panic/assert/OOM，Wi-Fi/Node 在线。

## 7. Host 验证

```text
make test          -> 3648 checks, 0 failures
syntax_check.sh    -> 47 files OK
```

覆盖：offline 优先、approval+working、error+working、completed dedupe/
900ms、error glitch 300ms、4 satellites+overflow、稳定 slot、P1/P2/P3.6
回归。

## 8. 未验证 / P5/P6

- 实屏六态视觉（Approval 缺口、Error glitch、Completed pulse、Offline dim）
  待用户确认；
- FPS 目标/内存长期稳定性属 P5；
- 真实 Node/ViewModel 接入属 P6；
- P4 demo 输入仅用于演示，不是生产数据源。
