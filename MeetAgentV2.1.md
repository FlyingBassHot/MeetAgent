MeetAgent V2.1

双客户端并行开发与南向插件化架构项目计划书

项目定位： 面向 Coding Agent 的本地优先控制与管理平台
架构方向： Node-Centric、Multi-Client、Plugin-Based
客户端： Mobile + ESP32 Terminal
核心服务： MeetAgent Node
南向扩展： Agent Adapter Plugin System
网络策略： LAN First、VPN Remote、Cloud Optional

一、项目目标与整体思路

1.1 项目定位

MeetAgent 不再被定义为某一个特定客户端或某一个 Agent 的遥控器，而是一个支持多种 Agent 接入、多个客户端交互的统一控制平台。

用户可以在不同设备上感知和控制运行中的 Agent。

本次确定两种客户端并行建设：

MeetAgent Mobile

移动端是主要的远程交互入口。

主要解决：

离开电脑后查看 Agent 状态。

远程处理审批请求。

远程中断 Agent。

向 Agent 发送 Prompt。

查看 Session 和事件详情。

接收系统通知。

MeetAgent Terminal

基于 ESP32 的专用局域网终端。

主要解决：

桌面或家庭环境下的常驻状态展示。

Agent 运行情况快速感知。

关键事件即时提醒。

使用触摸、按键进行快捷操作。

不打开电脑或手机即可完成部分轻量操作。

它不是移动端的缩小版本，而是专注于低信息密度、快速感知和物理交互。

MeetAgent Node

两个客户端共享的核心服务。

Node 负责 Agent 接入、事件处理、状态管理、命令执行、安全控制和客户端通信。

客户端不直接连接 Agent 原生接口。

1.2 总体设计原则

本项目采用以下原则：

一个核心，多种客户端。

统一业务模型，不强制统一交互界面。

南向 Adapter 插件化，避免绑定具体 Agent。

通信协议与网络连接方式解耦。

LAN 优先，VPN 负责远程访问，Cloud 保持可选。

客户端能力、Agent 能力和用户权限分别管理。

先建立稳定接口，再并行开发客户端。

二、总体架构设计

2.1 MeetAgent V2.1 总体架构

                   Northbound Layer
                  多客户端交互层
                          |
            ┌─────────────┴─────────────┐
            |                           |
       MeetAgent Mobile          MeetAgent Terminal
        Vue + Capacitor           ESP32 + LVGL
            |                           |
            |                           |
            └─────────────┬─────────────┘
                          |
                 Client Protocol
                  统一客户端协议
                          |
                 Connectivity Layer
                  通信连接抽象层
                          |
       ┌──────────────────┼─────────────────┐
       |                  |                 |
      LAN                VPN            Cloud Relay
       |                  |                 |
       └──────────────────┼─────────────────┘
                          |
                    MeetAgent Node
                    核心业务服务
                          |
       ┌──────────────────┼──────────────────┐
       |                  |                  |
   Domain Core       Security Core      Client Manager
       |                  |                  |
   Agent Manager      Auth Manager       Client Registry
   Session Manager    Permission         Subscription
   Event Engine       Audit              Device Profile
   Command Handler
       |
                Southbound Adapter Layer
                     南向插件层
                          |
             ┌────────────┼────────────┐
             |            |            |
        OpenCode       Agent B       Agent C
          Adapter      Adapter      Adapter
             |            |            |
         HTTP/SSE    WebSocket       CLI/API
             |            |            |
         OpenCode      Agent B       Agent C

Cloud Relay 和 Push Gateway 作为外围可选基础设施，不参与 Node 的核心业务执行。

2.2 架构分层

A. Northbound：客户端接入层

负责不同交互终端的接入。

第一批客户端：

Mobile Client

ESP32 Terminal

未来预留：

Web Dashboard

Desktop Client

其他嵌入式终端

客户端统一通过 Client Protocol 与 Node 通信。

不同客户端可以声明不同的硬件和交互能力，Node 根据客户端能力决定推送哪些数据、允许哪些交互。

