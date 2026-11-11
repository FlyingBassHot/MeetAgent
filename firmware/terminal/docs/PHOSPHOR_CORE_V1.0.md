# MeetAgent ESPUI — PHOSPHOR CORE 开发规格书

**文档版本：** 1.0  
**目标对象：** Coding Agent / ESP32 固件开发者  
**所属项目：** MeetAgent Terminal / ESPUI  
**首发主题：** `terminal_aesthetic`  
**核心视觉组件：** `phosphor_core`  
**目标硬件：** Waveshare ESP32-S3-Touch-LCD-4，4 英寸 480×480 RGB LCD，ESP32-S3，8MB PSRAM，16MB Flash  
**软件基线：** ESP-IDF + FreeRTOS + LVGL（必须沿用仓库当前已锁定的版本）

---

# 0. 给 Coding Agent 的执行说明

本文件是 PHOSPHOR CORE 的实现规格，不是自由发挥的视觉灵感文档。实现时应优先遵循布局、结构、状态语义、动画规律和验收标准。

## 0.1 首要目标

实现一个由 ASCII/Glyph 字符构成的动态数字核心，作为 MeetAgent Terminal 480×480 屏幕中的中央主视觉。它必须：

1. 静置时具有观赏性，不能像普通 Loading 图标。
2. 能通过形态、运动、字符密度和颜色表达 Agent 状态。
3. 同时支持多个 Session，不允许将全局状态简单压缩成单个 `IDLE/WORKING/ERROR` 枚举。
4. 使用程序生成的几何与字符渲染，不使用预渲染 GIF/视频作为主体。
5. 与 OpenCode 原生事件解耦，通过 MeetAgent ViewModel 输入状态。
6. 可在 Linux PC 模拟器中运行和调试，再移植至 ESP32 实机。
7. 不阻塞网络任务，不在动画帧中反复分配内存，不持续创建 LVGL 对象。

## 0.2 严格执行规则

开始写代码前，必须先检查当前仓库，并输出简短审计结果：

- ESP-IDF 版本；
- LVGL 主版本及精确版本；
- 当前板卡 BSP / `esp_lvgl_port` 版本；
- 当前屏幕分辨率、方向、色彩格式；
- 当前 LVGL 初始化、刷新和触摸输入路径；
- PSRAM 配置及可用堆空间；
- 当前工程是否已有 UI Task、事件队列和 ViewModel。

**不得**：

- 未经确认升级 LVGL 主版本；
- 重写已经正常工作的 LCD / Touch / BSP 初始化；
- 在网络回调中调用 LVGL API；
- 把 OpenCode 事件类型直接写进通用 Renderer；
- 把 PHOSPHOR CORE 写成一个巨型页面文件；
- 每帧创建/销毁大量 LVGL Label；
- 将动画做成 GIF 播放器；
- 用随机闪烁代替真实状态表现；
- 伪造 Agent 的 Token 速度、思考进度或执行百分比；
- 在没有实测的情况下宣称 FPS 或内存指标已经达标。

## 0.3 实施方式

按本文第 12 节阶段实施。每完成一个阶段：

1. 编译；
2. 运行 PC 模拟器或实机；
3. 保存截图/录屏；
4. 对照验收条件自检；
5. 输出变更文件列表、测量数据和未完成项；
6. 不得跳过失败验收直接进入下一阶段。

如果现有仓库结构与本文建议目录不同，保留既有工程风格，但必须保持本文定义的模块边界和依赖方向。

---

# 1. 产品与视觉定位

## 1.1 产品定位

MeetAgent Terminal 是一个仅存在于统一局域网中的桌面 Agent 状态展示设备。它主要承担：

- 长时间常驻展示 Agent 工作状态；
- 通过视觉动画让用户快速感知 Agent 的活动情况；
- 在审批、异常、任务完成时提供清晰但不刺眼的提醒；
- 通过触摸进入少量辅助场景。

它不是完整 Coding Agent 客户端，不是手机替代品，也不是密集型监控 Dashboard。

## 1.2 首发视觉主题

主题 ID：`terminal_aesthetic`  
核心组件 ID：`phosphor_core`  
视觉概念：**由字符构成、具有状态语义的数字核心。**

设计关键词：

- Retro Computing，但不是低质量 CRT 滤镜；
- ASCII / Glyph Geometry；
- Phosphor Green；
- 三层不完整轨道；
- 中心 `>_` Nucleus；
- Session Satellite；
- Procedural Motion；
- Ambient Computing。

## 1.3 设计哲学

PHOSPHOR CORE 不是一个只会变色的图标，也不是一个普通旋转球体。

- **中心 Nucleus**：代表 MeetAgent 所连接的 Agent Runtime，中心 `>_` 是稳定识别符号。
- **三条 Glyph Orbit**：代表 Session 活动、事件流和 Agent 工作循环。
- **Session Satellites**：代表有限数量的活跃/等待/异常 Session。
- **字符密度**：表现结构强弱、能量聚集和空间层次。
- **颜色**：作为次级编码，辅助识别状态。
- **运动方式**：是首要状态编码。不同状态必须具有不同的形态或运动规则。

视觉表现只能反映系统真实状态。不得把不确定信息绘制成确定的 Agent 进度。

---

# 2. 屏幕布局规格

## 2.1 屏幕基线

