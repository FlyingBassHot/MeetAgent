# PHOSPHOR R2.1：Approval/Error 状态主导字符 + 单一主体色

状态：R2.1 已烧录；实机跑满 2 个 30s demo 周期；未 commit/push；未进入 P5/P6。

## 1. 规则

Approval/Error 在 R1.1 state-first 几何的绘制末尾执行状态主导 pass：

- Approval：
  - 除顶部 label 行和 2× `>_` Nucleus 外，所有非空 cell 改为 `?`；
  - 所有非空 cell 主体色统一为 Warning `0xF5AB`（#F2B75E）；
  - 保留 1.35 rad 缺口；边缘 `?` + 1.75s 呼吸；running 继续；
- Error：
  - 除 label 行和 Nucleus 外，所有非空 cell 改为 `!`；
  - 所有非空 cell 主体色统一为 Error `0xF36C`（#F06C67）；
  - 保留 1.55 rad 断裂 + 红色 shard；220ms glitch 后稳定；
- 没有 base+overlay；只在 core 最终绘制阶段改 glyph/color/intensity；
- Idle/Working/Completed/Offline 的像素与 R2 逐像素一致（diff 0）。

## 2. 转换动画

`phosphor_state` 新增 approval/error attention 进入计时：

```text
approval_elapsed_ms / error_elapsed_ms
CORE_ATTENTION_TRANSITION_MS = 450 ms
```

- 每个非保护 cell 用 `((col*13 + row*7) % 17)/17` 阈值；
- 0→450ms 内 body glyph/color 逐 cell 扫过，不是瞬间切；
- 450ms 后所有非空 cell（含 label/Nucleus 的 color）为单一主体色；
- Approval 完成后整场按 1.75s breath 做 intensity 呼吸；
- Error 的 220ms glitch 与 450ms 转换重叠，之后稳定红色，不持续抖。

## 3. Host 统计

`phosphor_core_metrics` 新增 `attention_cells / body_glyph_cells / body_color_cells`。

稳态（demo t=600/1000ms）：

```text
Approval  attention=110  ?=99   90.0%  Warning=110/110
Error     attention=109  !=101  92.7%  Error=109/109
```

- label 行和 2× `>_` 保留，因此 `?`/`!` 不是 100%，但 **远超 70%**；
- Error 中 `?` = 0；Approval 中 `!` = 0；
- 主体色只有 Warning / Error，无 Primary/Secondary/Highlight 混色。

转换采样：

```text
Approval t=0 ?9/110, t=300 ?71/110, t=600 ?99/110
Error    t=0 !10/109, t=300 !73/109, t=600 !101/109
```

## 4. 截图 / Contact / GIF

```text
docs/phosphor/r2b_contact.png   732×488, 330,083 B
sha256 39057174ffdfa39976456a8220e72ca9824b72f81c9815c461da7e0dc811453a
```

```text
r2b_approval.png   7ecfd0de...ff61f24c
r2b_error.png      680607d9...9b95129
r2b_idle.png       44bd1264...57095fde
r2b_working.png    3074f113...9c6d7818
r2b_offline.png    a2c6cae4...a95d3f32f
r2b_completed.png  1e2bcc77...ec64c21e
```

动画采样：

```text
r2b_approval_motion.gif   9 帧，0–1000ms 转换 + 呼吸
r2b_error_motion.gif      9 帧，glitch + 转换
```

固定时间 pixel diff vs Working：

```text
Idle        18,672
Approval    23,893
Error       20,973
Offline     21,580
Completed   25,101
```

## 5. 实机烧录 / 运行

- IDF 5.5.5 build 通过；
- app-flash 0x10000 / NVS preserved；Wrote **1,285,136 B**，hash verified；
- app SHA-256：
  `22685bdc9324f19706730f3857f0e37bf9c3706a39774e3f52180ae0f205f217`；
- app free 0x3d3f0 (16%)；DIRAM 204,639 / 341,760 (59.88%)；
  IRAM 16,384 / 16,384。

70s 抓取 `/tmp/opencode/phosphor-r2b-boot.log`：

```text
STA 172.20.10.2 -> Node reachable -> websocket connected -> online
idle 1.7, working 7.8, approval 13.8, error 19.7,
completed 25.9, offline 27.9, idle 31.8,
... offline 57.8, idle 61.9, working 67.9
```

- 2 个完整周期；两次 Offline→Idle；
- Wi-Fi/Node/WS 在线；heartbeat RTT 16–59ms；
- 无 panic/Guru/abort/OOM/watchdog/brownout；
- internal_free 55,695→64,935 B；psram_free 6,129,748 B；
- FPS：Idle 6–7，Working 7；Working 10–15 目标仍未达，留 P5。

## 6. 验证 / 边界

- Host tests：**3723 checks, 0 failures**；
  - Approval/Error 稳态占比 ≥70%；
  - 转换早期占比低于 70%，600ms 后达标；
  - body color 覆盖全部非空 cell；
  - Warning/Error RGB565 精确值；
  - glitch/pulse/offline 旧测试保持。
- syntax 48 files；`git diff --check` clean；
- 未 commit/push；未改 Node/mobile/根文档；未调 Wi-Fi；未进 P5/P6。

## 7. 待确认

- 实机 Approval/Error 视觉由用户确认；设备正在跑 R2.1 demo。
- Working 7 FPS 留 P5，不虚报。