B. Core：核心业务层

负责统一管理 Agent、Session、Event 和 Command。

它不应该包含任何手机 UI 或 ESP32 UI 相关逻辑。

核心模块：

Agent Manager

Adapter Registry

Session Manager

Event Engine

Snapshot Service

Command Handler

Client Manager

Permission Manager

Audit Logger

C. Southbound：Agent 接入层

负责对接不同 Agent。

每一个 Agent Adapter 都需要实现统一的接口规范。

Node 不直接依赖 OpenCode、Claude Code 或其他特定 Agent 的协议。

D. Connectivity：网络连接层

负责提供多种网络访问路径。

LAN：移动端和 ESP32 的局域网通信。

VPN：移动端远程访问。

Relay：未来的公网中继。

Push：未来移动端后台系统通知。

连接层不应该影响 Core 的业务接口。

三、南向插件化设计

这是本次架构设计中需要重点投入的部分。

目标不是简单地把 OpenCode 的代码拆成一个文件夹，而是建立一套可以持续增加 Agent 类型的接入规范。

3.1 南向扩展边界

南向插件负责完成以下工作：

发现和识别 Agent。

与 Agent 建立连接。

查询 Agent 能力。

获取 Session 列表和状态。

订阅 Agent 事件。

将原生事件标准化。

接收 Node 的控制命令。

调用 Agent 原生接口执行操作。

将执行结果返回 Node。

插件不负责：

Mobile 页面展示。

ESP32 页面展示。

客户端身份认证。

用户权限管理。

公网消息路由。

客户端连接生命周期。

这些属于 Core 的职责。

3.2 Agent Adapter 与 Transport Driver 分离

建议在南向内部再分一层：

Southbound Adapter Layer
          |
          v
     Agent Adapter
          |
          v
     Transport Driver
          |
    ┌─────┼──────────┐
    |     |          |
   HTTP  WebSocket   CLI/IPC
    |     |          |
    └─────┼──────────┘
          |
     Agent Runtime

Agent Adapter 负责理解 Agent 的业务语义。

Transport Driver 负责具体通信方式。

例如：

OpenCode Adapter：

了解 OpenCode 的 Session 概念。

理解它的事件类型。

映射 Prompt、Interrupt、Permission 等操作。

通过 HTTP API 和 SSE 获取信息。

未来的另一个 Adapter 可以通过 WebSocket、CLI 或其他受控接口接入。

这样就不会把某种通信协议误认为 Agent 本身的能力。

3.3 Adapter 统一接口

建议先定义以下接口契约：

type AgentAdapter interface {
    Manifest() AdapterManifest

    Discover(ctx context.Context) ([]AgentInstance, error)

    Connect(ctx context.Context, cfg AdapterConfig) error

    Disconnect(ctx context.Context) error

    Health(ctx context.Context) (HealthStatus, error)

    ListSessions(ctx context.Context) ([]Session, error)

    GetSession(ctx context.Context, id string) (*Session, error)

    SubscribeEvents(ctx context.Context) (<-chan AgentEvent, error)

    Execute(ctx context.Context, cmd AgentCommand) (CommandResult, error)
}

这是接口设计示意，正式实现时应根据已有 Node 代码与 Agent 的异步特性调整。

特别注意：不能假设所有 Agent 都具备相同能力。

因此 Adapter 还需要提供能力声明。

3.4 Adapter Manifest

每个 Adapter 应当提供一个 Manifest：

id: opencode
name: OpenCode Adapter
version: 1.0.0
api_version: 1

runtime:
  type: local

transport:
  - http
  - sse

capabilities:
  - agent.health
  - session.list
  - session.read
  - session.events
  - session.prompt
  - session.interrupt
  - permission.review

permissions:
  - session.read
  - session.control

config:
  endpoint:
    type: string
    required: true

Manifest 用于：

Adapter 注册。

版本兼容检查。

能力发现。

配置校验。

权限声明。

客户端功能映射。

