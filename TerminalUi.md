# MeetAgent ESPUI
## ESP32 UI 基础设施与主题插件系统设计规划书

**项目名称：** MeetAgent ESPUI  
**所属项目：** MeetAgent Terminal  
**架构定位：** 嵌入式 UI Framework + Theme Plugin System  
**首发主题：** Terminal Aesthetic（终端美学）  
**开发平台：** ESP32-S3 + ESP-IDF + FreeRTOS + LVGL  
**目标硬件：** Waveshare ESP32-S3-Touch-LCD-4 V4（480 × 480，GT911 触摸，CH32V003 helper 控制背光/复位/电源）  
**设计原则：** 基础设施先行、主题可替换、数据与表现分离、低资源消耗、持续视觉体验。

---

# 一、项目定位与设计目标

## 1.1 为什么需要独立的 ESPUI

MeetAgent Terminal 不应该只是一组固定的 LVGL 页面。

如果把所有颜色、组件、布局、动画直接写进 Overview、Session、Activity 等页面，那么未来更换视觉方向时，就需要修改大量页面代码。

例如：

- 当前主题采用 ASCII 字符动画。
- 未来想改为像素艺术。
- 再未来想加入科幻仪表盘。
- 用户还希望能够调整配色、字体、动画速度和显示密度。

如果 UI 和具体主题绑定，这些变化会不断侵入原有页面和业务逻辑。

因此，ESPUI 应该成为独立的基础设施：

它管理 UI 生命周期、状态模型、输入事件、页面场景、主题加载、动画调度和资源管理。

具体主题则负责定义如何呈现这些信息。

## 1.2 核心目标

ESPUI 需要实现以下能力：

1. **主题可替换**：更换主题不需要修改 Agent 业务逻辑和通信协议。
2. **主题可配置**：能够修改配色、字体、动画速度、视觉密度等参数。
3. **主题可扩展**：后续可以加入新的主题实现。
4. **动画可复用**：建立统一的动画调度和动效资源管理机制。
5. **状态驱动渲染**：UI 根据真实 Agent 状态产生视觉变化。
6. **硬件解耦**：主题逻辑不直接依赖具体 GPIO、LCD 驱动和硬件型号。
7. **资源可控**：动画、字体和素材受到内存、Flash 和 CPU 预算约束。

## 1.3 本项目不追求的目标

首版不实现：

- 类似桌面操作系统的窗口管理器。
- 用户在设备上直接编辑主题代码。
- 类似浏览器的 HTML/CSS 渲染引擎。
- 任意下载和执行第三方二进制插件。
- 复杂的三维渲染引擎。
- 完整的终端 Shell。
- 主题市场和在线主题商店。

首版重点是建立可长期扩展的基础设施，并完成一个具有代表性的终端美学主题。

---

# 二、核心设计哲学

## 2.1 UI 不只是信息展示，而是设备体验本身

MeetAgent Mobile 的价值主要来自功能效率。

MeetAgent Terminal 则不同。它长期放置在桌面上，即使没有人操作，也应该具有存在感。

因此，它的价值分为两个部分：

**功能价值**

- 感知 Agent 状态。
- 展示重要事件。
- 提供快捷控制。
- 反映 Node 连接情况。

**体验价值**

- 成为桌面环境中的视觉组成部分。
- 通过动画表现系统活动。
- 形成独特的数字美学。
- 通过持续、细腻的动态反馈增加陪伴感。

UI 不能只在用户触摸屏幕后才有价值。

它必须在静置状态下同样成立。

## 2.2 状态应当拥有自己的视觉语言

传统 Dashboard 通常通过文字、数字和卡片展示状态。

ESPUI 应当进一步探索：

- Idle 不只是显示一个灰色的 Idle 标签，而是有自己的平静视觉状态。
- Working 不只是显示一个 Loading 图标，而是通过持续动态体现 Agent 的活动。
- Completed 不只是增加一条日志，而是有一个短暂、明确的完成反馈。
- Approval 不只是增加一个数字，而是让整个画面进入等待用户关注的状态。

也就是说：

**Agent 的业务状态应该能够转化为视觉状态，而不只是文本内容。**

## 2.3 动画应该表达含义，而不是装饰

动画不是为了让屏幕一直动。

每一个重要动画都应该有对应的语义：

- 活跃：工作中的动态。
- 等待：不确定、等待中的动态。
- 完成：短暂的收束和确认。
- 异常：有节制的警示。
- 空闲：低频、安静的呼吸式动态。

避免没有意义的随机闪烁、大面积滚动字符和持续高速粒子效果。

## 2.4 终端美学不是简单复制黑客电影画面

本项目的第一套主题采用 Terminal Aesthetic。

但不希望仅仅做成传统黑底绿字的命令行模拟器。

我们要提取的是终端美学的视觉语言：

- Monospace Typography。
- ASCII Art。
- Glyph-based Graphics。
- CRT / Phosphor Visual Language。
- Cursor 和扫描线。
- 文本化的信息组织。
- 极简状态符号。
- 程序生成的字符动画。

最终目标是：

**一个看起来像具有生命的数字终端，而不是一块静态的开发者监控面板。**

---

# 三、ESPUI 总体架构

## 3.1 架构分层

ESPUI 采用五层结构：