- 逻辑分辨率：480×480；
- 屏幕方向：以当前板卡实际配置为准，首版 UI 目标为正向方屏；
- 色彩：优先 RGB565；
- 安全边距：左右至少 20px；
- UI 背景：`#080C0A`。

不要通过修改 BSP 旋转参数来迁就 UI。先确认实际屏幕方向和触摸坐标映射。

## 2.2 Ambient 总体区域

首版采用以下参考布局，坐标均为逻辑屏幕像素，允许在实机验证后进行小幅调整，但不得破坏视觉层级。

| 区域 | 坐标 / 尺寸 | 职责 |
|---|---|---|
| Header | x=22, y=16, w=436, h=34 | MeetAgent / OpenCode、Node 连接状态 |
| Core Canvas | x=96, y=72, w=288, h=288 | PHOSPHOR CORE 主动画 |
| 分隔线 | x=24, y=376, w=432 | 主视觉与状态数据分隔 |
| Metrics | x=24, y=390, w=432, h=36 | Running / Waiting / Completed 数量 |
| Last Event | x=24, y=438, w=432, h=22 | 最近事件摘要 |

Header 示例：

```text
MEETAGENT // OPENCODE                         ● LAN
```

Metrics 示例：

```text
RUNNING 02       WAITING 01       COMPLETED 04
```

Last Event 示例：

```text
LAST EVENT                         TOOL EXECUTED
```

约束：

- 不要将 Ambient 做成卡片 Dashboard；
- 中央 Core 必须占据主要视觉注意力；
- Header 与 Metrics 字体不得比 Core 更抢眼；
- 首版不固定显示底部 Tab Bar；
- 辅助信息只展示摘要，不展示完整 Prompt、工具参数和日志。

## 2.3 Core Canvas 坐标

Core Canvas 为独立 288×288 RGB565 绘制区域。

- 屏幕位置：`(96, 72)`；
- Canvas 中心：`(144, 144)`；
- 对应屏幕中心：`(240, 216)`；
- 背景透明不可用时，画布底色统一填充 `#080C0A`；
- 外轨必须留出至少 8px 的安全空间，不能贴边裁切。

该区域建议使用 36×24 字符网格，每个字符单元为 8×12px：

- Grid width：36 columns；
- Grid height：24 rows；
- Cell width：8px；
- Cell height：12px；
- Raster size：288×288px；
- RGB565 单帧 buffer：288×288×2 = 165,888 bytes，约 162KiB。

字符单元非正方形。几何投影必须考虑 8:12 的字符宽高比，不能直接把字符坐标当作等比例像素坐标。

---

# 3. PHOSPHOR CORE 视觉构成

## 3.1 组成层级

由内向外严格分成四层：

1. **Nucleus**：中心字符核心；
2. **Inner Orbit**：内层密度环；
3. **Middle Orbit**：中层运动字符轨道；
4. **Outer Orbit**：外层扫描轨道及 Session Satellite。

辅助元素：少量刻度、极弱的背景字符点、事件节点。

不允许把整个主体画成三条完全同心的普通圆线。轨道应具有不同倾角、不同椭圆投影和不同相位，以形成 2.5D 的空间感。

## 3.2 Nucleus 中心核心

中心必须保留 `>_` 识别符号，但不要只显示一个巨大的 `>_` 文本。

推荐结构：

- 中央 2–4 个字符单元宽的 `>_`；
- 周围有一圈低密度字符核心壳；
- 核心壳根据状态改变密度、亮度和局部结构；
- 核心中心坐标固定，不随动画漂移；
- 任何状态下中心识别符号都不能被轨道或卫星遮挡。

## 3.3 三条轨道

三条轨道使用不同的几何参数：

| 轨道 | 角色 | 半径建议（屏幕像素） | 运动特点 |
|---|---|---:|---|
| Inner | 核心边界 / 内部活动 | 48–62 | 低幅度、与核心相联 |
| Middle | 主要字符流 | 78–96 | Working 时运动最明显 |
| Outer | Session 节点 / Attention | 104–120 | 卫星节点所在轨道 |

以上是初始设计范围，不是板卡机械尺寸。最终必须在 288×288 Canvas 中限制边界。

几何建议：

- Orbit A：近水平椭圆投影；
- Orbit B：向右上倾斜的椭圆投影；
- Orbit C：向左上倾斜的椭圆投影；
- 三条轨道的相位与角速度独立；
- 至少一条轨道允许局部断开；
- 深度方向可通过字符亮度、密度和遮挡近似表现。

建议使用三维圆环的二维正交投影，或等价的二维旋转椭圆参数化方式。首版不引入真实 3D 引擎。

## 3.4 Session Satellites

卫星节点用于表达少量关键 Session，而不是完整显示所有 Session。

- 最多直接渲染 4 个节点；
- 超过 4 个时，剩余数量通过 `+N` 或 Metrics 表达；
- 每个节点必须拥有稳定的状态颜色与形态标识；
- 不要只使用颜色区分节点；
- 节点的身份应与 Session ID 稳定绑定，避免每帧随机换位置。

建议符号：

| Session 状态 | 字符形态 | 颜色 |
|---|---|---|
| Running | `*` 或 `+` | Primary |
| Waiting / Approval | `?` | Warning |
| Error | `!` | Error |
| Completed（短暂） | `.` / `+` | Highlight |
| Idle | `o` | Muted |