具体能力必须以对应 Agent 的真实接口为准，不能因为统一模型存在某个操作，就假设所有 Agent 都支持。

3.5 能力模型

建议至少定义以下能力类别：

Agent Capabilities
├── agent.health
├── session.list
├── session.read
├── session.events
├── session.prompt
├── session.interrupt
├── session.pause
├── session.resume
├── permission.review
└── permission.respond

不同 Adapter 可以声明不同能力集合。

例如：

OpenCode Adapter:
- session.list
- session.read
- session.events
- session.prompt
- session.interrupt
- permission.review
- permission.respond

Another Adapter:
- session.list
- session.read
- session.events
- session.interrupt

客户端不能直接根据 Agent 类型判断功能是否存在，而应根据能力声明决定是否展示操作入口。

3.6 原生数据与标准化数据并存

不能为了统一而过度抽象。

建议事件采用：

{
  "event_id": "evt-1001",
  "event_type": "session.status_changed",
  "session_id": "session-001",
  "timestamp": "2026-11-05T19:00:00Z",
  "source": {
    "adapter_id": "opencode",
    "native_event_type": "session.updated"
  },
  "payload": {
    "status": "running"
  },
  "extensions": {
    "opencode": {}
  }
}

其中：

event_type 是平台统一事件。

payload 是通用字段。

source 标记原始来源。

extensions 保存特定 Agent 的扩展数据。

这样既能够统一客户端，也不会丢掉原生 Agent 的特殊信息。

3.7 插件加载策略

不建议第一阶段就开发复杂的动态插件市场。

分两步实施：

第一阶段：模块化 Adapter

Adapter 有统一接口。

以 Node 内部模块形式编译和注册。

每个 Adapter 独立目录。

独立配置和测试。

Core 只依赖 Adapter 接口。

第二阶段：外部进程插件

当需要第三方开发者自行编写 Adapter 时，再增加外部插件运行机制。

可以采用独立进程，通过 gRPC 或 JSON-RPC 与 Node 通信。

这样能降低第三方插件崩溃影响整个 Node 的风险，也更容易控制权限和运行环境。

不建议把动态加载机制与南向接口规范混为一谈：接口先稳定，运行机制以后再扩展。

四、北向多客户端统一设计

4.1 统一 Client Protocol

Mobile 与 ESP32 必须共用一套业务通信协议。

禁止出现以下情况：

Mobile 直接访问 Agent Adapter。

ESP32 单独调用一套特殊的 Agent API。

为了 ESP32 重新复制一套 Node 业务逻辑。

Mobile 和 ESP32 各自定义 Session、Event、Command 数据结构。

协议分为三个核心部分：

Snapshot

当前完整状态，用于首次连接和恢复。

Event Stream

实时事件，用于状态更新和通知。

Command

控制命令，用于客户端操作 Agent。

4.2 客户端握手

每一个客户端建立连接时，提交自己的信息：

{
  "protocol_version": 1,
  "client_id": "client-mobile-001",
  "client_type": "mobile",
  "client_version": "1.0.0",
  "capabilities": [
    "ui.full",
    "input.text",
    "notification.system",
    "voice.input"
  ]
}

ESP32 则可以声明：

{
  "protocol_version": 1,
  "client_id": "client-terminal-001",
  "client_type": "terminal",
  "firmware_version": "1.0.0",
  "capabilities": [
    "display.color",
    "input.button",
    "input.encoder",
    "notification.local"
  ]
}

Node 根据客户端类型和 capabilities 建立对应连接上下文。

4.3 必须分开的三种能力

这是非常重要的设计。

Client Capability

客户端本身支持什么交互。

例如：

是否有触摸屏。

是否支持文本输入。

是否支持语音。

是否支持系统通知。

是否有板载实体按键或外接旋钮（LCD-4 默认只有触摸 + BOOT/RESET/BAT_PWR）。

Agent Capability

Agent 本身支持什么操作。

例如：

是否支持 Interrupt。

是否支持 Prompt。

是否支持 Permission Response。

是否支持 Session 查询。