```text
MeetAgent ESPUI
│
├── 1. Hardware Abstraction Layer
│   ├── Display Driver
│   ├── Touch Driver
│   ├── Encoder Driver
│   ├── Button Driver
│   └── Power Interface
│
├── 2. UI Runtime
│   ├── LVGL Integration
│   ├── Theme Manager
│   ├── Scene Manager
│   ├── Animation Scheduler
│   ├── Asset Manager
│   └── Memory Manager
│
├── 3. UI Core
│   ├── ViewModel
│   ├── State Adapter
│   ├── Navigation Manager
│   ├── Input Router
│   ├── Action Dispatcher
│   └── Notification Manager
│
├── 4. Theme System
│   ├── Theme API
│   ├── Theme Registry
│   ├── Theme Manifest
│   ├── Theme Tokens
│   ├── Scene Definitions
│   ├── Motion Recipes
│   └── Theme Assets
│
└── 5. Theme Implementations
    ├── Terminal Aesthetic
    ├── Pixel Art (Future)
    ├── Sci-Fi Instrument (Future)
    └── Minimal Digital (Future)
```

这里需要明确：

硬件层负责画面和输入设备，UI Core 负责业务状态与交互流程，Theme System 负责视觉表现。

主题不应该直接控制 Agent，也不应该直接访问 Node 的网络接口。

## 3.2 各层职责

### Hardware Abstraction Layer

负责隐藏具体硬件差异。

例如：

- 当前 LCD 型号。
- 触摸屏驱动。
- 触摸输入；外接旋钮/编码器输入。
- 按键输入。
- 屏幕方向。
- 亮度控制。

上层只需要调用统一显示和输入接口。

以后换一块 ESP32 开发板，理论上只需要替换硬件适配层，不应该重写主题。

### UI Runtime

这是 UI 系统的运行环境。

负责：

- LVGL 初始化。
- Theme 生命周期。
- Scene 切换。
- 动画调度。
- 素材加载和释放。
- 内存预算。
- 显示刷新。

### UI Core

负责将 MeetAgent 数据转换成 UI 能够消费的信息。

例如 Node 上报：

```json
{
  "node_id": "node-home",
  "status": "online",
  "agents": [
    {
      "agent_id": "agent-001",
      "status": "running",
      "active_sessions": 2
    }
  ]
}
```

UI Core 将其转换成统一 ViewModel。

Theme 不需要知道这个数据来自 WebSocket 还是 Snapshot。

### Theme System

负责主题的注册、配置、生命周期和场景调用。

它是 ESPUI 可扩展能力的核心。

### Theme Implementations

每套主题负责自己的：

- 主视觉布局。
- 场景组织。
- 组件组合。
- 动效编排。
- 色彩与字体。
- 特定的视觉表现。

---

# 四、主题插件系统设计

## 4.1 主题到底应该包含什么

建议将一个主题拆成六个部分：

| 组成 | 职责 |
|---|---|
| Manifest | 主题身份、版本、兼容信息 |
| Design Tokens | 配色、字体、间距、视觉尺寸 |
| Components | 可复用视觉组件 |
| Scenes | 页面和场景定义 |
| Motion Recipes | 状态对应的动画方案 |
| Assets | 字体、图标、位图等资源 |

这意味着主题不仅仅是换色。

主题甚至可以完全改变主界面的布局方式和主要视觉主体。

例如：

- Terminal Aesthetic 使用 ASCII 核心图形。
- Pixel Art 使用像素角色和帧动画。
- Sci-Fi Instrument 使用圆形仪表和动态刻度。

但它们接收的 UI ViewModel 是同一份。

## 4.2 Theme Plugin 的实现机制

这里需要区别两种概念：

**Theme Renderer：主题渲染实现**

负责主题如何构建场景和绘制内容。它包含实际的 C 代码和 LVGL 组件。

**Theme Package：主题资源包**

负责颜色、字体、布局参数、动画参数和素材配置。

推荐采用两阶段设计。

### 第一阶段：编译期主题注册

第一个版本采用编译期注册。

每个 Theme Renderer 实现统一的接口，然后通过 Registry 注册到 ESPUI。

例如：

```text
Theme Registry
│
├── terminal_aesthetic
│
├── pixel_art (future)
│
└── sci_fi_instrument (future)
```

这是最适合 ESP32 首版的实现方式：

- 稳定。
- 方便调试。
- 资源占用可控。
- 不需要在 MCU 上实现复杂的动态代码加载。

### 第二阶段：数据驱动主题包

基础设施稳定后，再支持从 Flash / LittleFS / SD 等资源位置加载主题配置。

主题包可以包含：

- manifest.json
- tokens.json
- animations.json
- assets/
- fonts/

但仍然通过固件中已经注册的 Renderer 来执行渲染。

例如，一个基于 ASCII Renderer 的不同主题包可以只修改字体、颜色和动画参数，不需要编译新的固件。

以后如果需要第三方主题开发者提供完全不同的布局和绘制方式，再通过新的固件 Renderer 扩展。

**不要在 ESP32 上把 Theme Plugin 等同于任意动态执行的 C 代码。**

主题可替换与主题代码动态下载是两件事。

## 4.3 Theme Manifest

建议设计统一的主题清单：

```json
{
  "theme_id": "terminal_aesthetic",
  "name": "Terminal Aesthetic",
  "version": "1.0.0",
  "api_version": 1,

  "renderer_id": "builtin.ascii_terminal",

  "display": {
    "orientation": "any",
    "min_width": 480,
    "min_height": 480
  },

  "scenes": [
    "ambient",
    "focus",
    "activity",
    "attention",
    "settings"
  ],

  "configurable": [
    "accent_color",
    "brightness",
    "animation_speed",
    "motion_enabled",
    "visual_density"
  ],

  "motion_profile": "phosphor_default"
}
```