同一 Session 在状态不变时应保持轨道位置稳定。位置变化只能由明确的排序或状态变更触发，并应平滑过渡。

---

# 4. 颜色与字符规范

## 4.1 首版调色板

| Token | HEX | 用途 |
|---|---|---|
| Background | `#080C0A` | 主背景 |
| Surface | `#101813` | 次级背景 |
| Primary | `#A7F36B` | 主磷光绿 |
| Secondary | `#62C79B` | 次级绿色 |
| Muted | `#587364` | 低亮度字符 |
| Warning | `#F2B75E` | Approval / Waiting |
| Error | `#F06C67` | Error |
| Highlight | `#D9FFC3` | 核心高亮 / 完成反馈 |

必须通过统一 Palette Token 管理，禁止散落硬编码颜色。

RGB565 转换由统一函数实现。若板卡 RGB/BGR 配置与颜色显示不一致，应修正显示配置或统一颜色映射，不得在主题各处打补丁。

## 4.2 亮度层级

初始建议：

- 低亮度字符：约 15–25% 的主色亮度；
- 中亮度字符：约 40–55%；
- 高亮字符：约 70–80%；
- Highlight：短暂接近 90–100%。

视觉比例初始目标：70% 低/暗字符、20% 中亮字符、10% 高亮字符。此比例是视觉调色指导，不是每帧必须硬性满足的像素计数。

## 4.3 Glyph Ramp

首版使用稳定 ASCII 字符，不依赖扩展 Unicode 字符。

推荐亮度密度 Ramp：

```text
 .,:-=+*#%@
```

字符方向与几何符号：

```text
.-_=+*/\\|<>^v:;[]{}
```

要求：

- Ramp 字符必须来自同一套 Bitmap Glyph Atlas；
- 映射必须单调：字符视觉密度随亮度增加；
- 字符不能出现字体缺字；
- 不要混用宽度不一致的 Unicode 字符；
- 不能用 `M`、`W` 等宽字符假设为单格，必须经过字体图集验证。

## 4.4 字体图集

首版推荐自带 8×12 或 8×14 的 1bpp Bitmap Glyph Atlas。

要求：

- 字体等宽；
- 字符边缘清晰；
- `0/O`、`1/I/l` 区分明确；
- 仅包含首版实际使用的 ASCII 字符；
- 图集为只读资源，放 Flash；
- Rasterizer 在绘制时将 1bpp 字形写入 RGB565 buffer。

不要为了 CRT 感而使用难以辨认的像素字体。首版重点是字符几何清晰和密度层次。

---

# 5. 业务状态模型与视觉状态解析

## 5.1 不允许使用单一全局 Agent 状态

一个 Node 可以同时拥有多个 Session，可能出现一个 Session 正在工作、另一个 Session 等待审批、第三个 Session 异常的情况。

所以 Visual State 必须是多维数据，而不是简单枚举。

参考数据结构：

```c
typedef enum {
    CONN_ONLINE,
    CONN_SYNCING,
    CONN_STALE,
    CONN_OFFLINE
} connection_state_t;

typedef enum {
    SESSION_IDLE,
    SESSION_RUNNING,
    SESSION_WAITING,
    SESSION_COMPLETED,
    SESSION_ERROR,
    SESSION_UNKNOWN
} session_visual_status_t;

typedef struct {
    char session_id[40];
    session_visual_status_t status;
    uint32_t last_activity_ms;
    uint8_t stable_slot;       /* 0..3，用于卫星位置稳定映射 */
} phosphor_session_t;

typedef struct {
    connection_state_t connection;
    uint8_t session_count;
    uint8_t running_count;
    uint8_t waiting_count;
    uint8_t completed_count;
    uint8_t error_count;
    phosphor_session_t sessions[8];
    uint32_t last_event_id_hash;
    bool completion_event;
    uint32_t completion_event_time_ms;
} phosphor_input_state_t;
```

这是实现参考。若仓库已有统一 ViewModel，应由适配层转换，不得重复维护第二份业务真相。

## 5.2 状态维度

至少拆成：

- **Connectivity**：Node Online / Syncing / Stale / Offline；
- **Activity**：是否有 Running Session、活跃数量；
- **Attention**：Approval、Error 数量；
- **Transient Event**：刚刚完成的 Session 事件；
- **Session Collection**：最多 8 个摘要，直接可视化最多 4 个。

## 5.3 状态合成规则

规则优先级不是简单互斥关系，而是多层合成：

1. `CONN_OFFLINE`：覆盖整个主体运动；所有轨道冻结或逐渐解体，禁止继续表现 Working。
2. `CONN_STALE`：保留最后已知结构，但降低亮度，并显示数据陈旧提示。
3. `error_count > 0`：对对应 Satellite 使用 Error 形态；如果其他 Session 仍在运行，核心可以继续运行。
4. `waiting_count > 0`：外轨出现 Attention 缺口和琥珀节点；不要求停止其他 Running Session 的运动。
5. `running_count > 0`：核心进入 Working 基础运动。
6. 无 Running、无 Attention、无 Error：核心进入 Idle。
7. `completion_event`：在当前活动状态之上叠加一次 Completed Pulse，不能覆盖 Offline 或审批状态。