User Permission

当前客户端拥有怎样的操作权限。

例如：

是否允许读取 Session。

是否允许中断任务。

是否允许批准某类请求。

是否能够管理其他客户端。

真正能够执行的操作需要同时满足：

Effective Capability
=
Client Capability
∩ Agent Capability
∩ User Permission

注意：三者是不同维度的约束，不应该合并为一个简单的 capability 列表。

4.4 统一命令处理

两个客户端的所有操作统一经过：

Mobile / ESP32
       |
       v
Command Gateway
       |
       v
Identity Verification
       |
       v
Permission Check
       |
       v
Capability Check
       |
       v
Command Dispatcher
       |
       v
Agent Adapter
       |
       v
Command Result

Node 是最终权限判断和命令执行责任主体。

多个客户端可能同时操作同一个 Session，因此需要：

command_id。

幂等处理。

操作超时。

命令结果回传。

审批请求过期检查。

对有状态操作使用必要的版本校验。

五、通信协议设计

5.1 统一通信方式

协议

用途

移动端

ESP32

HTTPS

初始化与查询

支持

支持

WSS

实时事件和命令

支持

支持

APNs / FCM

系统推送

支持

不需要

mDNS

Node 局域网发现

支持

支持

第一阶段不额外引入 MQTT Broker。

HTTPS + WSS 足以支持两个客户端，避免为了两种客户端引入三套业务通信机制。

如果未来接入大量嵌入式设备或需要独立遥测消息总线，再考虑 MQTT 作为内部或专用传输方式。

5.2 统一事件信封

{
  "schema_version": 1,
  "event_id": "evt-001",
  "node_id": "node-home",
  "sequence": 1024,
  "type": "session.approval_required",
  "occurred_at": "2026-11-05T19:20:00Z",
  "session_id": "session-001",
  "payload": {
    "approval_id": "approval-001"
  }
}

两个客户端收到的是同一业务事件。

不同的是：

Mobile 可以展示完整上下文。

ESP32 可以展示精简摘要和操作入口。

Node 可以根据订阅和客户端能力进行数据过滤，但不能改变事件的业务含义。

5.3 统一命令信封

{
  "schema_version": 1,
  "command_id": "cmd-001",
  "node_id": "node-home",
  "session_id": "session-001",
  "type": "session.interrupt",
  "payload": {},
  "created_at": "2026-11-05T19:20:00Z"
}

ESP32 和 Mobile 产生的命令进入同一套 Command Gateway。

5.4 Snapshot + Event Stream

Node 维护：

当前状态 Snapshot。

单调递增 Sequence。

有限容量 Event Buffer。

连接流程：

客户端完成认证。

获取 Snapshot。

提交最后处理的 Sequence。

Node 补发可用的缺失事件。

事件已过期时重新提供 Snapshot。

建立实时 WSS 订阅。

ESP32 只需要保存必要的 Sequence 和本地 UI 状态，不需要在 Flash 中永久保存完整事件历史。

协议应该具备版本号和兼容机制。ESP32 对不认识的非关键事件可以忽略，但必须识别协议版本不兼容、连接失效等关键状态。

六、ESP32 Terminal 实现方案

6.1 产品定位

ESP32 是一个局域网常驻终端。

它不负责远程控制，也不替代移动端。

它重点提供：

实时 Agent 状态。

当前活跃 Session。

关键事件。

任务完成提醒。

快捷 Interrupt。

有上下文约束的审批操作。

简单的物理交互。

ESP32 不需要实现完整聊天和大量历史数据浏览。

6.2 硬件与软件

当前目标硬件：

Waveshare ESP32-S3-Touch-LCD-4（V4.0，近期购买确认）。