Manifest 需要支持版本兼容检查。

Theme Manager 在加载主题时检查：

- 主题 ID 是否存在。
- Renderer 是否已经注册。
- API 版本是否兼容。
- 显示尺寸是否满足要求。
- 素材是否存在。
- 配置是否有效。

如果主题加载失败，必须自动回退到一个内置安全主题，不能让设备因为 UI 主题损坏而无法正常进入主界面。

## 4.4 Design Tokens

主题通过 Tokens 控制视觉参数。

例如：

```json
{
  "colors": {
    "background": "#080C0A",
    "surface": "#101813",
    "primary": "#A7F36B",
    "secondary": "#62C79B",
    "muted": "#587364",
    "warning": "#F2B75E",
    "error": "#F06C67"
  },

  "typography": {
    "primary_font": "terminal_mono",
    "display_font": "terminal_display",
    "scale": 1.0
  },

  "motion": {
    "speed": 1.0,
    "idle_amplitude": 0.3,
    "work_speed": 1.0,
    "transition_duration": 220
  }
}
```

上述只是首版视觉配置的初始值。

最终配色和字体需要在真实屏幕上测试，特别是小尺寸字符的可读性。

## 4.5 Theme API

所有主题实现统一生命周期接口。

接口设计示意：

```c
typedef struct {

    esp_err_t (*init)(
        const espui_context_t *ctx
    );

    esp_err_t (*enter_scene)(
        espui_scene_id_t scene,
        const espui_view_model_t *model
    );

    void (*update)(
        const espui_view_model_t *model
    );

    void (*on_tick)(
        uint32_t delta_ms
    );

    void (*on_input)(
        const espui_input_event_t *event
    );

    void (*on_action_result)(
        const espui_action_result_t *result
    );

    void (*destroy)(void);

} espui_theme_ops_t;
```

这个接口的核心思想是让主题只负责视觉和交互表达。

特别注意：

1. `update()` 接收业务状态，不负责向 Node 请求数据。
2. `on_tick()` 负责动画时间推进，不允许无限创建新的 LVGL 对象。
3. `on_input()` 只能产生抽象 UI Intent，不得直接调用 Agent 命令。
4. `on_action_result()` 用于展示成功、失败和等待状态。
5. `destroy()` 必须释放主题私有资源。

命令执行仍然由 UI Core 的 Action Dispatcher 处理。

## 4.6 主题切换流程

```text
User Select Theme
       |
       v
Theme Manager
       |
       v
Check Manifest
       |
       v
Check Compatibility
       |
       v
Release Current Theme
       |
       v
Load New Theme
       |
       v
Initialize Renderer
       |
       v
Apply Current ViewModel
       |
       v
Enter Ambient Scene
```

切换主题不能丢失 Agent 当前状态。

Theme Manager 切换的是表现层，而不是 Node Session 或事件状态。

首版可以通过设置界面切换并在确认后重新初始化主题，不强制要求无缝热切换。

---

# 五、UI Core 与状态模型

## 5.1 为什么需要 ViewModel

ESPUI 不能直接让主题消费原始 Node JSON。

这样会使主题依赖通信协议结构。

例如，Node 将来调整某个 JSON 字段，所有主题都需要跟着修改。

所以需要一个独立 ViewModel 层：

```text
Node Snapshot / Event
          |
          v
     State Adapter
          |
          v
     UI ViewModel
          |
          v
      Theme API
          |
          v
        Renderer
```

Theme 只知道自己应该展示什么，不需要知道数据来自哪里。

## 5.2 状态必须分维度

不能只设计一个：

```c
AGENT_IDLE
AGENT_WORKING
AGENT_ERROR
```

因为未来一个 Node 可能同时存在多个 Agent、多个 Session。

建议将状态分成：

```text
Terminal ViewModel
│
├── Node State
│   ├── Online
│   ├── Connecting
│   ├── Offline
│   └── Syncing
│
├── Agent Collection
│   ├── Agent ID
│   ├── Agent Status
│   └── Session Summary
│
├── Session Collection
│   ├── Session Status
│   ├── Last Activity
│   └── Active Duration
│
├── Attention Queue
│   ├── Approval
│   ├── Error
│   └── Completion
│
└── Device State
    ├── Brightness
    ├── Battery
    └── Theme
```

主题可以根据这些数据自行决定如何呈现。

## 5.3 ViewModel 示例

```c
typedef struct {
    bool connected;
    bool syncing;
    uint32_t last_sync_timestamp;
} espui_node_view_t;

typedef struct {
    char agent_id[40];
    char display_name[32];
    agent_status_t status;
    uint16_t active_session_count;
} espui_agent_view_t;

typedef struct {
    char session_id[40];
    char title[48];
    session_status_t status;
    uint32_t duration_seconds;
} espui_session_view_t;

typedef struct {
    uint16_t approval_count;
    uint16_t error_count;
    uint16_t completion_count;
} espui_attention_view_t;
```

ESP32 不需要保存完整 Session 历史。ViewModel 应采用固定上限和有界内存设计，避免运行时动态分配造成碎片化。

---

# 六、场景系统 Scene Manager

