# ESPUI 架构（E0 / E1 / E2）

## 1. 分层

```text
┌────────────────────────────────────────────────────┐
│ Theme Implementations                              │
│   safe / terminal_aesthetic (E2 mock + E3a ASCII)  │
├────────────────────────────────────────────────────┤
│ Theme System                                       │
│   Theme API · Manifest · Tokens · Registry         │
│   Renderer API（主题不直接碰 LVGL）                 │
├────────────────────────────────────────────────────┤
│ UI Core                                            │
│   ViewModel · Navigation · Input Router            │
│   Action Dispatcher · UI Event Queue               │
├────────────────────────────────────────────────────┤
│ UI Runtime                                         │
│   Theme Manager · Scene Manager · Animation        │
│   Asset/Memory · Motion Profile · LVGL Renderer    │
│   UI Task                                          │
├────────────────────────────────────────────────────┤
│ HAL                                                │
│   Display · Touch · Input · Power · Board          │
├────────────────────────────────────────────────────┤
│ FreeRTOS / LVGL / ESP-IDF                          │
└────────────────────────────────────────────────────┘
```

目录对应：

| 层 | 目录 |
|---|---|
| Theme API / 公共类型 | `components/espui/theme_api/` |
| UI Core | `components/espui/core/` |
| UI Runtime | `components/espui/runtime/` |
| Theme 实现 | `components/espui/themes/` |
| HAL | `components/espui/hal/` |
| 协议类型 | `components/protocol/` |
| 状态适配 | `components/state/` |
| 网络接口 | `components/networking/` |

## 2. 数据流

```text
Node Snapshot / Event (E4)
        │
        ▼
protocol/meet_protocol.h     固定容量 C 结构
        │
        ▼
state/state_adapter.c        Snapshot → ViewModel
        │  espui_post_view_model()
        ▼
UI Event Queue               FreeRTOS Queue，有界
        │
        ▼
UI Task                      唯一允许操作 LVGL / Theme 的线程
        │
        ├── ViewModel 更新 ──► Theme.update()
        ├── Attention 变化 ──► Scene Manager → Theme.enter_scene()
        ├── Input 事件 ─────► Input Router → Navigation → Theme.on_input()
        └── Action Result ──► Theme.on_action_result()
```

关键规则：

1. 网络/状态任务只投递完整 ViewModel 副本；不直接调用 Theme 或 LVGL。
2. `espui_view_model_copy_business()` 只替换业务态（Node/Agent/Session/Attention），设备态（亮度、动画开关、当前主题）由 UI Core 持有。
3. Theme 只看到 ViewModel；不需要知道数据来自 Snapshot、Event 还是 Mock。

### 2.1 E3a ASCII 帧生成

```text
ViewModel + Scene
      │
      ▼
Visual State Resolver（离线/审批/错误/完成/工作/待机）
      │
      ▼
Motion Profile（speed / density / amplitude）
      │
      ▼
Animation Scheduler（10–15 FPS、暂停、调速）
      │
      ▼
ASCII Renderer（固定字符网格 + 密度场 + 几何）
      │
      ▼
espui_ascii_frame_t（cells / intensity / color）
      │
      ▼
E3b LVGL 字符网格 widget（尚未实现）
```

- 纯逻辑模块位于 `themes/terminal_aesthetic/renderers/`，不依赖 LVGL/FreeRTOS。
- 固定 80×40 上限，默认 60×30（480×480 目标）；默认单帧约 5.5 KB，无每帧分配。
- Completed 使用有限脉冲；Offline phase 不推进，帧静止。
- `apply_view_model()` 负责状态解析并立即重绘；Scheduler 负责定期推进。

## 3. 线程模型

```text
FreeRTOS
├── UI Task (espui.c)
│   ├── 消费 espui_event_t
│   ├── ViewModel / Scene / Theme 生命周期
│   ├── 持有 display HAL lock 调用 Theme + Renderer
│   └── 每 tick 调用 Animation Scheduler 与 Theme.on_tick()
│
├── LVGL port task（板级 BSP / esp_lvgl_port）
│   ├── lv_timer_handler()
│   └── 通过 display HAL lock 与 UI Task 串行化
│
├── State / Mock task
│   └── 生成 ViewModel → espui_post_view_model()
│
├── Network task（E4）
│   └── Wi-Fi / WSS / 心跳 → Snapshot/Event → State Adapter
│
└── Input task（板级）
    └── 旋钮 / 按键 / 触摸 → espui_post_input()
```

- `espui_config_t.ui_task_core = -1` 表示不亲和；默认 8192 字节栈、优先级 5、50ms tick。
- `espui_stop()` 不能在 UI Task 内调用；会等待 UI Task 退出后释放主题与队列。
- Theme 的 `init/update/enter_scene/on_tick/on_input/on_action_result/destroy` 全部在 UI Task 上下文执行（初始化阶段在 main task 中，但此时 UI Task 尚未启动）。
- Renderer 对象只能在 UI Task（或启动前的 main task）创建/销毁。