- 4.0 英寸 480 × 480 RGB565 电容触摸屏
- GT911 五点触控，支持中断
- ESP32-S3N16R8：16MB Flash + 8MB Octal PSRAM
- RGB 并口 LCD
- V4 使用 CH32V003F4U6 helper（I2C）控制背光、LCD/触摸复位、蜂鸣器和电源/电池 ADC；早期版本为 TCA9554
- 板载 CAN、RS485、RTC、SD 卡、蜂鸣器、电池接口（首版 Terminal 不要求全部使用）
- ESP-IDF（官方要求 >= 5.2.5；当前使用 5.4.4）
- FreeRTOS、LVGL 8.x
- Wi-Fi、触摸；板载 BOOT / RESET / BAT_PWR 按键

输入模型随之调整：新板没有旋钮，首版以**触摸为主**，板载按键作为 Back / Interrupt / 电源辅助；如后续需要旋钮，再外接编码器。

后续按需增加麦克风和音频输出。

硬件 GPIO、LCD、触摸、音频与电源设计必须以 V4 原理图、官方示例和 BSP 为准，不提前固定未经验证的引脚定义。旧版本文档中的 TCA9554/I2C 分配不适用于 V4。

### 6.2.1 开发顺序（BSP First）

1. 以 V4.0 原理图、官方 `06_lvgl_demo_v8` 示例和 BSP 组件为准，整理 RGB LCD、GT911、CH32V003 helper 和背光映射。
2. 优先评估官方 BSP `waveshare/esp32_s3_touch_lcd_4 ^3.0.0`，通过 HAL 包装接入；仅在版本/API 不兼容时写最小 BSP。
3. 先完成板级 BSP bring-up：LCD 点亮、颜色/方向、触摸坐标、背光。
4. 再把主题布局从旧的 320×480 竖屏适配为 480×480 方形屏。
5. 最后接 Wi-Fi 配网、mDNS、配对、WSS 和 Client Protocol（E4）。

在 BSP 未通过真机验证前，不把 GPIO 猜测值写入 `sdkconfig.defaults` 或板级代码。

6.3 软件任务划分

MeetAgent Terminal Firmware
│
├── UI Task
│   └── LVGL
│
├── Network Task
│   ├── WiFi
│   ├── HTTPS
│   └── WSS
│
├── Input Task
│   ├── Touch
│   ├── Encoder
│   └── Buttons
│
├── State Manager
│   ├── Node State
│   ├── Agent State
│   └── Session State
│
├── Event Manager
│
├── Device Provisioning
│
└── Power Manager

核心原则：

网络任务不直接操作 LVGL。

事件经过状态管理后再更新 UI。

UI Task 不直接执行 Agent 命令。

所有控制操作经过统一 Command Client。

网络断线不阻塞界面。

6.4 ESP32 页面

Overview

展示：

Node 连接状态。

Agent 状态。

活跃 Session 数。

最近事件。

待审批数量。

Session

展示：

当前关注的 Session。

Agent 名称。

Session 运行状态。

最近事件摘要。

Activity

展示：

任务开始。

任务完成。

任务异常。

审批请求。

Agent 状态变化。

Control

展示有限的快捷操作：

Interrupt。

Approval。

常用操作入口。

审批必须先展示足以判断请求的上下文。屏幕空间不足以完整展示风险信息时，应引导用户转移至 Mobile，而不是盲目批准。

6.5 局域网接入与设备配对

首次启动时：

ESP32 通过 BLE 或 SoftAP 配网。

加入家庭 Wi-Fi。

通过 mDNS 发现 MeetAgent Node。

请求 Node 配对。

屏幕展示设备身份和短期配对码。

用户在已授权的 Mobile 或 Node 管理界面确认。

Node 为终端签发独立凭证。

Terminal 建立 HTTPS / WSS 连接。

终端必须拥有独立身份，不能与 Mobile 共享长期 Token。

如果 Node 有多个实例，第一版可以允许用户为终端指定一个主要 Node；协议和数据结构应保留未来多 Node 订阅能力。

6.6 ESP32 工程目录

meetagent-terminal/
├── main/
│   └── app_main.c
│
├── components/
│   ├── ui/
│   ├── networking/
│   │   ├── wifi/
│   │   ├── http_client/
│   │   └── websocket/
│   │
│   ├── protocol/
│   ├── state/
│   ├── events/
│   ├── commands/
│   ├── input/
│   ├── pairing/
│   └── power/
│
├── managed_components/
├── partitions/
├── sdkconfig
└── CMakeLists.txt