重要组合：

- Working + Approval：核心继续运转，外轨出现琥珀色等待节点；
- Working + Error：核心继续运转，异常 Session 对应节点红色断裂；
- Idle + Approval：主体整体安静，Attention 节点缓慢呼吸；
- Offline：所有活跃视觉停止，并显示最后同步时间；
- Working + Completed Pulse：完成脉冲短暂出现，随后继续 Working 或进入 Idle。

## 5.4 状态真实性

- Activity 强度只表示平台观察到的活动程度；
- 不得显示虚假的 Token/s、模型思考百分比或任务完成百分比；
- 如果数据未知，使用 Unknown / Stale，而不是猜测；
- Node 离线后显示最后已知时间，不得将缓存状态标记为实时状态。

---

# 6. 六类状态的具体视觉与动画规格

所有动画使用统一的 `motion_time` 和连续相位，不得在每帧重新随机生成相位。

下列时间为起始调参范围，实机评审后允许调整，但视觉语义必须保留。

## 6.1 IDLE

**几何：** 三条轨道完整，整体相对对称；核心结构稳定。  
**颜色：** Primary + Secondary + Muted。  
**运动：**

- 核心呼吸周期 6–10 秒；
- Inner Orbit 缓慢转动；
- Middle Orbit 极慢移动；
- 外轨仅有少量字符粒子；
- 不应产生大量同时运动的字符。

建议初始角速度：

- Inner：`+0.08 rad/s`；
- Middle：`-0.045 rad/s`；
- Outer：`+0.025 rad/s`。

呼吸幅度限制在核心亮度/密度的轻微变化，不应造成整个主体明显放大缩小。

## 6.2 WORKING

**几何：** 轨道仍完整，但字符密度和活动粒子增加，表现向心与环流。  
**颜色：** Primary + Highlight 少量点亮。  
**运动：**

- 中轨成为主要运动层；
- 字符粒子沿轨道流动；
- 部分字符从外层进入核心附近，再从另一段轨道释放；
- 核心亮度和密度比 Idle 高；
- 活动增加时提高运动强度，但设定上限。

建议初始角速度：

- Inner：`+0.22 rad/s`；
- Middle：`-0.55 rad/s`；
- Outer：`+0.32 rad/s`。

工作强度可以根据真实事件活动更新，但必须使用平滑低通滤波，避免每收到一个工具事件就突然加速。

建议 `activity_energy` 范围 0.0–1.0。它是 UI 活动强度指标，不是 Agent 计算进度。

## 6.3 COMPLETED

**触发：** 收到 Session Completed 事件时触发一次性动画。  
**持续时间：** 800–1200ms。  
**表现：**

1. 核心环短暂收束；
2. Nucleus 亮度上升；
3. 一道径向字符波向外扩张；
4. 字符粒子沿外轨释放；
5. Highlight 逐渐淡出；
6. 恢复当前真实 Activity 状态。

完成动画不得无限循环。重复事件必须根据 event ID 去重，避免同一个完成事件反复播放。

## 6.4 APPROVAL

**几何：** Outer Orbit 出现明显缺口，缺口附近固定一个或多个 Warning Satellite。  
**颜色：** Warning + Muted Green。  
**运动：**

- 不停止其他 Running Session 的核心运动；
- Warning Satellite 以 1.5–2.0 秒周期柔和呼吸；
- 轨道缺口边缘缓慢亮度变化；
- 不使用高频闪烁。

必须同步显示待审批数量。触摸进入 Attention 后，展示足够的审批摘要；如果屏幕无法呈现安全决策所需上下文，则只提供“在 Mobile 查看/处理”的引导，不允许盲目批准。

## 6.5 ERROR

**几何：** 对应 Session Satellite 的轨道段出现断裂或局部错位。不要无差别破坏整个核心。  
**颜色：** Error + 暗红 + 少量 Muted。  
**运动：**

- 进入 Error 时可以产生一次 150–250ms 的轻微 Glitch；
- Glitch 仅影响局部字符或对应卫星节点；
- 随后停留在稳定红色异常形态；
- 不持续抖动、不全屏闪烁。

如果同时存在 Running Session，其它轨道可以继续运行。

## 6.6 OFFLINE

**几何：** 轨道从完整形状变成离散字符片段；核心结构失去连续性。  
**颜色：** 低亮度灰绿。  
**运动：**

- 进入 Offline 时在 300–600ms 内降速并冻结；
- 不再产生新的 Working 粒子；
- 核心显示 `NO LINK` 或 `NODE OFFLINE`；
- 显示 Last Sync 时间；
- 可保留极低频静态扫描，但不能像正常运行一样活动。

如果只是 Stale 而非 Offline，应保留最后已知结构并显著降低亮度，不要直接进入完全离线形态。

---

# 7. 技术实现架构

## 7.1 总体渲染链路

```text
MeetAgent Node Snapshot / Events
              |
              v
      ESPUI ViewModel Adapter
              |
              v
      Phosphor State Resolver
              |
              v
       Motion Scheduler
              |
              v
       Geometry Renderer
              |
              v
        Glyph Composer
              |
              v
        Glyph Rasterizer
              |
              v
       RGB565 Canvas Buffer
              |
              v
          LVGL Canvas
              |
              v
           LCD Panel
```