## 4. 场景与 Attention 优先级

场景：`Ambient / Focus / Activity / Attention / Settings`。

```text
Attention（待审批/异常） > 用户主动导航 > Ambient
```

- `espui_scene_manager_set_attention_count()` 由 ViewModel 的 Attention 计数驱动。
- 计数 > 0 时强制进入 Attention；用户导航返回 `ESP_ERR_INVALID_STATE`，不能隐藏待审批。
- 计数归零后恢复到进入 Attention 前的场景。
- `force()` 仅供系统级设置页使用，会绕过 sticky 规则；主题不得调用。
- Scene 栈有界（8），溢出时丢弃最旧项。
- 切换主题时保留当前场景（包括 Attention），避免切主题把待审批藏起来。这一点比 TerminalUi 中“切换后进入 Ambient”的示意更保守。

## 5. 输入与动作

```text
Encoder CW/CCW   → NEXT / PREV（焦点）
Encoder Press    → ACTIVATE
Select           → ACTIVATE
Back             → BACK
Long Press       → TOGGLE_ATTENTION（有 Attention 时进入）
Touch (pressed)  → ACTIVATE
```

- Input Router 只产生抽象 `espui_nav_intent_t`。
- Navigation 只负责场景级动作：Ambient + ACTIVATE → Focus；Back；显式场景请求。
- Attention 场景中的审批动作必须由主题显式确认；输入映射不会自动批准。
- 主题/UI 产生 `espui_action_t`，统一经过 `Action Dispatcher` → 网络 Command Client。
- Action 执行结果回到 `on_action_result()`，主题只负责展示。

## 6. 主题生命周期与回退

```text
apply(theme_id)
  → registry find
  → descriptor / manifest / display 校验
  → renderer registry find
  → destroy 当前主题
  → 发布 pending theme（供 init 内访问 tokens/renderer）
  → ops.init(ctx)
  → ops.enter_scene(current_scene, vm)
  → ops.update(vm)
  → 成功：active = new，报告 OK
```

失败处理：

| 失败点 | 行为 |
|---|---|
| 未注册 | 回退 safe，报告 `FALLBACK_USED / UNKNOWN_THEME` |
| Manifest 不兼容 | 回退 safe，报告 `MANIFEST` 或 `DISPLAY_UNSUPPORTED` |
| Renderer 未注册 | 回退 safe，报告 `RENDERER_MISSING` |
| init / enter_scene 失败 | `destroy()` 清理后回退 safe，报告 `INIT_FAILED` |
| safe 也失败 | 报告 `NO_FALLBACK`，保持无活动主题 |

- `init()` 失败后 Theme 必须保证 `destroy()` 可安全调用（允许清理半初始化状态）。
- 切换主题不清空 ViewModel；`espui_theme_manager_apply()` 不写业务状态，只更新设备态的 `active_theme_id`（由 runtime 做）。
- `last_report` 保留状态、原因、Manifest 状态、错误码、请求/实际主题，便于设置页和日志展示。

## 7. HAL 边界

- Display：面板信息、`lock/unlock`（板级 LVGL 锁）、背光。
- Touch：逻辑坐标触点读取。
- Input：旋钮/按键原始事件读取。
- Power：电池/充电/屏幕开关。
- Board：把以上接口打包；ESPUI 不包含任何板级引脚。

真实板级 BSP 接入时，只替换 `hal/` 与 `main` 的 board bundle，不修改 Theme 或 UI Core。

LCD-4 V4 当前走官方 BSP：`main/board_lcd4.c` 把
`bsp_display_start / bsp_display_lock / bsp_display_brightness_set /
bsp_display_get_input_dev` 映射到 `espui_board_hal_t`。`espui_board_stub()`
只保留给 host tests；它报告 480×480，但不驱动面板。

## 8. E2 / E3a 的刻意简化

- E2 的 `terminal_theme.c` 仍是 **label 渲染的 Mock**；E3a 的 ASCII 帧生成已独立完成，E3b 再接入 LVGL 字符网格。
- 字体缩放字段存在但 Renderer 暂不处理；E3b 接入字体资源后生效。
- 主题为单实例静态状态；不支持同一主题多实例。
- Settings Storage 尚未实现；Manifest 的 `configurable` 位已经保留。
- Asset / Memory 模块提供有界记账，不实现动态加载或分配器。
- 动画调度器已实现调速/暂停/追帧上限；E3a 的 ASCII Renderer 可以绑定到它，但主题尚未在 UI Task 内绑定。
- ASCII Renderer 的 `scene` 参数保留给 E3b 的场景化布局；当前只影响调用方，不改变状态优先级。