## 6.1 场景定义

第一版 ESPUI 定义五个场景：

| Scene | 定位 | 主要展示 |
|---|---|---|
| Ambient | 默认常驻 | 时间、Agent 总览、动态视觉主体 |
| Focus | 当前关注 | 活跃 Session 和任务状态 |
| Activity | 事件回顾 | 最近事件摘要 |
| Attention | 需要用户关注 | 审批、异常、完成通知 |
| Settings | 设备设置 | 主题、亮度、连接信息 |

Scene 是逻辑场景。

不同主题可以为同一个 Scene 提供完全不同的布局。

例如：

Terminal Aesthetic 的 Ambient 是 ASCII 核心动画；Pixel Art 的 Ambient 可以是一个像素化数字世界。

## 6.2 Scene 与 Navigation 的边界

Navigation Manager 属于 UI Core。

Scene 的内容布局属于 Theme。

这样：

- 触摸滑动（或外接旋钮旋转）可以映射成 Focus 或页面切换。
- 按键可以映射成 Select / Back。
- 长按可以触发全局操作。
- 主题可以定义焦点和视觉反馈，但不能越权执行命令。

---

# 七、首发主题设计：Terminal Aesthetic

## 7.1 主题名称

内部主题名：`terminal_aesthetic`

建议产品名称：**PHOSPHOR**

副标题：`A Living Terminal for MeetAgent`

它是一套以早期计算机终端和磷光显示器为视觉灵感的主题。

设计不是复刻古老 CRT，而是提取终端文化中的核心元素，并以现代嵌入式屏幕重新组织。

## 7.2 设计关键词

- Retro Computing
- ASCII Art
- Phosphor Display
- Monospace Typography
- Procedural Animation
- Minimal Information
- Ambient Computing

## 7.3 视觉风格

背景：

- 近黑色。
- 少量深绿色或暖黑色。
- 不使用大面积渐变作为主体。

主要强调色：

- 磷光绿。
- 辅助青绿。
- 警告琥珀色。
- 错误红色。

主要图形语言：

- ASCII 字符。
- 线框。
- 光标。
- 状态点。
- 字符密度变化。
- 扫描线。
- 字符流动。
- 终端式信息标签。

整体画面应该克制、清晰、有秩序。

不能为了复古效果牺牲可读性。

## 7.4 色彩规范

初始颜色 Token：

| 名称 | 建议颜色 | 用途 |
|---|---|---|
| Background | `#080C0A` | 主背景 |
| Surface | `#101813` | 次级区域 |
| Primary | `#A7F36B` | 主磷光绿 |
| Secondary | `#62C79B` | 次级文字和图形 |
| Muted | `#587364` | 辅助信息 |
| Warning | `#F2B75E` | 审批和等待 |
| Error | `#F06C67` | 异常 |
| Highlight | `#D9FFC3` | 重要信息 |

不要所有文字都使用高亮绿。

通过亮度和透明度建立视觉层次。

## 7.5 字体规范

建议使用两类字体：

**Terminal Mono**

用于主要状态文字、标签、时间、Session ID 和事件信息。

**Terminal Display**

用于大号数字、标题和状态标识。

字体要求：

- 等宽。
- 小尺寸字符清晰。
- 区分 `0/O`、`1/I/l`。
- 能够显示必要的 ASCII 字符。
- 控制字体文件大小。

首版不追求庞大的 Unicode 字符库。

优先使用稳定的 ASCII 字符集，避免字体缺字或 Flash 空间浪费。

## 7.6 UI 的核心视觉主体

不要把终端做成一屏日志。

应该有一个稳定的视觉主体：**Agent Core**。

它是一个抽象的 ASCII 动态核心，代表 MeetAgent 正在感知和连接的 Agent 集合。

建议由以下元素组成：

- 中央核心图形。
- 周围的字符轨道。
- 状态字符。
- 运动的字符流。
- 少量连接线和刻度。

概念示意：

```text
       .     :      .
          .--+--.
      .-==  CORE  ==-.
     /   .-######-.   \
    |   :## <> ##:    |
     \   '-######-'  /
      '-==------==-'
          .  .
     / / /     \ \ \
```

实际实现不应依赖整张静态图片，而要使用字符和几何图形动态生成。

这样才有真正的状态驱动能力。

---

# 八、Terminal Aesthetic 的动画实现逻辑

这是首发主题最重要的技术部分。

## 8.1 动画系统的总体思路

动画系统应该分成三层：

```text
Agent Business State
          |
          v
Visual State Resolver
          |
          v
Motion Profile
          |
          v
Animation Scheduler
          |
          v
ASCII / Glyph Renderer
          |
          v
LVGL Display
```

### Business State

来自 Node 的真实状态。

例如：

- Agent Idle。
- Agent Running。
- Session Completed。
- Approval Required。
- Agent Error。
- Node Offline。

### Visual State Resolver

把业务状态映射成抽象视觉状态。

例如：

```text
Agent Running
     |
     v
Visual: ACTIVE
     |
     v
Motion: FLOW
```

### Motion Profile

控制动画的节奏、密度和强度。

### ASCII Renderer

使用字符和简单几何元素生成动态画面。

## 8.2 首发动画集合

### A. Idle：待机动画

视觉效果：

- 中央字符核心缓慢变化。
- 少量字符缓慢出现和消失。
- 光标以低频节奏闪烁。
- 周围只有少量活动。

