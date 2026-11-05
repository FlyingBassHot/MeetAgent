# Terminal Aesthetic — ASCII Renderer（E3a）

> 范围：E3a 纯逻辑。真正把字符帧画到 LVGL 字符网格是 E3b；本模块不包含 LVGL、FreeRTOS 或 ESP-IDF 依赖。

## 1. 文件

```text
components/espui/themes/terminal_aesthetic/renderers/
├── ascii_renderer.h      # 状态、Motion、Frame、API
├── ascii_renderer.c      # 状态解析、密度场、六态合成、Scheduler 绑定
├── ascii_canvas.h        # 字符画布原语
└── ascii_canvas.c        # put/text/line/ring/box/scanline + 整数 sin/cos
```

主机工具：

```text
tests/host/ascii_preview.c   # stdout 帧预览 CLI
tests/host/test_ascii_renderer.c
```

## 2. 固定字符网格

```c
#define ESPUI_ASCII_MAX_COLS 80
#define ESPUI_ASCII_MAX_ROWS 40

typedef struct {
    uint8_t cols, rows;
    char cells[40][81];        // 每行 NUL 结尾
    uint8_t intensity[40][80]; // 0..255
    uint8_t color[40][80];     // espui_ascii_color_t
} espui_ascii_frame_t;
```

- 默认 60 × 22，适合终端预览；设备目标 320×480、8×16 字体约 40×30。
- 每帧固定约 9.4 KB；`espui_ascii_renderer_t` 自身持有 frame，不每帧分配。
- 行尾始终有 `\0`；文本绘制自动裁剪。
- `color` 是抽象颜色索引：PRIMARY / SECONDARY / MUTED / WARNING / ERROR / HIGHLIGHT。E3b 再映射到 Theme Tokens。

## 3. 字符密度场

默认 glyph ramp：

```text
" .:-=+*#%@"
```

- 强度 0..100 映射到 ramp 索引。
- 核心区域按到圆心的视觉距离计算强度，叠加整数 sine 波纹。
- 背景区域用确定性 hash + 状态密度生成稀疏字符，不使用随机数。
- 所有坐标/时间/状态输入相同 → 帧完全确定，便于快照测试。
- 不使用浮点、不使用 `malloc`。

## 4. ASCII Geometry

`ascii_canvas.c` 提供：

| 原语 | 用途 |
|---|---|
| `canvas_put` | 按强度覆盖写字符 |
| `canvas_text` | 状态行/标题，空格会清除背景 |
| `canvas_line` | 自动选择 `-` / `|` / `/` / `\` |
| `canvas_ring` | 字符环；x 方向 2:1 修正，适配高字符 |
| `canvas_box` | `+` 角、`-`/`|` 边；可留断口 |
| `canvas_scanline` | 横向扫描线，按步长稀疏绘制 |
| `sin/cos` | 64 项 Q7 整数 LUT，无 libm |

Agent Core 合成顺序：

1. 线框（Working/Error/Offline 可带断口）
2. 扫描线（Offline 不绘制）
3. 背景密度场
4. 核心密度场
5. 核心环
6. 轨道（虚线，随 phase 旋转）
7. 辐条（Working 4 条，其他关注态 2 条）
8. 轨道粒子（由 amplitude/density 决定数量）
9. 顶部状态行 + 底部状态摘要

Offline 不绘制扫描线、辐条和粒子，phase 不推进，帧完全静止。

## 5. 六种视觉状态

```c
typedef enum {
    ESPUI_VISUAL_IDLE,
    ESPUI_VISUAL_WORKING,
    ESPUI_VISUAL_COMPLETED,
    ESPUI_VISUAL_APPROVAL,
    ESPUI_VISUAL_ERROR,
    ESPUI_VISUAL_OFFLINE,
} espui_visual_state_t;
```

解析优先级（`espui_ascii_resolve_visual_state`）：

```text
OFFLINE（Node 离线/连接中/connected=false）
  > APPROVAL（待审批数量 > 0，安全优先）
  > ERROR（error_count > 0 或 Agent error）
  > COMPLETED（completion_count > 0）
  > WORKING（Syncing / 活跃 Session / Agent busy）
  > IDLE
