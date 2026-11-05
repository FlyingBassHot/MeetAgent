# ESPUI Theme API（E0 / E1 / E2）

代码入口：`components/espui/theme_api/espui_theme.h`

## 1. 主题生命周期

```c
typedef struct {
    esp_err_t (*init)(const espui_context_t *ctx);
    esp_err_t (*enter_scene)(espui_scene_id_t scene,
                             const espui_view_model_t *model);
    void (*update)(const espui_view_model_t *model);
    void (*on_tick)(uint32_t delta_ms);
    void (*on_input)(const espui_input_event_t *event);
    void (*on_action_result)(const espui_action_result_t *result);
    void (*destroy)(void);
} espui_theme_ops_t;
```

与 `TerminalUi.md` 的签名一致。`ctx` 只在 `init()` 传入，主题自行保存；全局只有一个活动主题，因此单实例状态是显式约定。

### init

- 在 UI Task（启动前是 main task）执行。
- 可以调用 `espui_renderer(ctx)`、`espui_tokens(ctx)`、`espui_display(ctx)`。
- 只能通过 Renderer API 创建 LVGL 对象。
- 失败时必须返回非 `ESP_OK`，并保证 `destroy()` 能清理半初始化资源。
- 不允许访问网络、协议 JSON、Node 客户端。
- 不要在 `init()` 内调用 `espui_request_scene()`；场景进入由 `enter_scene()` 完成。

### enter_scene

- 进入/切换逻辑场景时调用（包括主题切换后的当前场景）。
- 主题应更新场景文案/布局，不得改变 Scene Manager 状态。
- 可读取 ViewModel，但不得修改 ViewModel。

### update

- ViewModel 业务态变化时调用。
- 只做视觉更新；不得发起网络请求、不得创建无界对象。
- 不得修改 ViewModel。

### on_tick

- 由 UI Task 的动画调度器推进，参数为逻辑毫秒（已考虑 tick 间隔）。
- E2 主题为空实现；E3 的 ASCII 动画在这里推进。
- 不允许每帧创建 LVGL 对象；只更新已有对象/字符缓冲区。

### on_input

- 原始输入已经过 Input Router/Navigation；主题收到的是同一份 `espui_input_event_t`，用于焦点视觉反馈。
- 主题不能直接把按键映射成 Agent 命令。
- 需要命令时构造 `espui_action_t`，调用 `espui_dispatch_action(ctx, action)`。

### on_action_result

- 展示命令成功/失败/超时。
- 结果结构：`espui_action_result_t { action, status, message }`。

### destroy

- 释放主题创建的 Scene/Widget/字体/缓冲区。
- 必须允许 `init()` 失败后的调用。
- 不得删除 ESPUI 管理的 ViewModel、Scene Manager 或 Renderer 单例。

## 2. 上下文访问器

`components/espui/core/espui.h`：

```c
const espui_view_model_t *espui_view_model(const espui_context_t *ctx);
const espui_renderer_ops_t *espui_renderer(const espui_context_t *ctx);
const espui_tokens_t *espui_tokens(const espui_context_t *ctx);
const espui_motion_profile_t *espui_motion_profile(const espui_context_t *ctx);
const espui_display_info_t *espui_display(const espui_context_t *ctx);
espui_scene_id_t espui_current_scene(const espui_context_t *ctx);
esp_err_t espui_request_scene(const espui_context_t *ctx, espui_scene_id_t scene);
esp_err_t espui_dispatch_action(const espui_context_t *ctx,
                                const espui_action_t *action);
bool espui_motion_enabled(const espui_context_t *ctx);
uint32_t espui_uptime_ms(const espui_context_t *ctx);
```

约束：

- `espui_request_scene()` 在 UI Task 内直接切换；其他任务会投递事件。
- `espui_dispatch_action()` 只转发抽象动作；网络发送回调必须非阻塞。
- 主题不得调用 `espui_post_*` 等任务侧 API。

## 3. Theme Manifest

代码：`components/espui/theme_api/espui_manifest.h`

```c
typedef struct {
    const char *theme_id;
    const char *name;
    const char *version;
    uint16_t api_version;
    const char *renderer_id;
    espui_display_requirement_t display;
    uint32_t scene_mask;
    uint32_t configurable_mask;
    const char *motion_profile_id;
} espui_manifest_t;
```

示例（与 `themes/terminal_aesthetic/manifest.json` 对应）：

```json
{
  "theme_id": "terminal_aesthetic",
  "name": "Terminal Aesthetic (E2 Mock)",
  "version": "0.2.0-mock",
  "api_version": 1,
  "renderer_id": "builtin.label",
  "display": {
    "orientation": "portrait",
    "min_width": 320,
    "min_height": 480
  },
  "scenes": ["ambient", "focus", "activity", "attention", "settings"],
  "configurable": [
    "accent_color", "brightness", "animation_speed",
    "motion_enabled", "visual_density", "idle_timeout", "night_mode"
  ],
  "motion_profile": "phosphor_default"
}
```

校验规则：