设计理念：

空闲不是设备停止工作，而是系统处于安静、等待的状态。

动画应该低频、柔和，不打扰桌面环境。

### B. Working：运行动画

视觉效果：

- ASCII 核心出现周期性变化。
- 字符沿轨迹移动。
- 中心区域密度增加。
- 少量数据字符从边缘流入中心。

设计理念：

让 Agent 的工作变成可以感知的视觉活动。

但动画不能假装显示真实的模型 Token、思考过程或计算进度。

它只是系统活动的抽象表现。

### C. Completed：完成动画

视觉效果：

- 中央核心短暂收束。
- 亮度出现一次柔和脉冲。
- 显示完成事件摘要。
- 最后回到 Idle。

设计理念：

用户不需要看屏幕，也能感知一次工作已经结束。

完成动画不能无限重复。

### D. Approval：等待审批动画

视觉效果：

- 核心状态转为琥珀色。
- 动态速度下降。
- 显示明确的待审批数量。
- 进入 Attention 场景。

设计理念：

让用户明确感知 Agent 正在等待人工决策。

如果有多个请求，显示队列数量和当前请求信息。

### E. Error：异常动画

视觉效果：

- 使用红色强调。
- 字符结构发生有限变化。
- 边框出现状态标识。
- 显示简短错误摘要。

避免不断闪烁和强烈频闪。

### F. Offline：断线动画

视觉效果：

- 字符核心变暗。
- 运动停止或降速。
- 出现断开的 ASCII 连接符号。
- 明确显示 `NODE OFFLINE`。

离线时不得继续播放看起来像真实 Agent 正在工作的动画。

## 8.3 ASCII 动画的程序生成方法

首版重点采用程序生成，而不是制作大量逐帧 GIF。

推荐结合三种实现技术：

**字符密度场（Glyph Density Field）**

根据字符网格中每个位置的坐标和动态参数，计算视觉强度，并映射成字符。

例如：

```text
" .:-=+*#%@"
```

低强度使用空格和点，高强度使用密度更高的字符。

抽象计算：

```text
intensity = calculate_field(x, y, time, state)

glyph_index = map_range(
    intensity,
    glyph_ramp_length
)

output[x][y] = glyph_ramp[glyph_index]
```

这样能够生成：

- 呼吸式核心。
- 动态波纹。
- 字符密度变化。
- 流动的抽象图形。

**ASCII Geometry**

通过简单二维几何算法生成：

- 圆环。
- 轨道。
- 线框。
- 波形。
- 旋转扫描线。
- 粒子位置。

再将几何结果映射成字符。

例如：

- 接近圆环的字符使用 `.`。
- 高亮区域使用 `+`、`#`。
- 连接线使用 `|`、`/`、`\`。
- 运动轨迹使用 `:` 和 `-`。

**Text-Based Motion**

专门用于：

- 光标闪烁。
- 字符逐步出现。
- 状态行更新。
- 文本滚动。
- 事件提示。
- 模拟终端输出。

不要所有动画都用逐字符打字效果。

文本打字动画只应用于少数具有表达价值的状态转换。

## 8.4 动画参数化

动画必须受到主题参数控制。

例如：

```json
{
  "motion_profile": {
    "idle": {
      "speed": 0.25,
      "density": 0.2,
      "amplitude": 0.15
    },

    "working": {
      "speed": 1.0,
      "density": 0.8,
      "amplitude": 0.7
    },

    "approval": {
      "speed": 0.35,
      "density": 0.5,
      "amplitude": 0.25
    },

    "error": {
      "speed": 0.2,
      "density": 0.7,
      "amplitude": 0.4
    }
  }
}
```

具体数值后续需要通过实机测试调整。

用户设置可以调整整体动画速度，甚至关闭非必要动画。

## 8.5 动画实现约束

- 不允许每一帧重新创建完整页面。
- 主题初始化时创建固定 LVGL 对象。
- 动画只更新必要的字符区域和图形对象。
- 动画调度不阻塞网络任务。
- 不在 UI 线程执行耗时计算。
- 不持续全屏高频刷新。
- 对动画帧率和最大执行时间进行测量。
- 网络断开时，动画自动进入对应的离线状态。

首版以局部区域动态刷新为主。

---

# 九、Terminal Aesthetic 首屏 UI 设计

目标硬件使用 480 × 480 方形屏作为首版布局基准（Waveshare ESP32-S3-Touch-LCD-4）。

方形屏不再沿用旧的 320×480 竖屏比例，首屏需要重新组织视觉中心；ASCII Core、状态区和底部信息区都要按 480×480 重新分配，而不是把旧布局简单拉伸。

设计需要具有明确的视觉中心，而不是把整个屏幕平均划分成若干卡片。

## 9.1 Ambient 主界面

建议分成四个区域：

```text
┌──────────────────────────┐
│ MEETAGENT // NODE-01     │
│ 19:28             ● LAN  │
├──────────────────────────┤
│                          │
│       ASCII CORE         │
│                          │
│        .-====-.          │
│      .' +####+ '.        │
│     /  :##<>##:  \       │
│     \   '++++'  /        │
│      '--------'          │
│                          │
│      SYSTEM ACTIVE       │
│                          │
├──────────────────────────┤
│ AGENTS      SESSIONS     │
│ 02 ONLINE   03 ACTIVE    │
│                          │
│ 1 APPROVAL REQUIRED      │
├──────────────────────────┤
│ [NODE] [FOCUS] [EVENTS]  │
└──────────────────────────┘
```

这只是区域关系示意，实际设计需要根据字体、字号、安全边距和实机显示效果重新调整。

四个区域：

**顶部 Header**

- MeetAgent 标识。
- Node 名称。
- 时间。
- LAN 连接状态。

**中心 Visual Core**

- ASCII 动态主体。
- Agent 活动表现。
- 主要状态文字。

这是视觉占比最大的区域。

**底部 Information Strip**

- Agent 数量。
- Session 数量。
- 待处理事件数量。
- 重要状态摘要。

**底部 Navigation**

- Overview。
- Focus。
- Activity。

不要让导航占用过多屏幕空间。

## 9.2 Focus Scene

当用户触摸主要视觉主体（或外接旋钮旋转）时，进入 Focus Scene。

显示：

- 当前关注的 Agent。
- Session 名称。
- 状态。
- 已持续时间。
- 最近一次事件。
- 可用操作。

重点仍然保留 ASCII 主题的视觉表达。

## 9.3 Activity Scene

展示精简事件时间线：

```text
19:24:31 SESSION START
19:24:35 TOOL EXECUTION
19:25:02 FILE UPDATED
19:25:41 SESSION DONE
```

首版只保留有限条数。

不试图在小屏幕上展示完整 Agent 日志。

## 9.4 Attention Scene

优先显示：

- Approval 请求。
- Agent Error。
- 任务完成提醒。

审批详情如果超出屏幕能够清晰呈现的范围，必须通过 Mobile 完成完整审核。

终端不能因为主题追求美观而隐藏安全信息。

---

# 十、交互设计与主题关系

## 10.1 输入设备

第一版交互方式（ESP32-S3-Touch-LCD-4 没有板载旋钮）：

- Touch：主要交互方式，点击选择、滑动切换、长按确认。
- Back：屏幕返回区域或板载按键。
- Interrupt：屏幕快捷按钮或板载按键组合。
- 板载 BOOT / RESET / BAT_PWR：可作为调试、复位和电源操作。
- Encoder CW / CCW：保留抽象事件，后续需要时外接旋钮编码器。

实际 GPIO、按键数量和外接输入以开发板原理图及后续硬件设计为准。

## 10.2 输入抽象

硬件输入先转换为 UI Core 的标准事件：

```text
Encoder CW
Encoder CCW
Select
Back
Long Press
Touch
```

UI Core 将其转换为抽象导航和操作意图。

Theme 可以决定焦点怎样呈现、选中时怎样动画，但不能决定任意命令的权限。

## 10.3 全局 Attention 优先级

场景切换需要有优先级。

建议：

```text
Safety / Permission Context
          |
          v