## 7.2 模块边界

### `phosphor_state_resolver`

输入业务 ViewModel，输出视觉状态和各轨道参数。不得依赖 OpenCode 原生 JSON。

### `phosphor_motion`

管理连续相位、角速度、呼吸、事件脉冲、状态过渡。必须支持暂停和重置。

### `phosphor_geometry`

计算 Nucleus、Orbit、Satellite、轨道缺口和字符点位置。不得依赖 LVGL。

### `glyph_composer`

将几何采样结果合成为 36×24 字符 Cell Buffer。负责字符冲突优先级和亮度合成。

### `glyph_atlas`

提供只读 Bitmap Glyph 数据。

### `glyph_rasterizer`

把 Glyph Cell Buffer 光栅化为 RGB565 像素缓冲。

### `phosphor_lvgl_port`

仅负责将 RGB565 buffer 作为 LVGL Canvas 或受控的自定义绘制对象显示出来。LVGL 版本差异集中在这里。

---

# 8. 核心算法规格

## 8.1 字符帧数据结构

建议每个字符单元包含：

```c
typedef enum {
    GLYPH_COLOR_MUTED,
    GLYPH_COLOR_SECONDARY,
    GLYPH_COLOR_PRIMARY,
    GLYPH_COLOR_HIGHLIGHT,
    GLYPH_COLOR_WARNING,
    GLYPH_COLOR_ERROR
} glyph_color_id_t;

typedef struct {
    uint8_t glyph_index;
    uint8_t color_id;
    uint8_t intensity;  /* 0..255 */
    uint8_t flags;      /* front/back, satellite, transient 等 */
} glyph_cell_t;

#define PHOSPHOR_GRID_W 36
#define PHOSPHOR_GRID_H 24

typedef struct {
    glyph_cell_t cells[PHOSPHOR_GRID_H][PHOSPHOR_GRID_W];
} glyph_frame_t;
```

应避免把每个 Cell 做成包含指针的结构。字符与颜色使用索引，降低内存占用。

## 8.2 几何生成方式

首版不使用真正的 3D GPU 渲染。使用参数化椭圆 / 3D 圆轨道投影，形成 2.5D 视觉。

推荐实现：

1. 为每条 Orbit 定义半径、倾角、旋转角、相位、采样点数量；
2. 初始化时预计算单位圆的 `sin/cos` 查表；
3. 每帧只更新相位和必要的旋转参数；
4. 将采样点投影到 288×288 Canvas；
5. 映射到 36×24 字符网格；
6. 使用深度值和切线方向选择字符、亮度和前后层级；
7. 将每条轨道绘制到 Cell Buffer。

建议每条 Orbit 采样 48–72 个点。首版先使用 64 点。

不要对每个字符格每帧执行大量三角函数。预计算查表，减少浮点计算。

## 8.3 轨道投影

可采用以下数学模型：

```text
P(theta) = C + R * (U * cos(theta) + V * sin(theta))
```

其中：

- `C`：轨道中心；
- `R`：轨道半径；
- `U/V`：经过倾斜和旋转后的平面基向量；
- `theta`：当前相位。

再使用正交投影得到屏幕坐标。深度分量用于亮度和前后遮挡。

若采用二维旋转椭圆替代三维向量投影，也必须满足三条轨道不同倾角、不同相位的视觉要求。

## 8.4 Glyph 密度场

中心区域和轨道附近可以叠加字符密度场。

建议流程：

1. 计算当前 Cell 到 Nucleus 或 Orbit 的距离；
2. 使用平滑衰减函数得到 intensity；
3. 将 intensity 映射到 Glyph Ramp；
4. 根据 depth 和 Visual State 选择 color_id；
5. 对同一个 Cell 的多个贡献采用可预测的合成规则。

可使用平滑高斯或线性衰减作为第一版，不需要一开始追求复杂 SDF。

建议函数：

```c
float smooth_falloff(float distance, float radius);
uint8_t intensity_to_glyph(float intensity);
```

注意：

- 使用 clamp 防止越界；
- 不使用每帧随机噪声作为主体；
- 随机粒子必须有固定 seed 和生命周期；
- 同一个输入状态与时间点应尽可能得到可重复画面，便于截图验收。

## 8.5 字符方向

对轨道采样点计算切线方向。根据切线方向选择：

- 水平：`-` / `=`；
- 垂直：`|`；
- 斜向：`/` / `\\`；
- 转折或高密度：`+` / `*` / `#`。

首版可使用 8 个方向量化，不需要自由旋转字体。

## 8.6 深度与前后关系

通过投影深度决定字符亮度：

- 前侧轨道：Primary / Highlight；
- 侧向轨道：Secondary；
- 后侧轨道：Muted；
- 发生遮挡时，后层字符不能覆盖前层高亮字符。

同一 Cell 的合成规则必须明确，例如按深度优先，其次按强度优先；不能因绘制顺序不同导致画面随机闪烁。

## 8.7 Satellite 分配

- Session ID 映射到稳定 Slot；
- Slot 位置在状态更新时尽可能保持；
- 只有 Session 集合变化时才重新计算槽位；
- 同一 Session 状态变化只改变符号/颜色/脉冲，不无故改变位置；
- 超过可视节点数时，显示 `+N` 汇总。

---