```

`scene` 参数保留给 E3b 的场景化布局（Focus/Activity），当前不改变状态优先级。

状态色：

| 状态 | 颜色 |
|---|---|
| Idle / Working | PRIMARY |
| Completed | HIGHLIGHT |
| Approval | WARNING |
| Error | ERROR |
| Offline | MUTED |

## 6. Motion Profile

基础表（`ascii_renderer.c`）：

| State | speed | density | amplitude |
|---|---:|---:|---:|
| Idle | 25% | 25% | 18% |
| Working | 100% | 80% | 70% |
| Completed | 60% | 65% | 55% |
| Approval | 35% | 50% | 25% |
| Error | 20% | 70% | 40% |
| Offline | 0% | 15% | 0% |

与现有 Motion Profile 组合：

```text
speed     = base.speed × profile.speed/work_speed × user_speed
density   = base.density × tokens.density
amplitude = base.amplitude（Idle 再乘 profile.idle_amplitude/30）
reduced_motion = true → amplitude = 0
```

- `espui_motion_profile_none`：全部 speed/amplitude = 0，静态帧。
- `phosphor_default`：E3a 默认，目标 15 FPS。
- `set_user_speed()` / `set_density()` 可运行时调整，不重新分配缓冲。
- Completed 是有限脉冲：`completed_pulse_ms` 内 amplitude 线性衰减到 0；不会无限重复。状态行仍保留 `SESSION COMPLETED`，直到业务状态变化。

## 7. 与 Animation Scheduler / Scene Manager 对接

### Animation Scheduler

```c
espui_animation_scheduler_t scheduler;
espui_animation_scheduler_init(&scheduler);

espui_ascii_renderer_bind_scheduler(&renderer, &scheduler);
// UI Task 每次 tick：
espui_animation_scheduler_tick(&scheduler, delta_ms);
```

- bind 添加一个周期槽：`1000 / target_fps`（15 FPS → 66 ms）。
- Scheduler 的 pause/speed 仍生效；ASCII 自身还会应用状态 Motion。
- `unbind_scheduler()` 或 `renderer_deinit()` 移除槽位。
- 不与 UI Task 的 `espui_animation_scheduler_tick()` 冲突；E3b 在主题内绑定一次。

### Scene Manager

```c
espui_ascii_renderer_apply_view_model(&renderer, &model, current_scene);
```

测试中实际使用了 `espui_scene_manager_set_attention_count()` → 回调 →
`apply_view_model()`，验证 Attention 场景与 Approval 状态同步。

- 主题的 `enter_scene()` 和 `update()` 都可以调用 `apply_view_model()`。
- `apply_view_model()` 会立即重绘一帧；若 E3b 需要降低刷新频率，可改为
  `status_from_view_model + set_status`，由 Scheduler tick 统一 render。

## 8. Host CLI

```bash
cd firmware/terminal/tests/host
make preview STATE=working FRAMES=3 DELAY=300
make preview STATE=all FRAMES=1
make test
```

参数：

```text
ascii_preview [all|idle|working|completed|approval|error|offline] [frames] [delay_ms]
```

输出包含状态、时间、speed/density/amplitude、frame 序号和字符帧。

示例（Working, 60×22 截取）：

```text
=== WORKING  t=0ms  speed=100% density=80% amp=70%  frame=1 ===
  MEETAGENT // WORKING
 +--------------------------------------------------------+
 | - -%- -#- - - - -#-:+**##***=--:-.- - - - - -#- - - -.-|
 |                ::=+++***#**#**=***..:: :          * = :|
 ...
  SYSTEM ACTIVE // 01 SESSION(S)
```

## 9. 未实现（E3b 及以后）

- LVGL 字符网格 widget、字体加载、局部刷新、颜色 Token 映射
- Focus / Activity / Settings 的完整场景布局
- 文本打字/滚动动效
- 真机 FPS、CPU、内存测量
- 主题内实际绑定（当前是 host/API 级集成）

## 10. 约束

- 固定缓冲；无 `malloc`/`calloc`/VLA。
- 无浮点、无 libm、无 LVGL/FreeRTOS 依赖。
- 同一状态 + 同一时间 → 同一帧（确定性）。
- Offline 帧静止；Completed 脉冲有限；Approval 优先于错误。