Critical Error
          |
          v
Approval Required
          |
          v
Important Completion
          |
          v
User Focus
          |
          v
Ambient
```

这里不是所有事件都必须强制抢占屏幕。

例如审批请求可以通过小型提醒提示用户，再由用户主动进入详情。

主题可以定义视觉优先级和提示方式，但事件的安全级别由 UI Core 决定。

---

# 十一、工程结构设计

建议独立建立 ESPUI 子目录：

```text
meetagent-terminal/
│
├── main/
│
├── components/
│
│   ├── espui/
│   │   │
│   │   ├── core/
│   │   │   ├── espui.c
│   │   │   ├── view_model.c
│   │   │   ├── navigation.c
│   │   │   ├── input_router.c
│   │   │   └── action_dispatcher.c
│   │   │
│   │   ├── runtime/
│   │   │   ├── theme_manager.c
│   │   │   ├── scene_manager.c
│   │   │   ├── animation_scheduler.c
│   │   │   ├── asset_manager.c
│   │   │   └── memory_manager.c
│   │   │
│   │   ├── theme_api/
│   │   │   ├── espui_theme.h
│   │   │   ├── espui_manifest.h
│   │   │   ├── espui_scene.h
│   │   │   └── espui_tokens.h
│   │   │
│   │   ├── themes/
│   │   │   └── terminal_aesthetic/
│   │   │       ├── manifest.json
│   │   │       ├── terminal_theme.c
│   │   │       │
│   │   │       ├── scenes/
│   │   │       │   ├── ambient.c
│   │   │       │   ├── focus.c
│   │   │       │   ├── activity.c
│   │   │       │   └── attention.c
│   │   │       │
│   │   │       ├── animations/
│   │   │       │   ├── idle.c
│   │   │       │   ├── working.c
│   │   │       │   ├── completed.c
│   │   │       │   └── error.c
│   │   │       │
│   │   │       ├── renderers/
│   │   │       │   ├── ascii_renderer.c
│   │   │       │   ├── geometry_renderer.c
│   │   │       │   └── text_renderer.c
│   │   │       │
│   │   │       └── assets/
│   │   │
│   │   └── hal/
│   │       ├── display.c
│   │       ├── touch.c
│   │       └── input.c
│   │
│   ├── networking/
│   ├── protocol/
│   ├── state/
│   ├── events/
│   └── commands/
│
└── tests/
    ├── theme/
    ├── animation/
    ├── view_model/
    └── integration/