# 9. Motion Scheduler 与状态过渡

## 9.1 时间管理

动画使用单调递增时钟，输入 `delta_ms`。

- `delta_ms` 必须进行上限截断；
- 避免设备从低功耗唤醒后一次性推进极大时间；
- 使用平滑插值切换角速度、亮度和密度；
- 状态切换不得立即跳变；
- UI Task 卡顿后允许跳过过期中间帧，不能补绘大量历史帧。

建议：

```c
void phosphor_motion_update(
    phosphor_motion_t *motion,
    uint32_t delta_ms,
    const phosphor_visual_state_t *state
);
```

## 9.2 过渡时间

| 转换 | 建议时间 |
|---|---:|
| Idle → Working | 300–700ms |
| Working → Idle | 500–900ms |
| Working → Approval overlay | 250–500ms |
| Online → Offline | 300–600ms |
| Offline → Online | 500–900ms |
| Completed Pulse | 800–1200ms |
| Error Glitch | 150–250ms |

所有过渡使用 ease-in-out 或平滑插值。避免线性硬切造成机械感。

## 9.3 防止动画抖动

- 状态 resolver 对短时间密集事件进行合并；
- Running 数量短暂变化时使用平滑过渡；
- Approval / Error 是注意力事件，不应被普通状态更新清除；
- Completed 事件按 event_id 去重；
- 动画状态必须有有限生命周期；
- 不能在网络断开后继续沿用 Working 动画。

## 9.4 Motion Profile 初始值

| 参数 | Idle | Working | Approval | Error | Offline |
|---|---:|---:|---:|---:|---:|
| 总体速度倍率 | 0.20 | 1.00 | 0.35 | 0.10 | 0.00 |
| 字符密度 | 0.30 | 0.75 | 0.45 | 0.50 | 0.15 |
| 高亮比例 | 低 | 中 | 中 | 中 | 极低 |
| 轨道完整性 | 1.00 | 1.00 | 0.85 | 局部破损 | 0.45 |
| 粒子数量 | 低 | 中 | 极低 | 极低 | 0 |

数值仅作为起始调参参数。应通过 theme config 暴露，不能散落在算法代码中。

---

# 10. 嵌入式渲染实现

## 10.1 推荐方式

推荐使用：

**Glyph Frame Buffer → 自定义 Glyph Rasterizer → RGB565 Buffer → LVGL Canvas / Image Object**

原因：

- 主体只有一个 Canvas，不创建数百个 LVGL Label；
- 字符渲染可控；
- 字符亮度、颜色和图形叠加更容易实现；
- 可以对中央区域独立进行截图和性能统计；
- 几何算法可以在 PC 环境复用。

## 10.2 LVGL 版本适配

开始前检查项目实际 LVGL 版本。

- LVGL v9 使用对应版本的 `lv_canvas_set_buffer` / Display API；
- LVGL v8 使用对应版本的 Canvas / Image API；
- 不允许将 v8 和 v9 的 API 混用；
- LVGL API 差异集中在 `phosphor_lvgl_port.c`；
- Geometry、Motion、Glyph Composer 和 Rasterizer 不得 include LVGL 头文件。

Canvas Buffer 在 Canvas 生命周期内必须保持有效。使用 PSRAM 分配时必须检查分配失败，并在 UI 对象销毁时释放。

## 10.3 缓冲区与颜色

中央 288×288 RGB565 Buffer：165,888 bytes。

建议：

- RGB565 像素缓冲优先放 PSRAM；
- Glyph Cell Buffer 放内部 RAM 或 PSRAM，按实际访问频率选择；
- Glyph Atlas 放 Flash 只读区；
- 不在每帧执行 malloc/free；
- 初始化时一次性分配，退出或切主题时统一释放；
- 若显示 DMA 不支持直接读取当前 buffer 内存区域，应由现有 BSP / esp_lvgl_port 做正确搬运，不可假设 PSRAM 必然 DMA 兼容。

## 10.4 刷新策略

- 首版目标 10–15 FPS；
- Idle 可降至 6–8 FPS；
- Approval 可降至 4–6 FPS；
- Completed 短暂提升到 12–15 FPS；
- 静态 Header / Metrics 只在数据变化时更新；
- 仅失效 Core Canvas 区域，不每帧重绘整个 480×480 屏幕；
- UI Task 统一操作 LVGL；
- 使用双缓冲/异步 DMA 时严格遵循当前 BSP 的 flush 完成回调规则。

不要为了追求更高 FPS 导致整个屏幕长期高功耗刷新。

## 10.5 线程安全

- 只有 UI Task 可以执行 LVGL 对象操作；
- Network Task 只写入受保护的消息队列；
- State Adapter 在合适的上下文中更新 ViewModel；
- UI Task 在锁或消息同步机制保护下读取稳定快照；
- 动画计算不能持有网络锁；
- UI Render 不允许等待网络响应。

---

# 11. PC 模拟器与视觉回归

## 11.1 必须具备 PC 模拟器

优先在 Linux 上使用 LVGL 官方 SDL PC Driver 构建 480×480 模拟窗口。

要求：

- 复用相同的 Geometry / Motion / Glyph / Theme 代码；
- PC 端仅替换 Display / Input Platform Adapter；
- 模拟器不允许重新实现一套独立的 ASCII 算法；
- 窗口按 1:1 显示 480×480 逻辑屏幕，另提供可选放大显示；
- 支持鼠标模拟触摸；
- 支持键盘快捷键切换状态。

