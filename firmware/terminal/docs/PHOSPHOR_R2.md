# PHOSPHOR R2：state-first core + motion

状态：R2 motion 已实现并烧录；实机 demo 连续跑满 2 个完整周期，Offline 后恢复 Idle。
未 commit/push；未进入 P5/P6。

## 1. 架构

R2 继续使用 R1.1 的 direct state-first core，没有 base+overlay：

```text
phosphor_visual_state_t + phosphor_motion_state_t
        -> phosphor_core_build_frame_motion()
        -> glyph_frame -> RGB565
```

motion 只改 phase / radius / intensity / glitch / pulse，不重建几何：

- `phosphor_scene_build_frame_visual()` 把 motion 传给 core；
- 静态 `phosphor_core_build_frame()` 仍可用（motion=NULL）；
- 不引入 overlay、不引入第二套 geometry。

## 2. Motion 参数

| 项 | Idle | Working |
|---|---:|---:|
| 轨道速度 inner | +0.08 rad/s | +0.22 rad/s |
| 轨道速度 middle | -0.045 rad/s | -0.55 rad/s |
| 轨道速度 outer | +0.025 rad/s | +0.32 rad/s |
| breath | 8.0 s | 5.0 s |
| density | 0.30 | 0.85 |
| brightness | 0.82 | 1.00 |

- mode transition：**600 ms**（0.3–0.9 s 内）；
- delta 截断：**200 ms**；
- sin/cos：`phosphor_lut` 1024 点 LUT；
- core 用 phase 转动三轨/水粒子，用 breath 做低幅亮度呼吸；
- Completed 另用 elapsed 驱动收缩和波半径；Approval/Error 不靠高频闪。

## 3. 六态行为

- **Idle**：三轨按 0.08/-0.045/0.025 转动，8 s 亮度呼吸；完整轨道；1 个 `o`。
- **Working**：0.22/-0.55/0.32；更高密度水层/漂浮物；4 个 running `*`；水带随 outer phase 移动。
- **Approval**：R1.1 大缺口保持 **1.35 rad**；缺口边缘 2 `?` + 3 层 boundary；
  `?`/边缘亮度按 **1.75 s** breath 缓慢变化；running 轨道继续；无高频闪。
- **Error**：进入后 **220 ms** glitch：断裂段整体偏移 0.12 rad + 额外红 `!`；
  之后稳定 7 红字 `! x - ! x - !` + 暗红 shards；不持续抖动；无 `?`。
- **Completed**：**900 ms** 一次性 pulse；Middle/Outer 半径 **0.96→0.90**；
  3 层径向波 16/24/32 随 elapsed 扩张；8 个外轨粒子；Nucleus 提亮；
  波强度 `230→20` 淡出；event_id 去重，不无限循环。
- **Offline**：进入后最多 **550 ms** 以 Idle 速度降速，然后冻结 motion；
  轨道 cell 解体、dim 35%、NO LINK + LAST SYNC --；恢复后从冻结相位继续，
  下一帧回到 Idle/Working，不卡死。

## 4. Demo

- 4 sessions：Working 4 running；Approval 2 running+2 waiting；
  Error 2 running+1 error+1 idle；
- 30 s 六态循环：Idle 6 / Working 6 / Approval 6 / Error 6 / Completed 2 / Offline 4；
- PC keys `1..6` 切 Idle/Working/Approval/Error/Offline/Completed；
- state label 保持 `IDLE / WORKING / APPROVAL 2 / ERROR 1 / OFFLINE / COMPLETED`。

## 5. PC 资产

固定时间六态（`--time-ms 3000`，Completed 300）：

```text
docs/phosphor/r2_contact.png      732×488, 309,540 B
sha256 e08637419361a22b313c6ffb1f559a544034f4a721c669532b302bd55bd7a3aa
```

六态 PNG：

```text
r2_idle.png       0b7ab972...fe435d7
r2_working.png    e56bd971...e121a7d55
r2_approval.png   d19eb067...78fa7b1
r2_error.png      80e3bfcb...4ade21b0
r2_offline.png    fad328df...f3cb1d6a
r2_completed.png  2f0c4422...676db17e
```

动画采样：

```text
r2_working_motion.gif   11 帧 / Working phase 动画
r2_completed_pulse.gif   9 帧 / Completed 900 ms 收缩+波
```

固定时间 pixel diff vs Working：

```text
Idle        18,672
Approval     6,990
Error        7,910
Offline     21,580
Completed   25,101
```

Approval/Error 的静态几何仍是 R1.1 的 1.35/1.55 rad gap 与 7 红字；
此处 diff 受 3 s 时 phase 位置影响，不作为 R1.1 的验收指标。

## 6. ESP32 烧录 / 实机

- IDF 5.5.5 build：通过；
- **app-flash 已执行**（0x10000，保留 NVS）；写入
  1,284,512 B，esptool hash verified；
- app size 0x1399a0 = **1,284,512 B**，free 0x3d660 (16%)；
- DIRAM 204,623 / 341,760 (59.87%)；IRAM 16,384 / 16,384；
- app SHA-256：
  `9afad977762063315d546a6224695f40a692bed9991ea1258a3531d84e42c2bf`。

启动/运行（70 s 抓取，`/tmp/opencode/phosphor-r2-boot.log`）：

```text
STA 172.20.10.2 -> Node reachable -> websocket connected -> online
I (1737)  phase=idle
I (7869)  phase=working
I (13805) phase=working+approval
I (19833) phase=working+error
I (25748) phase=completed
I (27758) phase=offline
I (31823) phase=idle        <- 第一次 Offline 恢复
...
I (57839) phase=offline
I (61750) phase=idle        <- 第二次 Offline 恢复
I (67878) phase=working
```

- 2 个完整 30 s 周期；两次 Offline→Idle 恢复；
- 无 panic / Guru / abort / OOM / watchdog / brownout；
- Wi-Fi/Node/WS 在线；
- `internal_free` 约 55.7 KB → 64.9 KB，`psram_free` 6,129,748 B；
- FPS：Idle 6–7，Working 7；Working 目标 10–15 **仍未达到**，留 P5。

已知启动 warning（非本次引入）：i2c pull-up 提示、ST7701 36h/3Ah 覆盖提示。

## 7. 验证

- Host tests：**3708 checks, 0 failures**；
  - phase 连续 / 三轨速度；
  - 600 ms transition；
  - Completion pulse 生命周期 + 900 ms 结束 + event_id 去重；
  - Error glitch 220 ms 一次性；
  - Approval 1.75 s breath；
  - Offline freeze / recovery；
  - R2 core motion frame 差异。
- syntax：**48 files OK**；
- `git diff --check` clean；
- 未 commit/push；未改 Node/mobile/根文档；未调 Wi-Fi。

## 8. 待用户确认 / 下一步

- 实机六态 + 动画需用户目视确认；当前设备正在跑 demo；
- Working 7 FPS 留 P5，不虚报；
- P5 性能、P6 真实 ViewModel 未开始。