```

目录是逻辑组织建议，可以根据现有 ESP-IDF Component 结构调整。

---

# 十二、技术实现规范

## 12.1 LVGL 作为基础渲染引擎

LVGL 负责：

- 控件绘制。
- 字体渲染。
- 局部动画。
- 输入设备管理。
- 屏幕刷新。

ESPUI 不需要自行实现完整图形引擎。

但主题不应该直接在任何位置随意操作 LVGL 对象。

应通过统一的 UI Runtime 和 Theme API 管理对象生命周期。

## 12.2 ASCII Renderer

ASCII Renderer 是首发主题的核心组件。

它接收：

- 字符网格宽高。
- 字体配置。
- 当前视觉状态。
- 动画参数。
- 时间步长。

输出：

- 字符缓冲区。
- 字符亮度或颜色索引。
- 局部绘制区域。

它不直接访问 Agent 数据。

## 12.3 Animation Scheduler

动画调度器负责统一推进各个动画。

设计要求：

- 由统一时钟推进。
- 按区域更新。
- 允许暂停。
- 支持主题级速度配置。
- 支持低功耗降速。
- 状态切换时终止不再适用的动画。

第一版目标是以约 10–15 FPS 驱动主要 ASCII 动画区域，具体以实机渲染性能为准；静态文字、时间和状态不必每帧刷新。

## 12.4 内存管理

目标设备具有 PSRAM，但仍然应该控制内存使用。

重点：

- UI 对象固定创建，尽可能复用。
- 限制字符帧缓冲区数量。
- 避免动画每帧动态分配内存。
- 图片素材优先使用压缩存储和按需加载。
- 字体只加载主题需要的字符集。
- 主题切换必须释放旧资源。
- 事件队列和 ViewModel 必须设置容量上限。

## 12.5 线程模型

```text
FreeRTOS
│
├── UI Task
│   ├── LVGL Handler
│   ├── Theme Update
│   └── Animation Scheduler
│
├── Network Task
│   ├── Wi-Fi
│   ├── HTTP
│   └── WebSocket
│
├── State Task
│   ├── Snapshot
│   ├── Event Processing
│   └── ViewModel Update
│
├── Input Task
│   ├── Encoder
│   ├── Touch
│   └── Buttons
│
└── Power Task
    ├── Brightness
    └── Idle / Sleep