## 11.2 Mock State 控制器

模拟器必须提供以下状态按钮：

```text
[IDLE]
[WORKING]
[COMPLETED]
[APPROVAL]
[ERROR]
[OFFLINE]
[WORKING + APPROVAL]
[WORKING + ERROR]
[MULTI SESSION]
```

支持调节：

- animation_speed；
- density；
- orbit_speed；
- brightness；
- motion_enabled；
- satellite_count。

## 11.3 Golden Screenshots

每个状态至少保存一张 480×480 PNG：

- `idle.png`
- `working.png`
- `completed.png`
- `approval.png`
- `error.png`
- `offline.png`
- `working_approval.png`
- `multi_session.png`

对比时重点检查：

- Core 中心位置是否稳定；
- 外轨有没有裁切；
- `>_` 是否清晰；
- 三条轨道是否可区分；
- 状态颜色和形态是否符合定义；
- Footer / Header 是否侵入主视觉；
- 字符密度是否过高或过低。

截图必须来自真实代码运行，不允许用静态设计图冒充运行结果。

---

# 12. 分阶段实施与验收

## P0：仓库与硬件基线审计

任务：

- 检查当前 ESP-IDF / LVGL / BSP 版本；
- 确认 480×480 分辨率、色彩格式、方向；
- 检查 PSRAM；
- 保留现有可工作的屏幕驱动；
- 建立 480×480 静态测试画面。

验收：

- 屏幕无旋转和触摸坐标错误；
- 黑色背景和调色板显示正确；
- 屏幕可稳定刷新；
- 有当前工程版本和硬件配置审计记录。

## P1：静态 Core 几何

任务：

- 实现 36×24 Glyph Frame；
- 实现三条不同倾角 Orbit；
- 实现中心 Nucleus；
- 实现 1–4 个 Satellite；
- 生成静态 RGB565 帧；
- 在 PC 模拟器和实机显示。

验收：

- 三条轨道明显不同，不像三个同心圆；
- 中心 `>_` 清晰；
- 主体完整位于 Canvas 内；
- 字符没有乱码、缺字和明显拉伸；
- 静态画面在桌面观看距离下有辨识度。

## P2：Glyph Renderer 与颜色

任务：

- 构建 Glyph Atlas；
- 实现密度 Ramp；
- 实现 1bpp → RGB565 Rasterizer；
- 实现亮度层级和 Palette；
- 实现前后层级合成。

验收：

- 字符密度呈连续层次；
- 前后轨道有空间感；
- 颜色不发生 RGB/BGR 颠倒；
- 无明显字符锯齿或重叠脏块；
- 帧 buffer 无越界。

## P3：Idle / Working 动画

任务：

- Motion Scheduler；
- 连续相位；
- Idle 呼吸和低速轨道；
- Working 字符流和密度变化；
- 状态平滑切换。

验收：

- Idle 和 Working 远看可区分；
- 状态切换无硬切；
- Working 不像普通旋转 Loading；
- 长时间播放没有明显抖动；
- PC 与 ESP32 的动画语义一致。

## P4：Completed / Approval / Error / Offline

任务：

- 一次性 Completed Pulse；
- Approval 外轨缺口和 Warning Satellite；
- Error 局部断裂和一次性 Glitch；
- Offline 轨道解体、冻结和提示；
- 多维状态合成。

验收：

- 六类状态不只靠颜色区分；
- Working + Approval 能同时表达；
- Working + Error 不会错误停止其他 Session 的活动；
- Offline 时没有伪 Working 动画；
- Completed 不重复播放；
- 事件重复到达不会重复触发动画。

## P5：性能与资源优化

任务：

- 统计渲染时间；
- 检查 heap / PSRAM；
- 检查任务栈；
- 降低不必要的整屏刷新；
- 检查长时间运行内存变化；
- 加入低动态降帧。

验收目标：

- Working 目标 10–15 FPS；
- Idle 目标 6–8 FPS；
- 无持续内存增长；
- 无 UI 卡死；
- 网络任务不被动画阻塞；
- 长时间运行至少 2 小时无崩溃和明显资源泄漏。

以上是目标，需要以实机测量数据为准并记录结果。

## P6：真实 OpenCode ViewModel 接入

任务：

- 接入 Node Snapshot；
- 接入 Node Event Stream；
- 将 Session 状态映射到 Phosphor Input State；
- 处理事件去重；
- 处理 Node Offline / Stale；
- 处理多 Session；
- 更新 Header 与 Metrics。

验收：

- OpenCode 真正进入 Running 时核心进入 Working；
- 所有 Session 空闲时进入 Idle；
- Permission 请求触发 Approval；
- Session 完成触发 Completed Pulse；
- 错误事件只影响对应节点和适当的提示层；
- Node 断开后按 Offline 规则显示；
- 恢复连接后以新 Snapshot 为准，不能沿用过期视觉状态。

---

# 13. 测试用例清单

## 13.1 视觉测试