6.7 ESP32 功能限制

首版不要求实现：

完整 Prompt 聊天。

文件浏览。

长历史查询。

大量 Agent 同屏复杂交互。

远程 VPN 访问。

系统级推送。

本地 AI 推理。

这些能力由 Mobile 或 Node 提供。

七、Mobile 实现方案

7.1 定位

Mobile 是完整的移动交互客户端，既支持 LAN，也支持 VPN 远程连接。

它承担更加丰富的 Session 交互和管理功能。

7.2 技术栈

Vue 3

TypeScript

Vite

Pinia

Vue Router

Capacitor

HTTPS / WSS

Preferences / SQLite

APNs / FCM（后续）

7.3 页面划分

Home

Node 列表。

Agent 状态。

活跃 Session。

运行统计。

待审批和异常事件。

Sessions

Session 列表。

Session 详情。

任务和工具事件。

Prompt 操作。

Interrupt 操作。

Activity

全局事件流。

事件筛选。

事件详情。

审批入口。

Settings

Node 配置。

LAN / VPN 连接。

客户端设备管理。

通知设置。

安全和权限设置。

7.4 移动端工程目录

meetagent-mobile/
├── src/
│   ├── app/
│   ├── views/
│   ├── components/
│   ├── stores/
│   ├── domain/
│   ├── services/
│   │   ├── api/
│   │   ├── websocket/
│   │   ├── connectivity/
│   │   ├── notification/
│   │   └── storage/
│   │
│   └── platform/
│       ├── browser/
│       └── capacitor/
│
├── tests/
├── public/
└── package.json

Mobile 业务层不直接依赖 Capacitor 插件，原生能力通过 Platform Adapter 提供。

八、统一网络连接方案

8.1 移动端连接

支持：

家庭 Wi-Fi：LAN 直连。

外部 Wi-Fi：通过 Tailscale。

蜂窝网络：通过 Tailscale。

8.2 ESP32 连接

只支持：

家庭 Wi-Fi。

与 Node 同一局域网的有线或无线网络。

首版不让 ESP32 直接接入互联网 Cloud，也不要求它具备 VPN 能力。

8.3 Cloud 可选

未来增加：

Cloud Relay。

Push Gateway。

Cloud Registry。

Cloud 不拥有 Agent 执行权，也不负责终端 UI 状态管理。

九、两条客户端开发线与公共开发线

这次采用三条并行工作线，而不是只分成两个完全独立项目。

Workstream A：Core / Node

负责公共能力，是两条客户端开发线的基础。

工作内容：

Adapter Interface。

Event Model。

Snapshot Service。

Command Gateway。

Client Registry。

Auth / Pairing。

HTTP / WSS。

Protocol Simulator。

交付物：

可独立运行的 Node。

Client Protocol。

Mock Agent。

测试服务器。

OpenCode Adapter 接入。

Workstream B：Mobile

工作内容：

Vue3 工程。

页面与交互。

API Client。

WebSocket Client。

状态同步。

Approval / Interrupt / Prompt。

Capacitor。

VPN 接入。

交付物：

Mobile Web 原型。

Mobile MVP。

Capacitor Android / iOS 应用。

Workstream C：ESP32 Terminal

工作内容：

硬件与 BSP：ESP32-S3-Touch-LCD-4 V4（480×480 RGB LCD + GT911 + CH32V003 helper）。

系统初始化与显示/触摸/背光 bring-up。

Wi-Fi 配网。

mDNS 发现。

设备配对。

Client Protocol 实现。

LVGL 页面（480×480 方形布局）。

输入设备驱动（触摸为主，板载按键辅助）。

事件显示与快捷控制。

交付物：

ESP32 Firmware。

Terminal MVP。

局域网常驻显示与控制能力。

十、阶段计划与验收

M0：公共架构与协议设计

