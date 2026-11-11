# PHOSPHOR CORE — R1 状态几何重构（静态六态）

日期：2026-11-11
状态：R1.1 静态六态完成；PC contact sheet 与 host tests 通过；**未烧录 ESP32**。
设备保留 P4b 固件。未进入 motion/烧录/P5/P6。

## 1. 审计：f9a5fed P4b 失败原因

- P4b 使用 `P3.6 base frame + overlay`：Approval/Error/Completed 只是在既有轨道上
  叠加少量 glyph/颜色；远看几乎与 Working 相同。
- Approval 缺口只 0.20 rad，事后清除；宽度不足。
- Error 只 1 个 `!`，没有轨道生成阶段的断裂。
- Completed 只 12 个径向点，900ms，过弱。
- Offline 只 dim/冻结/清卫星，没有轨道解体与 NO LINK。
- Demo 只有 1–2 个 satellite，缺少 state label。

## 2. 新架构

新增 `phosphor_core.c/.h`：

```text
visual state → direct geometry → glyph frame → RGB565
```

- 不再调用 P3.6 scene 再 overlay；
- `phosphor_scene_build_frame_visual()` 直接委托 `phosphor_core_build_frame()`；
- 轨道缺口、Error 断裂、Offline 解体、Completed 波、Satellite、Label
  全部在生成阶段决定；
- 保留纯模块边界：core 不 include LVGL/ESP-IDF/FreeRTOS；
- 保留 P3.6 水层/漂浮物、P3.4 中心减量、P3.3 厚带、24×16/18×27/432/(24,24)、
  三轨 80/140/192、2× `>_`、atlas/palette/rasterizer。

R1 是静态渲染；PC keys 1–6 只切 visual state，不跑 motion。没有 ESP32 烧录。

## 3. 六态独特几何

| State | 直接几何 |
|---|---|
| Idle | 完整三轨，1 个 idle `o`，低密度 |
| Working | 完整三轨 + 外圈 Working 水带/水粒子，4 running `*` |
| Approval | Outer 生成阶段跳过 **1.35 rad**（R1.1 为达 pixel target 进一步扩大）；双边缘 3 层 boundary + 2 `?`；label APPROVAL 2 |
| Error | 对应 slot Outer 直接断开 **1.55 rad**；7 个红色主断裂 + 2 层暗红 shard；无残留 `?`；label ERROR 1 |
| Completed | Outer/Middle 半径收缩 10%；3 层 16/24/32 径向波 + 8 外轨粒子；Nucleus 提亮；label COMPLETED |
| Offline | 轨道交替 cell 解体；其余 dim 35%；NO LINK + LAST SYNC --；label OFFLINE |

Host metrics（R1 core）：

```text
Idle    cells=116 sats=1 gap=4  break=0 waves=0 removed=0
Working cells=124 sats=4 gap=4  break=0 waves=0 removed=0
Approval cells=110 sats=4 gap=42 break=0 waves=0 removed=0
Error   cells=109 sats=4 gap=48 break=7 waves=0 removed=0
Completed cells=147 sats=4 gap=4 break=0 waves=3 removed=0
Offline cells=88  sats=0 gap=4 break=0 waves=0 removed=103
```

## 4. Golden / Contact Sheet

```text
docs/phosphor/r1_contact.png   732×488, 254,998 B
sha256 0752c17eeb07c7519ad9675e11502386d10aaa52346fb1de2a62efdd2cffbf8a
```

六态：

```text
r1_idle.png       3,222 B  ac07edfb...b1066a
r1_working.png    3,501 B  a5e3e729...214455c9
r1_approval.png   2,893 B  02f4de80...c5aaf6
r1_error.png      2,856 B  13f180de...e0bd1a1
r1_offline.png    2,653 B  44fd23cc...3e8d5dd
r1_completed.png  3,762 B  0c198993...b7b9e90
```

Pixel diff vs Working：

```text
Idle        4,104
Approval    8,171
Error       8,558
Offline    19,901
Completed  20,138
```

R1.1 为达到 >8k 目标，Approval/Error gap 比最初 0.75/0.40 更大；
但都是生成阶段几何缺口/断裂，不是 overlay。Contact sheet 中六态几何和 label
可一眼区分。

## 5. 构建 / 验证（不烧录）

- IDF 5.5.5 build：通过。
- 未执行 app-flash；设备保 P4b。
- build-only app：1,284,080 bytes，SHA-256
  `0f65a53f0fff4632b39d5d5c9bc6c46abc0e64e5e1b6fe8382e2a21c7b129d78`
- app free 0x3d810 (16%)；DIRAM 204,623/341,760 (59.87%)。
- Host tests：**3693 checks, 0 failures**；syntax **48 files OK**。

## 6. 未做 / 下一步

- 没有 motion/动画；R2 才考虑 motion/ESP32 flash。
- P5 性能、P6 真实 ViewModel 未开始。
- 需用户先确认 R1 contact sheet 的六态静态几何。