- `theme_id/name/version/renderer_id` 非空。
- `api_version == ESPUI_THEME_API_VERSION`（当前 1）。
- `scene_mask` 至少包含一个合法场景，非法位会被忽略。
- `display.min_width/min_height > 0`，orientation 合法。
- 设备显示方向必须匹配，或 Manifest 声明 `ANY`；`ANY` 允许旋转后满足最小尺寸。
- Renderer 必须已在 Renderer Registry 注册。

`configurable` 配置位：

```text
accent_color / brightness / animation_speed / motion_enabled /
visual_density / idle_timeout / night_mode
```

E2 只保存 Manifest 位；设置持久化在 E5。

## 4. Design Tokens

代码：`components/espui/theme_api/espui_tokens.h`

```c
typedef struct {
    const char *theme_id;
    espui_color_tokens_t colors;
    espui_typography_tokens_t typography;
    espui_motion_tokens_t motion;
    uint8_t density_percent;
} espui_tokens_t;
```

- 颜色使用 `0xRRGGBB`。
- 浮点参数用整数百分比：`1.0 → 100`、`0.3 → 30`。
- 字体字段是资产名，E3 由字体资源解析；E2 Renderer 忽略缩放。
- 安全默认 tokens 黑底白字、无动画；Terminal 默认 tokens 来自 `TerminalUi.md`。

提供的编译期表：

```c
extern const espui_tokens_t espui_tokens_safe_default_values;
extern const espui_tokens_t espui_tokens_terminal_default_values;
```

Manifest 旁放 JSON 仅作为未来数据驱动主题包的对照；E2 不解析 JSON。

## 5. Motion Profile

代码：`components/espui/theme_api/espui_motion.h`

```c
typedef struct {
    const char *id;
    uint16_t speed_percent;
    uint8_t idle_amplitude_percent;
    uint16_t work_speed_percent;
    uint16_t transition_ms;
    uint8_t target_fps;
    bool reduced_motion;
} espui_motion_profile_t;
```

E2 内置：

- `motion_none`：安全主题使用，关闭动画
- `phosphor_default`：Terminal Aesthetic 初始配置，目标 15 FPS，过渡 220ms

Manifest 的 `motion_profile_id` 通过 `espui_motion_profile_find()` 解析；找不到时 runtime 使用默认 profile。真正的六态 motion recipes 在 E3 实现。

## 6. Renderer API

代码：`components/espui/theme_api/espui_renderer.h`

```c
typedef struct {
    esp_err_t (*init)(void);
    void (*deinit)(void);
    esp_err_t (*scene_create)(espui_ui_scene_t **out_scene);
    void (*scene_destroy)(espui_ui_scene_t *scene);
    esp_err_t (*scene_commit)(espui_ui_scene_t *scene);
    espui_ui_widget_t *(*widget_create)(espui_ui_scene_t *scene,
                                        espui_widget_kind_t kind,
                                        const espui_widget_spec_t *spec);
    esp_err_t (*widget_set_text)(espui_ui_widget_t *widget, const char *text);
    esp_err_t (*widget_set_spec)(espui_ui_widget_t *widget,
                                 const espui_widget_spec_t *spec);
    esp_err_t (*widget_set_visible)(espui_ui_widget_t *widget, bool visible);
} espui_renderer_ops_t;
```

E2 提供：

- `builtin.label`：LVGL 8.3 实现，支持 LABEL / RECT、颜色、对齐、显隐。
- Widget 上限 32/Scene；Scene 固定对象，不每帧创建。
- 主题通过 `scene_commit()` 把 screen 设为活动屏幕并 invalidate。
- `font_scale_percent` 已进入 spec，但 E2 不处理。

E3 新增 `builtin.ascii_terminal` 时：

1. 实现同一 `espui_renderer_ops_t`（或扩展后向兼容的字符网格接口）。
2. 在 registry 注册新 id。
3. 把 Terminal Manifest 的 `renderer_id` 改为新 id。
4. 主题选择/切换逻辑不需要改。

## 7. 主题注册与切换

```c
espui_theme_registry_add(&descriptor);
espui_theme_manager_apply(&manager, "terminal_aesthetic",
                          espui_scene_manager_current(&scenes), &model);
```

- 编译期注册，固定上限 8；重复 `theme_id` 拒绝。
- 内置：`safe_fallback`、`terminal_aesthetic`。
- `espui_theme_manager_apply()` 返回请求主题的失败码；即使已回退安全主题，也不会假装成功。
- `last_report` 可查询：`status / reason / manifest_status / error / requested_id / active_id`。
- ViewModel 指针不进入 Theme Manager 的持久状态；切换后由 runtime 重新 `update()`。

## 8. E2 Mock 主题说明

`terminal_aesthetic` 当前使用 `builtin.label` + 终端色板 + 静态 ASCII 核心图形，仅用于验证：

- Manifest / Tokens 生效
- 主题切换 / 回退
- ViewModel 驱动的状态文字
- 安全主题保底

它不是 E3 的动画主题；E3 将替换为字符网格、六态动画和完整场景布局。详见 `components/espui/themes/terminal_aesthetic/README.md`。