建议周期：第 1 周

公共任务：

明确 Northbound / Core / Southbound。

梳理现有代码。

定义 Adapter Interface。

定义 Adapter Manifest。

定义 Client Protocol。

定义 Event、Snapshot、Command。

定义配对与权限模型。

建立 Mock Agent 和 Protocol Simulator。

验收：

两种客户端使用相同业务协议。

Agent Adapter 与客户端无直接依赖。

Node 可以独立运行。

新增 Agent 不需要改动客户端业务代码。

不同客户端能够声明不同能力。

M1：Node Core 改造

建议周期：第 2—3 周

公共任务：

Node Standalone Mode。

API Server。

WSS Server。

Client Registry。

Pairing Manager。

Snapshot Service。

Event Engine。

Command Handler。

验收：

不启动 Cloud 也能使用 Node。

至少一个真实 Adapter 可以工作。

能同时连接 Mobile 模拟器和 Terminal 模拟器。

多个客户端收到一致的业务状态。

客户端身份能够单独撤销。

M2：双客户端并行开发

建议周期：第 3—4 周

Mobile

完成 Home。

完成 Sessions。

完成 Activity。

完成 Settings。

接入 Mock Node。

实现状态展示。

验收：

能够正常展示 Mock 数据。

页面组件与通信层分离。

连接状态变化可以反馈到 UI。

ESP32

ESP-IDF 工程初始化。

ESP32-S3-Touch-LCD-4 V4 BSP：480×480 RGB LCD、GT911 触摸、CH32V003 helper 复位/背光。

480×480 LVGL 布局。

触摸优先的输入抽象。

Wi-Fi 配网。

Node mDNS 发现。

WSS 通信原型。

Mock Event 显示。

验收：

终端能够加入局域网。

能够发现 Mock Node。

能够显示连接状态。

能够接收并展示标准化事件。

M3：真实 Agent 集成与客户端控制

建议周期：第 5—6 周

公共任务

接入真实 OpenCode Adapter。

统一 Session 模型。

实现事件订阅。

实现命令处理。

实现幂等机制。

Mobile 验收

能展示真实 Agent。

能查看 Session。

能查看事件详情。

能够发送 Prompt。

能够 Interrupt。

能够处理审批。

ESP32 验收

LCD-4 480×480 显示、GT911 触摸、背光通过真机验证。

能显示真实 Agent 状态。

能显示活跃 Session。

能接收关键事件。

能进行快捷 Interrupt。

能展示审批请求摘要。

能在具有充分上下文时执行审批，否则引导使用 Mobile。

M4：双端统一与稳定性

建议周期：第 7 周

测试：

Mobile 和 ESP32 同时在线。

两个客户端订阅同一 Session。

一个客户端执行操作，另一个客户端状态更新。

两个客户端同时操作同一 Session。

网络中断。

Node 重启。

客户端重新连接。

Event Sequence 补偿。

Command 重复提交。

验收：

两个客户端的核心状态保持一致。

操作结果能够同步到另一客户端。

重复命令不会重复执行。

网络恢复后能够正确同步。

不同客户端的权限能够独立配置。

M5：移动端远程连接

建议周期：第 8 周

任务：

Tailscale。

多网络切换。

VPN 连接状态展示。

远程连接恢复。

Capacitor。

系统通知基础实现。

验收：

Mobile 在外部网络可以访问 Node。

ESP32 仍在 LAN 内正常工作。

远程 Mobile 与本地 ESP32 看到同一套 Agent 状态。

Mobile 远程操作后，ESP32 能接收到对应状态变化。

VPN 断开后准确反馈离线状态。

M6：增强与扩展

后续迭代，不作为双客户端 MVP 的前置条件。

APNs / FCM 完整推送。

Cloud Relay。

第二种 Agent Adapter。

外部进程插件机制。

语音输入。

多 Node 高级管理。

Terminal 固件 OTA。

更多客户端类型。

十一、双客户端统一验收场景

除了各自的功能验收，还必须验证双客户端共同工作的场景。