- [ ] IDLE 静态截图；
- [ ] WORKING 静态截图；
- [ ] 三条轨道具有不同倾角与相位；
- [ ] 中心 Nucleus 在任何状态下不被遮挡；
- [ ] 外轨无裁切；
- [ ] Satellite 不重叠成无法识别的字符团；
- [ ] Warning / Error 状态具有形态差异；
- [ ] Offline 清晰表现为停止活动；
- [ ] 字符亮度层次自然；
- [ ] 无无意义高频闪烁。

## 13.2 状态组合测试

- [ ] Idle + No Attention；
- [ ] Running × 1；
- [ ] Running × 3；
- [ ] Running + Approval；
- [ ] Running + Error；
- [ ] Idle + Approval；
- [ ] Completed + Running；
- [ ] Offline + Last Known Running；
- [ ] Stale + Approval；
- [ ] Session 数量超过 4。

## 13.3 稳定性测试

- [ ] 快速连续切换状态；
- [ ] 重复收到同一 Completed Event；
- [ ] Event 突发到达；
- [ ] Node 断开与重连；
- [ ] Node 重启；
- [ ] Theme / Scene 退出与重新进入；
- [ ] 动画暂停与恢复；
- [ ] 运行 2 小时观察 heap / PSRAM。

---

# 14. 建议工程目录

根据当前仓库调整，但依赖边界必须保持。

```text
components/espui/
├── include/
│   ├── espui.h
│   ├── phosphor_core.h
│   ├── phosphor_types.h
│   └── phosphor_config.h
│
├── core/
│   ├── visual_state_resolver.c
│   └── view_model_adapter.c
│
├── motion/
│   ├── phosphor_motion.c
│   ├── motion_scheduler.c
│   └── interpolation.c
│
├── geometry/
│   ├── orbit_geometry.c
│   ├── radial_field.c
│   ├── satellite_layout.c
│   └── projection.c
│
├── glyph/
│   ├── glyph_frame.c
│   ├── glyph_composer.c
│   ├── glyph_atlas.c
│   └── glyph_rasterizer.c
│
├── render/
│   ├── phosphor_renderer.c
│   └── color_resolver.c
│
├── platform/
│   └── phosphor_lvgl_port.c
│
├── theme/
│   └── terminal_aesthetic/
│       ├── theme.c
│       ├── theme_tokens.h
│       └── motion_profile.h
│
└── tests/
    ├── test_state_resolver.c
    ├── test_geometry.c
    ├── test_glyph_composer.c
    └── test_motion.c
```

依赖方向：

- `geometry/` 不依赖 LVGL；
- `motion/` 不依赖 LVGL；
- `glyph/` 不依赖 Agent 或 Node 协议；
- `render/` 负责几何与字符帧合成；
- `platform/` 是唯一需要适配 LVGL 主版本和 BSP 的区域；
- `theme/` 通过 Theme API 调用上述能力；
- OpenCode 原生事件只能在上层 ViewModel Adapter 中映射。

---

# 15. Coding Agent 每阶段输出要求

每个阶段结束后，必须提交：

1. 修改文件列表；
2. 核心实现说明；
3. 编译命令和编译结果；
4. PC 模拟器截图或实机截图；
5. 实测 FPS、渲染耗时和内存数据（该阶段适用时）；
6. 未完成项和风险；
7. 与本规格不一致的地方及原因。

如果无法达到某项指标，必须报告具体原因和测量数据，不能默默降低验收标准。

不要在一个任务中一次性完成 P0–P6。建议每次只交付一个阶段，验收通过再继续。

---

# 16. 外部技术参考

以下链接用于核对当前工程所使用 LVGL 版本的 API。**实际代码必须以仓库锁定版本的头文件和文档为准。**

- LVGL Canvas：<https://lvgl.io/docs/open/widgets/canvas>
- LVGL Display / Draw Buffers：<https://lvgl.io/docs/open/main-modules/display/setup>
- LVGL SDL PC Driver：<https://lvgl.io/docs/open/integration/pc/sdl>
- Espressif `esp_lvgl_port`：<https://github.com/espressif/esp-bsp/tree/master/components/esp_lvgl_port>
- Waveshare ESP32-S3-Touch-LCD-4 Wiki：<https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4>

---

# 17. 最终完成定义

PHOSPHOR CORE V1.0 完成，不以“能显示 ASCII 动画”为标准，而以以下条件全部成立为标准：

1. 中央主体由程序生成字符与几何构成，不是视频/GIF；
2. 三条轨道、中心 Nucleus 和 Session Satellites 具备清晰层级；
3. 六类核心状态拥有可识别的形态、运动和颜色语义；
4. 多 Session 状态能够同时表达，不通过单个枚举互相覆盖；
5. Node Offline 时绝不展示虚假的 Agent 活跃动画；
6. 同一套视觉引擎可在 Linux SDL 模拟器和 ESP32 上运行；
7. 渲染、动画和状态解析不依赖 OpenCode 原生协议；
8. LVGL 对象生命周期、PSRAM buffer 和线程访问安全；
9. 长时间运行稳定；
10. 所有最终视觉截图来自实际运行程序，并完成实机确认。

**最终设计原则：PHOSPHOR CORE 应当像一个安静而有生命感的数字核心。空闲时完整、平稳；工作时字符流动、核心明亮；等待审批时轨道打开；发生错误时局部断裂；断网时逐渐失去连续性。颜色辅助识别，形态和运动负责讲述状态。**