```

所有 LVGL 对象更新都应通过统一线程安全机制或 UI Task 执行，不能直接在网络回调中操作 LVGL。

Network Task 不允许因为屏幕刷新或动画而被阻塞。

---

# 十三、主题配置与用户设置

ESPUI 应当提供统一的设置管理，而不是每个主题实现一套独立配置页面。

第一版配置：

| 配置项 | 描述 |
|---|---|
| Active Theme | 当前主题 |
| Accent Color | 主题强调色 |
| Animation Speed | 动画速度 |
| Motion Enabled | 动画开关 |
| Brightness | 屏幕亮度 |
| Visual Density | 视觉密度 |
| Idle Timeout | 空闲时间 |
| Night Mode | 夜间亮度模式 |

主题可以声明支持哪些配置项。

Theme Manager 读取 Manifest 后生成可用配置列表。

用户设置持久化在独立的 ESPUI Settings Storage 中。

不要将用户设置直接写入主题资源文件。

---

# 十四、开发阶段与验收计划

本项目以六个阶段实施。

## E0：ESPUI 架构定义

目标：确立 UI 基础设施边界。

任务：

- 建立 ESPUI 分层。
- 定义 Theme API。
- 定义 Manifest。
- 定义 ViewModel。
- 定义 Scene Manager。
- 定义 Animation Scheduler。
- 确定主题注册机制。

验收：

- Theme 不依赖 Node 网络接口。
- UI Core 不依赖具体主题。
- 不同主题使用相同 ViewModel。
- 主题切换不会清除业务状态。

交付：

- ESPUI Architecture。
- Theme API Specification。
- ViewModel Specification。

## E1：基础 UI Runtime

目标：先让 ESPUI 正常工作。

任务：

- ESP-IDF 工程集成。
- ESP32-S3-Touch-LCD-4 V4 板级 BSP：优先复用官方 `waveshare/esp32_s3_touch_lcd_4`，封装 RGB LCD、GT911 触摸、CH32V003 复位/背光。
- 480×480 LVGL 初始化与显示刷新。
- UI Task。
- 输入事件抽象（触摸为主，按键辅助）。
- Theme Registry。
- 默认安全主题。

验收：

- 能正常显示 480×480 基础 UI。
- LCD 颜色、方向正确；GT911 触摸坐标正确。
- 输入事件能够正常响应。
- 主题可以初始化和销毁。
- 出现主题加载错误时能回退默认界面。

交付：

- ESPUI Runtime MVP。

## E2：主题框架

目标：建立完整主题生命周期。

任务：

- Theme Manifest 解析。
- Theme Manager。
- Theme Tokens。
- Scene Manager。
- Motion Profile。
- Theme Settings Storage。

验收：

- 能够注册至少两个 Mock Theme。
- 能够切换主题。
- 主题切换后业务状态不丢失。
- 不兼容主题能够被拒绝加载。

交付：

- Theme System MVP。

## E3：Terminal Aesthetic 实现

目标：实现首个正式主题。

任务：

- 字体和色彩规范。
- ASCII Renderer。
- 字符网格。
- ASCII Geometry。
- Idle 动画。
- Working 动画。
- Completed 动画。
- Approval 动画。
- Error / Offline 动画。
- Ambient、Focus、Activity、Attention 场景。

验收：

- 所有主要状态均有对应视觉表达。
- 动画与状态变化同步。
- 无持续无意义闪烁。
- 主要动画区域达到预期流畅度。
- 长时间运行无明显内存增长和卡顿。

交付：

- Terminal Aesthetic V1.0。

## E4：真实 Node 状态接入

目标：使视觉表现由真实 Agent 事件驱动。

任务：

- Node Snapshot 接入。
- Event Stream 接入。
- ViewModel 更新。
- 事件优先级处理。
- 多 Agent / 多 Session 汇总。
- 状态过期处理。

验收：

- Agent Running 能触发 Working 动画。
- Approval 能触发 Attention。
- Completed 能触发完成反馈。
- Node Offline 能触发离线表现。
- 断线重连后 UI 正确恢复。

交付：

- MeetAgent Terminal UI V1.0。

## E5：主题配置与优化

目标：完成可配置和可长期使用的桌面体验。

任务：

- 动画速度设置。
- 配色设置。
- 亮度设置。
- 夜间模式。
- 主题切换。
- 长时间运行测试。
- 内存与 CPU 性能优化。

验收：

- 可以修改主题配置。
- 设置持久化。
- 设备长时间运行稳定。
- 低活动状态下不会浪费不必要的渲染资源。
- 主题切换不会导致 UI 堆积或资源泄漏。

交付：

- ESPUI V1.0。
- Terminal Aesthetic Theme V1.0。

---

# 十五、视觉体验验收标准

ESPUI 的验收不应只有功能测试。

终端美学主题需要建立独立的视觉验收标准。

### 视觉辨识度

用户在不仔细阅读文字的情况下，能够通过主视觉判断 Agent 大致处于什么状态。

### 桌面美观度

设备在静置状态下仍然具有观赏性，不应该像一块持续滚动的监控终端。

### 状态真实性

所有动态状态都应该有真实的业务事件作为依据，不能把模拟动画伪装成 Agent 的真实进度。

### 信息层次

正常运行状态低干扰，审批和异常状态高可见性。

### 动画品质

动画具有连续性、节奏感和方向性，避免突然跳变和高频闪烁。

### 交互体验

触摸与板载按键具有可预测的行为，用户不需要每次都查看说明；外接旋钮时复用同一套输入抽象。

### 稳定性

长时间运行时，主题不能不断创建对象、累积内存或阻塞通信。

---

# 十六、风险与设计约束

| 风险 | 解决方案 |
|---|---|
| 主题过度依赖 LVGL 内部细节 | 统一 Theme API |
| 主题切换内存泄漏 | 生命周期管理和资源释放测试 |
| 动画导致刷新负担过大 | 局部刷新和统一调度 |
| 字体占用过大 | 精简字符集和字体资源 |
| 字符动画与 Agent 状态脱节 | ViewModel 驱动 |
| 多 Agent 状态过于复杂 | UI Core 提供汇总和 Focus 模型 |
| 主题扩展导致固件复杂 | 首版编译期注册 |
| 主题资源加载失败 | 内置安全回退主题 |
| 美观牺牲可读性 | 视觉和功能分别验收 |
| UI 与网络任务耦合 | 单独 UI Task 和事件队列 |

---

# 十七、首版交付边界

首版不要求完成完整的主题商店、动态代码加载或多主题市场。

V1.0 应当达到：

**ESPUI Core**

- Theme Manager。
- Scene Manager。
- ViewModel。
- Input Router。
- Animation Scheduler。
- Asset Manager。
- Settings Storage。

**Theme Infrastructure**

- Theme API。
- Manifest。
- Design Tokens。
- Motion Profile。
- Theme Registry。
- 主题切换。
- 资源生命周期管理。

**Terminal Aesthetic Theme**

- ASCII Renderer。
- ASCII Geometry。
- Monospace Typography。
- Ambient Scene。
- Focus Scene。
- Activity Scene。
- Attention Scene。
- 六类状态动画。

---

# 十八、最终设计决策

MeetAgent ESPUI 采用：

**基础设施层：**

负责 UI 生命周期、状态、场景、输入和资源。

**主题系统：**

负责主题注册、主题配置、组件、动画与资源。

**主题实现层：**

使用统一 Theme API 提供不同视觉体验。

**首发主题：**

Terminal Aesthetic，以 ASCII 字符、磷光效果和程序生成动画为主要视觉表达。

最终关系：

```text
                   MeetAgent Node
                         |
                         v
                   UI ViewModel
                         |
                         v
                    ESPUI Core
                         |
              ┌──────────┴──────────┐
              |                     |
         Scene Manager        Input Router
              |                     |
              └──────────┬──────────┘
                         |
                    Theme Manager
                         |
              ┌──────────┴──────────┐
              |                     |
         Theme Registry       Animation Runtime
              |                     |
              └──────────┬──────────┘
                         |
                Terminal Aesthetic
                         |
                  ASCII Renderer
                         |
                       LVGL
                         |
                      Display
```

**ESPUI 最重要的设计理念是：数据和交互逻辑属于系统，视觉表达属于主题。**

MeetAgent 只需要告诉 ESPUI 当前有哪些 Agent、正在发生什么、哪些事件需要关注。

ESPUI 不应该规定这些信息必须以卡片、列表或仪表盘的形式呈现。

Terminal Aesthetic 只是第一种表达方式。未来新的主题可以使用完全不同的视觉语言，而不需要改造 Node、通信协议和核心状态管理。