场景一：本地状态同步

Node 启动。

Mobile 通过 LAN 配对。

ESP32 通过 LAN 配对。

两端同时打开。

Agent 创建 Session。

Node 推送事件。

两端展示相同的 Session 状态。

验收：事件不会因某一个客户端的存在而改变业务结果。

场景二：跨客户端操作

ESP32 展示一个正在运行的 Session。

Mobile 远程进入该 Session。

Mobile 执行 Interrupt。

Node 调用 Adapter。

Agent 停止执行。

Node 发布状态更新。

ESP32 收到更新并显示停止状态。

验收：一个客户端执行操作，另一个客户端能够正确感知。

场景三：多端审批

Agent 发起审批请求。

Node 生成标准化审批事件。

Mobile 和 ESP32 同时展示待处理状态。

Mobile 完成审批。

Node 记录最终决策。

ESP32 删除待处理状态并显示决策结果。

验收：同一个审批只能有一个有效的最终决策；另一个客户端不能再次提交有效决策。

场景四：南向扩展验证

在 Node 中新增一个 Mock Adapter。

为它声明不同的 Agent Capability。

不修改 Mobile 和 ESP32 的业务代码。

Node 自动通过统一协议提供其标准化状态和可用操作。

验收：只要符合 Adapter Contract，客户端不需要知道具体接入的是哪一种 Agent。

十二、风险控制与设计约束

风险

解决方案

两个客户端重复建设业务逻辑

核心逻辑集中在 Node

Agent 差异影响客户端

Adapter + Capability Model

ESP32 内存与性能受限

精简事件、限制历史缓存

移动端后台连接失效

Snapshot + Event 补偿 + 系统推送

多客户端操作冲突

命令幂等、权限检查、状态版本

插件异常影响 Node

首版模块化，后续外部进程隔离

新 Agent 接入需要修改客户端

统一模型与扩展字段

网络连接方式与业务耦合

Connectivity Layer

Cloud 成为单点依赖

Node Standalone

十三、最终交付目标

本轮 V2.1 完成后，MeetAgent 应该具备：

Core：

独立运行的 MeetAgent Node。

标准 Agent Adapter Interface。

Agent Capability Model。

统一事件和命令体系。

客户端认证、配对与权限系统。

Mobile：

Vue3 + Capacitor 移动端。

LAN 和 VPN 连接。

Agent 与 Session 管理。

Approval、Prompt、Interrupt。

系统通知基础能力。

ESP32：

独立固件。

Wi-Fi 配网与 Node 发现。

局域网安全配对。

实时状态和事件展示。

实体按键快捷操作。

有约束的审批与控制。

Infrastructure：

Cloud 不再是必需依赖。

LAN 和 VPN 可用。

Relay 与 Push 可独立扩展。

十四、架构决策总结

MeetAgent V2.1 采用：

Northbound：多客户端

Mobile、ESP32 和未来其他终端共享统一业务接口。

Core：统一业务核心

所有 Agent 管理、状态维护、命令处理和安全判断都由 Node 完成。

Southbound：插件化接入

每种 Agent 通过 Adapter 接入，提供能力声明和标准化数据。

Connectivity：网络方式可替换

LAN、VPN、Relay 不影响核心业务。

最终形成：

          Mobile       ESP32
              \         /
               \       /
           Client Protocol
                   |
              Node Core
                   |
            Adapter Registry
                   |
            Agent Adapters
                   |
             Agent Runtime

这次项目的核心不是同时做两个界面，而是建设一套能够服务多个交互终端、接入不同 Agent 的统一平台。

Mobile 和 ESP32 是两个并行的客户端项目；Adapter 体系和 Client Protocol 是支撑它们长期演进的公共基础。

项目实现的判断标准也不再只是“手机能控制 OpenCode”或“ESP32 能显示 OpenCode 状态”，而是：在不修改客户端核心代码的情况下，系统能接入新的 Agent；在不修改 Node 业务逻辑的情况下，系统能增加新的客户端。