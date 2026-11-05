# MeetAgent

> **Legacy V1 设计文档（归档参考）**：当前项目已转向 V2.1，见 `MeetAgentV2.1.md`、`TerminalUi.md` 与 `docs/v2.1/`。

MVP 开发设计文档

**项目名称：** MeetAgent  
**项目定位：** 面向 Coding Agent 的远程监控、交互与控制平台  
**文档版本：** v0.1 — MVP  
**核心技术：** Go、WebSocket、ESP32-S3、ESP-IDF、LVGL  
**首个接入目标：** OpenCode

---

# 1. 项目概述

## 1.1 项目背景

随着 Coding Agent 的普及，开发者越来越多地使用 OpenCode、Claude Code、Codex、Pi 等工具完成代码编写、项目分析和自动化开发任务。

但这些 Agent 大多运行在个人电脑或服务器上，交互入口通常局限于本地终端、IDE 或 Web 界面。

当开发者离开电脑后，很难及时了解 Agent 的运行状态，也无法方便地进行以下操作：

- 查看 Agent 是否正在运行。
- 查看当前正在处理的项目和 Session。
- 观察 Agent 的执行过程及工具调用。
- 向 Agent 发送新的 Prompt。
- 中断正在执行的任务。
- 对 Agent 发起的权限请求进行审批。
- 在多个 Agent 和 Session 之间切换。

本项目希望解决这些问题，建立一个独立于具体 Coding Agent 的远程控制与监控系统。

## 1.2 产品定位

MeetAgent 是一个多 Agent 接入的远程控制基础设施。

它由云端控制平面、本地 Agent 代理和远程客户端组成。

核心目标：

1. 将不同 Coding Agent 的接入方式统一抽象。
2. 将 Agent 的运行状态和事件统一转换为平台内部数据模型。
3. 允许远程客户端通过统一协议观察和控制 Agent。
4. 支持不同设备、不同网络环境下的远程连接。
5. 通过 Adapter 插件机制接入不同 Agent，而不将系统绑定到某个具体产品。

第一阶段使用 ESP32-S3 作为便携式远程终端。

但需要明确：**MeetAgent 的核心产品不是 ESP32 终端，而是 Agent 接入与远程通信平台。**

未来可以在同一套基础设施上增加 Web Dashboard、移动端和桌面客户端。

## 1.3 MVP 目标

第一版不追求功能完整，而是验证整个架构是否成立。

MVP 必须完成：

- 云端 Gateway 能够同时管理多个 Node 和 Device 连接。
- 本地 Agent Node 能够主动连接云端。
- ESP32 或浏览器测试客户端能够主动连接云端。
- 两端无需公网 IP、无需路由器端口映射即可进行双向通信。
- Node 能够通过 Adapter 接入 OpenCode。
- 远程客户端能够查看 Agent、Session 和运行事件。
- 能够发送 Prompt、中断任务和处理权限审批（以底层 Agent 实际支持的能力为准）。
- 具备心跳、断线重连和状态恢复机制。

第一版暂不实现：

- 多用户 SaaS 和复杂组织权限。
- P2P 直连。
- 语音交互。
- AI 对 Agent 的二次分析。
- 任意 Shell 远程执行。
- 完整代码编辑器和 Diff 浏览器。
- 多 Agent 自动编排。
- 完整的会话历史云端存储。
- 自定义插件市场。

---

# 2. 总体技术架构

## 2.1 三节点架构

整个系统分为三个主要组成部分。

### A. MeetAgent Cloud

部署在具有公网访问能力的 VPS 上。

职责：

- 接收 Node 和 Device 的连接。
- 管理设备身份和连接状态。
- 管理 Node 与 Device 的关系。
- 转发命令和事件。
- 维护短期事件缓存。
- 提供 REST API。
- 处理连接认证和基础权限校验。

Cloud 不负责运行 Coding Agent，也不直接访问 Agent 所在机器的本地接口。

它只负责通信协调和控制平面管理。

### B. MeetAgent Node

安装在运行 Coding Agent 的电脑或服务器上，以独立常驻进程的形式运行。

例如：

```bash
meetagent-node
```

Node 是整个系统的本地接入层。

职责：

- 主动连接 Cloud，建立 WSS 长连接。
- 发现和配置本机 Agent。
- 管理多个 Agent Instance。
- 通过 Adapter 接入不同 Agent。
- 收集 Agent 运行事件。
- 接收 Cloud 转发的远程命令。
- 执行本地命令和权限校验。
- 将执行结果及状态变化返回 Cloud。

Node 不依赖具体 Coding Agent，也不要求 Agent 修改自身代码。

### C. Agent Terminal

第一版使用 ESP32-S3 开发。

职责：

- 连接 Wi-Fi 和 Cloud。
- 获取 Node、Agent、Session 列表。
- 显示 Agent 运行状态。
- 显示执行事件和工具调用。
- 发送 Prompt。
- 中断任务。
- 处理权限审批。
- 管理 Session。

ESP32 不运行 Coding Agent，不承担复杂的会话历史存储和大规模数据处理。

---

## 2.2 整体拓扑

```text
                 Internet
                    |
           +------------------+
           |  MeetAgent     |
           |  Cloud / VPS     |
           |                  |
           |  Connection Mgr   |
           |  Device Registry  |
           |  Node Registry    |
           |  Message Router   |
           |  Event Hub        |
           |  Auth / Policy    |
           +------------------+
                 /       \
                /         \
           WSS /           \ WSS
              /             \
+-------------------+   +--------------------+
| Agent Terminal    |   | Developer Machine  |
| ESP32-S3          |   |                    |
|                   |   | MeetAgent Node   |
| Dashboard         |   |                    |
| Session UI        |   | Adapter Manager    |
| Activity UI       |   | Event Collector    |
| Prompt UI         |   | Command Executor   |
| Approval UI       |   |                    |
+-------------------+   +--------------------+
                                |
                     +----------+----------+
                     |          |          |
                  OpenCode    Codex      Pi/Claude
```

核心设计：**两端都主动连接 Cloud，不让终端直接访问 Agent 所在机器。**

这样即使开发电脑位于家庭 NAT 网络、公司网络或手机热点后面，也不需要配置端口映射。

## 2.3 网络连接关系

| 连接 | 协议 | 作用 |
|---|---|---|
| ESP32 → Cloud | WSS | 终端连接 |
| Node → Cloud | WSS | 本地节点连接 |
| Node → OpenCode | HTTP / SSE | Agent API 和事件订阅 |
| Node → Pi | JSONL / RPC | Agent 进程通信 |
| Node → Codex | stdio / JSON-RPC | Agent App Server |
| Node → Claude Code | Hooks / SDK / CLI | 根据实际开放能力接入 |

所有远程通信统一经过 Cloud。

Node 与 Agent 之间则保留各 Agent 的原生通信方式，不强行统一底层协议。

---

# 3. 技术选型

## 3.1 Cloud 和 Node：Go

选择 Go 作为核心后端语言。

原因：

- 擅长网络服务和长连接管理。
- Goroutine 适合并发管理多个连接。
- 编译后可以得到独立可执行文件。
- 方便跨平台部署。
- 适合实现长期运行的后台服务。
- 可以共享协议模型和部分业务代码。

推荐技术：

| 技术 | 用途 |
|---|---|
| Go | 核心开发语言 |
| chi | HTTP 路由 |
| gorilla/websocket 或 coder/websocket | WebSocket |
| encoding/json | JSON 序列化 |
| slog | 结构化日志 |
| SQLite | MVP 本地持久化（可选） |
| PostgreSQL | 后续云端持久化（需要时引入） |
| systemd | Linux 服务管理 |

建议先使用模块化单体，不引入微服务架构。

Cloud 和 Node 编译为两个独立程序：

```text
meetagent-cloud
meetagent-node
```

两个程序共享 protocol、domain 等基础代码。

## 3.2 ESP32：ESP-IDF + LVGL

硬件建议：

- ESP32-S3。
- 8MB PSRAM。
- 16MB Flash。
- 3.5 英寸触摸屏。
- Wi-Fi。
- USB-C 供电或锂电池供电。

软件选型：

| 技术 | 用途 |
|---|---|
| ESP-IDF | ESP32 官方开发框架 |
| FreeRTOS | 多任务管理 |
| LVGL | UI 渲染 |
| esp_websocket_client | WebSocket 客户端 |
| esp-tls | TLS 加密 |
| cJSON | JSON 解析 |
| NVS | 配置持久化 |

UI 和网络逻辑必须分离。

网络任务接收事件后，将数据放入消息队列，由 UI Task 统一更新 LVGL。

禁止在 WebSocket 回调中直接操作 LVGL。

## 3.3 测试客户端：Web

在 ESP32 开发之前，先实现一个浏览器测试客户端。

技术：

- Vue 3。
- TypeScript。
- WebSocket API。
- 简单的 Agent/Session/Activity 页面。

它主要用于验证 Cloud 和 Node 的完整通信链路。

不要一开始就投入大量时间开发 ESP32 UI。

---

# 4. 分层设计

系统采用分层架构。

核心原则：

- 领域模型不依赖具体 Agent。
- Cloud 不依赖具体 Agent 的原生协议。
- Node 不依赖具体终端设备。
- Adapter 负责具体 Agent 的协议适配。
- Terminal 只消费统一的数据模型和协议。

## 4.1 第一层：Transport Layer

通信传输层。

职责：

- WebSocket 连接。
- TLS。
- 心跳。
- 断线检测。
- 自动重连。
- 消息发送和接收。

这一层不处理具体 Agent 业务。

## 4.2 第二层：Protocol Layer

统一通信协议层。

负责定义 Cloud、Node 和 Device 之间的数据格式。

统一使用 JSON Envelope。

消息至少包含：

```json
{
  "version": "1",
  "message_id": "msg_001",
  "type": "command",
  "action": "send_prompt",
  "request_id": "req_001",
  "source": {
    "type": "device",
    "id": "device_001"
  },
  "target": {
    "node_id": "node_001",
    "agent_id": "agent_001",
    "session_id": "session_001"
  },
  "timestamp": "2026-11-03T10:00:00Z",
  "payload": {
    "text": "分析当前项目的目录结构"
  }
}
```

注意：

- message_id：每条消息唯一标识。
- request_id：关联请求和响应。
- source：消息来源。
- target：消息目标。
- action：具体操作。
- payload：业务参数。
- timestamp：消息产生时间。
- version：协议版本。

所有消息必须遵守统一 Envelope。

## 4.3 第三层：Domain Layer

领域模型层。

不直接暴露 OpenCode、Codex 等产品的原生数据结构。

核心实体：

### Node

代表一台运行 MeetAgent Node 的机器。

```text
Node
├── node_id
├── name
├── hostname
├── platform
├── version
├── status
└── last_seen
```

### AgentInstance

代表某个具体运行实例。

```text
AgentInstance
├── agent_id
├── node_id
├── type
├── name
├── status
└── capabilities
```

注意：Agent 类型和 Agent Instance 必须分离。

一台机器可以同时存在多个相同类型的 Agent Instance。

### Session

代表 Agent 的一个会话。

```text
Session
├── session_id
├── agent_id
├── title
├── working_directory
├── status
└── updated_at
```

### AgentEvent

代表 Agent 产生的事件。

```text
AgentEvent
├── event_id
├── node_id
├── agent_id
├── session_id
├── event_type
├── payload
└── timestamp
```

### PermissionRequest

代表 Agent 发起的权限请求。

```text
PermissionRequest
├── permission_id
├── agent_id
├── session_id
├── description
├── status
├── expires_at
└── created_at
```

以上是平台内部概念，不要求每个 Agent 都能提供所有字段。

对于不支持的字段，应该明确标记为 unknown 或 unsupported，而不是伪造数据。

## 4.4 第四层：Adapter Layer

Adapter 是整个系统实现多 Agent 扩展的核心。

每种 Agent 使用独立 Adapter。

```text
Adapter Manager
      |
      +-- OpenCode Adapter
      |
      +-- Codex Adapter
      |
      +-- Pi Adapter
      |
      +-- Claude Adapter
```

Adapter 负责：

1. 建立 Agent 原生连接。
2. 发现 Agent 状态。
3. 获取 Session。
4. 发送 Prompt。
5. 中断执行。
6. 订阅运行事件。
7. 处理权限请求。
8. 将原生事件转换为统一领域事件。

建议定义接口：

```go
type AgentAdapter interface {
    Type() string

    Connect(ctx context.Context) error

    Disconnect(ctx context.Context) error

    Capabilities() Capabilities

    ListSessions(ctx context.Context) ([]Session, error)

    SendPrompt(
        ctx context.Context,
        sessionID string,
        prompt string,
    ) error

    Interrupt(
        ctx context.Context,
        sessionID string,
    ) error

    SubscribeEvents(
        ctx context.Context,
    ) (<-chan AgentEvent, error)
}
```

权限审批应设计为可选能力，不能假设每个 Agent 都支持相同的审批机制。

可以进一步拆分为：

```go
type PermissionResponder interface {
    RespondPermission(
        ctx context.Context,
        permissionID string,
        decision string,
    ) error
}
```

Capabilities 示例：

```json
{
  "list_sessions": true,
  "send_prompt": true,
  "interrupt": true,
  "subscribe_events": true,
  "permission_response": false,
  "switch_model": false
}
```

不要将 pause、resume、switch_model 等能力强制定义为所有 Agent 的必备功能。

## 4.5 第五层：Application Layer

应用服务层。

将底层能力组织成平台业务操作。

例如：

```text
SendPromptUseCase
InterruptSessionUseCase
ListAgentsUseCase
ListSessionsUseCase
GetAgentStatusUseCase
RespondPermissionUseCase
```

Cloud 和 Node 都应有各自的应用服务。

Cloud 负责远程授权、路由和状态管理。

Node 负责本地 Agent 操作和原生协议执行。

---

# 5. Node 内部设计

Node 是本地常驻进程，建议内部使用模块化架构。

```text
MeetAgent Node
│
├── Connection Manager
│   ├── Cloud Connection
│   ├── Heartbeat
│   └── Reconnect
│
├── Agent Manager
│   ├── Agent Registry
│   ├── Agent Discovery
│   └── Instance Manager
│
├── Adapter Manager
│   ├── OpenCode Adapter
│   ├── Pi Adapter
│   └── Other Adapters
│
├── Event Collector
│   ├── Event Normalizer
│   └── Local Event Bus
│
├── Command Executor
│   ├── Command Dispatcher
│   ├── Capability Checker
│   └── Local Policy
│
├── State Manager
│   └── Local Snapshot
│
└── Configuration
```

## 5.1 Agent Discovery

第一版不需要实现复杂的自动发现。

采用手动配置 + 有限自动发现。

例如：

```yaml
node:
  name: developer-machine

agents:
  - id: opencode-main
    type: opencode
    enabled: true
    connection:
      mode: http
      base_url: http://127.0.0.1:4096

  - id: pi-main
    type: pi
    enabled: false
    connection:
      mode: process
      command: pi
      args:
        - --mode
        - rpc
        - --no-session
```

注意：

- 配置中的命令参数必须经过明确授权。
- 不允许远程客户端任意修改或执行 Shell 命令。
- Adapter 只能使用预先定义的接入方式。
- 自动发现只负责发现候选服务，不能默认授予操作权限。

## 5.2 Adapter 生命周期

每个 Adapter 具有独立生命周期：

```text
CREATED
   |
CONNECTING
   |
CONNECTED
   |
SUBSCRIBING
   |
READY
   |
DISCONNECTING
   |
STOPPED
```

Adapter 故障不应该直接导致整个 Node 崩溃。

一个 Agent 不可用，不应影响同一 Node 上其他 Agent 的运行。

## 5.3 Adapter 扩展方式

第一版 Adapter 使用 Go 内部模块实现，不使用 Go Plugin 动态插件机制。

原因：

- Go Plugin 存在平台和构建限制。
- 版本兼容性复杂。
- 初期没有必要承担动态加载的复杂度。

当前采用：

```text
Node
  |
  +-- Adapter Interface
          |
          +-- OpenCode Implementation
          +-- Pi Implementation
          +-- Codex Implementation
```

后续如果确实需要第三方独立开发 Adapter，再考虑子进程插件协议。

---

# 6. Cloud 内部设计

Cloud 是统一消息中枢。

第一版采用模块化单体。

```text
MeetAgent Cloud
│
├── Transport
│   └── WebSocket Server
│
├── Connection Manager
│   ├── Node Connections
│   └── Device Connections
│
├── Device Registry
│
├── Node Registry
│
├── Message Router
│
├── Event Hub
│
├── State Manager
│
├── Event Cache
│
├── Auth & Policy
│
├── Audit
│
└── REST API
```

## 6.1 Connection Manager

维护所有在线连接。

需要分别维护：

```text
Node Connections
Device Connections
```

每个连接都有：

- 身份 ID。
- 连接 ID。
- 建立时间。
- 最后心跳时间。
- 连接状态。
- 协议版本。

同一个设备重新连接时，应替换或清理旧连接，避免出现多个连接争抢同一身份的问题。

## 6.2 Message Router

负责消息路由。

例如：

```text
Device
  |
  | send_prompt
  v
Cloud Router
  |
  | 根据 node_id 找到 Node 连接
  v
Node
  |
  v
OpenCode Adapter
```

Cloud 不负责解释 OpenCode 的原生接口。

它只验证身份、目标和消息类型，然后完成转发。

## 6.3 Event Hub

负责 Agent 事件的分发。

例如：

```text
OpenCode
   |
   v
Node Adapter
   |
   v
Event Normalizer
   |
   v
Cloud Event Hub
   |
   +---- ESP32
   |
   +---- Web Client
```

## 6.4 Event Cache

第一版不需要部署 Redis 或 Kafka。

可以先使用内存缓存，按 Node、Agent、Session 保存最近一段时间的事件。

但必须注意：

- 缓存需要有容量上限。
- 事件需要有序列号。
- 服务重启后允许丢失部分短期事件。
- 客户端必须能够重新获取 Snapshot。

如果后续需要跨重启保存事件，再引入 PostgreSQL 等持久化方案。

## 6.5 状态存储

建议区分三类数据：

| 数据 | 存储方式 |
|---|---|
| 设备身份、Node 注册信息 | 数据库 |
| 当前在线状态 | 内存状态 + 心跳 |
| 短期事件 | 有界缓存 |
| 完整 Agent 会话历史 | Agent 原生存储，按需查询 |

Cloud 默认不存储完整 Prompt、代码内容和工具输出。

这是为了降低云端的数据安全风险和存储压力。

---

# 7. 数据流向设计

本节定义核心业务的数据流。

## 7.1 Node 注册流程

```text
MeetAgent Node
       |
       | 1. WSS Connect
       v
MeetAgent Cloud
       |
       | 2. Authentication
       v
Connection Manager
       |
       | 3. Register Node
       v
Node Registry
       |
       | 4. Return Node Configuration
       v
MeetAgent Node
       |
       | 5. Discover / Configure Agents
       v
Adapter Manager
       |
       | 6. Connect to OpenCode
       v
OpenCode
```

Node 认证成功后，注册自身信息和可接入 Agent。

Cloud 保存 Node 的在线状态。

## 7.2 Device 注册流程

```text
ESP32
  |
  | WSS Connect
  v
Cloud
  |
  | Device Authentication
  v
Device Registry
  |
  | Register Device
  v
Cloud
  |
  | Return Authorized Node List
  v
ESP32
```

设备首次配对采用一次性配对码或其他安全的配对机制。

配对成功后，为每台设备分配独立凭据。

## 7.3 远程发送 Prompt

```text
ESP32
  |
  | send_prompt
  v
Cloud
  |
  | Authentication
  | Authorization
  | Validate Target
  v
Message Router
  |
  | Forward Command
  v
MeetAgent Node
  |
  | Capability Check
  | Local Policy Check
  v
OpenCode Adapter
  |
  | Native API
  v
OpenCode
```

结果返回：

```text
OpenCode
   |
   v
Adapter
   |
   v
Node
   |
   v
Cloud
   |
   v
ESP32
```

命令执行状态需要区分：

- accepted：已经被目标接收。
- executing：正在执行。
- completed：执行成功。
- failed：执行失败。
- rejected：权限或能力不允许。
- timeout：执行超时。

不能将 Cloud 成功转发消息直接等同于 Agent 已经执行成功。

## 7.4 Agent 事件流

例如 Agent 开始调用工具：

```text
OpenCode
   |
   | 原生事件
   v
OpenCode Adapter
   |
   | Normalize
   v
AgentEvent
   |
   v
Local Event Bus
   |
   v
Node
   |
   | WSS
   v
Cloud Event Hub
   |
   v
Device / Web Client
```

统一事件示例：

```json
{
  "event_id": "evt_001",
  "sequence": 1024,
  "event_type": "tool.started",
  "node_id": "node_001",
  "agent_id": "opencode-main",
  "session_id": "session_001",
  "timestamp": "2026-11-03T10:00:00Z",
  "payload": {
    "tool_name": "read_file",
    "status": "running"
  }
}
```

事件类型建议：

```text
agent.connected
agent.disconnected
agent.status_changed

session.created
session.updated
session.status_changed

message.started
message.delta
message.completed

tool.started
tool.completed
tool.failed

permission.requested
permission.resolved

agent.error
```

并不是每个 Agent 都会提供全部事件。Adapter 应负责将可获取的信息转换为统一事件。

## 7.5 权限审批流程

```text
Agent
  |
  | Permission Request
  v
Node Adapter
  |
  v
Cloud
  |
  v
ESP32
  |
  | Approve / Reject
  v
Cloud
  |
  v
Node
  |
  | Validate Permission
  v
Agent Adapter
  |
  v
Agent
```

权限审批必须遵守 Agent 原生安全机制。

MeetAgent 不应该绕过 Agent 自身的权限系统。

审批需要：

- 唯一 permission_id。
- 过期时间。
- 请求来源校验。
- Session 关联。
- 防止重复提交。
- 明确审批结果。

审批超时或目标离线时，不允许默认批准。

---

# 8. 统一协议设计

建议第一版定义以下消息类型：

| 类型 | 方向 | 作用 |
|---|---|---|
| register | 双向 | 节点或设备注册 |
| heartbeat | 双向 | 心跳 |
| command | Device → Cloud → Node | 远程命令 |
| response | Node → Cloud → Device | 命令结果 |
| event | Node → Cloud → Device | Agent 事件 |
| sync | 双向 | 状态同步 |
| error | 双向 | 错误报告 |

## 8.1 命令示例

发送 Prompt：

```json
{
  "type": "command",
  "action": "send_prompt",
  "request_id": "req_001",
  "target": {
    "node_id": "node_001",
    "agent_id": "opencode-main",
    "session_id": "session_001"
  },
  "payload": {
    "text": "分析当前项目的架构"
  }
}
```

中断任务：

```json
{
  "type": "command",
  "action": "interrupt",
  "request_id": "req_002",
  "target": {
    "node_id": "node_001",
    "agent_id": "opencode-main",
    "session_id": "session_001"
  },
  "payload": {}
}
```

权限响应：

```json
{
  "type": "command",
  "action": "permission_response",
  "request_id": "req_003",
  "target": {
    "node_id": "node_001",
    "agent_id": "opencode-main",
    "session_id": "session_001"
  },
  "payload": {
    "permission_id": "permission_001",
    "decision": "approve"
  }
}
```

## 8.2 消息可靠性

必须实现：

1. 每个请求都有 request_id。
2. 每个事件都有 event_id。
3. 事件具有递增 sequence。
4. 命令执行结果可以关联原始请求。
5. 重复请求不能导致危险操作重复执行。
6. 客户端重新连接后可以请求 Snapshot。
7. 支持从最后一个已确认 sequence 开始补发缓存事件。

对于不可安全重试的命令，不应在网络断开后自动重新执行。

---

# 9. 连接管理与状态恢复

## 9.1 Node 连接状态

```text
STARTING
   |
CONNECTING
   |
AUTHENTICATING
   |
REGISTERING
   |
SYNCING
   |
ONLINE
   |
DISCONNECTED
   |
RECONNECTING
   |
SYNCING
   |
ONLINE
```

## 9.2 Device 连接状态

```text
BOOT
  |
WIFI_CONNECTING
  |
GATEWAY_CONNECTING
  |
AUTHENTICATING
  |
SYNCING
  |
ONLINE
```

断线后进入：

```text
RECONNECTING
```

## 9.3 心跳策略

初始建议：

- 每 20–30 秒发送一次心跳。
- 连续多个心跳周期未收到消息，标记连接异常。
- 连接断开后进行指数退避重连。
- 重连成功后重新注册并进行状态同步。

具体超时参数需要通过网络测试调整。

## 9.4 Snapshot + Event Replay

重连时不能只恢复 WebSocket 连接。

客户端需要重新获取：

```text
Node List
Agent List
Session List
Current Status
Pending Permissions
Recent Events
```

如果 Cloud 仍保留对应事件序列，则可以从最后确认的 sequence 补发事件。

如果缓存已经过期，则重新获取 Snapshot。

---

# 10. 安全设计

安全设计不能等到项目完成后再补。

## 10.1 身份认证

至少需要两种身份：

- Node Identity。
- Device Identity。

每个 Node 和 Device 都拥有独立凭据。

建议：

- 首次使用一次性配对码。
- 配对完成后分配独立设备 Token 或密钥。
- 支持凭据撤销。
- 支持凭据轮换。
- 所有连接使用 TLS。

## 10.2 权限分层

必须区分三个层次：

### Device Permission

终端能否访问某个 Node。

### Agent Permission

终端能否查看、发送 Prompt、中断或审批某个 Agent。

### Runtime Permission

Agent 自身是否允许执行具体操作。

第三层不能被 MeetAgent 绕过。

## 10.3 本地安全

Node 不暴露任意 Shell 执行接口。

远程命令必须采用明确的 Action 白名单。

例如：

```text
send_prompt
interrupt
list_sessions
get_status
permission_response
```

不允许通过：

```json
{
  "command": "rm -rf ..."
}
```

这种形式直接执行任意命令。

Node 只接受合法协议消息，并对所有目标和参数进行校验。

---

# 11. 项目目录结构

建议使用一个主仓库管理 Cloud、Node 和协议定义。

```text
meetagent/
│
├── cmd/
│   ├── cloud/
│   │   └── main.go
│   │
│   └── node/
│       └── main.go
│
├── internal/
│   │
│   ├── cloud/
│   │   ├── transport/
│   │   ├── connection/
│   │   ├── router/
│   │   ├── registry/
│   │   ├── eventhub/
│   │   ├── cache/
│   │   ├── auth/
│   │   └── api/
│   │
│   ├── node/
│   │   ├── connection/
│   │   ├── discovery/
│   │   ├── agentmanager/
│   │   ├── adapter/
│   │   ├── event/
│   │   ├── command/
│   │   ├── security/
│   │   └── config/
│   │
│   ├── adapters/
│   │   └── opencode/
│   │
│   ├── domain/
│   │   ├── node.go
│   │   ├── agent.go
│   │   ├── session.go
│   │   ├── event.go
│   │   └── permission.go
│   │
│   └── application/
│
├── pkg/
│   └── protocol/
│       ├── envelope.go
│       ├── command.go
│       ├── response.go
│       ├── event.go
│       └── sync.go
│
├── web/
│   └── test-client/
│
├── configs/
│
├── deployments/
│
├── docs/
│
├── go.mod
└── README.md
```

ESP32 使用独立仓库：

```text
agent-terminal/
│
├── main/
├── components/
│   ├── network/
│   ├── protocol/
│   ├── display/
│   ├── ui/
│   ├── input/
│   ├── storage/
│   └── power/
│
├── managed_components/
├── sdkconfig.defaults
├── CMakeLists.txt
└── README.md
```

---

# 12. ESP32 UI 设计

第一版 UI 不追求复杂动画，优先确保信息密度、响应速度和可操作性。

建议采用五个页面。

## 12.1 Dashboard

显示：

- Cloud 连接状态。
- Node 在线数量。
- Agent 在线数量。
- 当前选中的 Agent。
- 当前 Session。
- Agent 执行状态。
- 待审批数量。

## 12.2 Session

显示：

- Agent 列表。
- Session 列表。
- 当前工作目录。
- Session 状态。
- 切换 Session。

## 12.3 Activity

显示实时执行事件：

```text
10:21:01  Agent started
10:21:04  Reading main.go
10:21:08  Tool: read_file
10:21:12  Tool: search
10:21:18  Agent completed
```

采用事件流和有限长度的本地显示缓冲区。

## 12.4 Prompt

提供：

- 输入框。
- 发送按钮。
- 中断按钮。
- 当前 Session 信息。

MVP 可以先使用触摸屏虚拟键盘。

后续考虑 BLE 键盘或语音输入。

## 12.5 Approval

显示：

- 权限请求内容。
- 请求所属 Agent。
- 请求所属 Session。
- 请求时间和过期时间。
- Approve / Reject。

必须清楚展示当前操作的目标，避免误审批。

---

# 13. 分阶段开发计划

整个项目按依赖关系逐步实现，不建议同时开发三个端。

## Phase 0：通信架构验证

目标：验证 Cloud 能否作为两个不同网络环境下设备的双向消息中枢。

开发内容：

1. 创建 Go 项目。
2. 实现 Cloud WebSocket Server。
3. 实现 Node WebSocket Client。
4. 实现 WebSocket Test Client。
5. 实现 Node 和 Device 身份区分。
6. 实现基本消息路由。
7. 实现心跳和断线重连。

此阶段暂时不接入任何 Coding Agent。

验收标准：

- Node 能够连接 Cloud。
- Web Client 能够连接 Cloud。
- 两端无需公网 IP。
- 两端无需端口映射。
- Web Client 可以向 Node 发送消息。
- Node 可以向 Web Client 返回消息。
- 断网后可以自动恢复。
- 能够正确识别在线和离线状态。

**这是整个项目最优先的验证阶段。**

## Phase 1：统一协议与 Cloud 基础设施

目标：从简单消息转发演进为正式的通信中枢。

开发内容：

1. 定义统一 Envelope。
2. 定义 Node、Device、Agent 身份模型。
3. 实现 Connection Manager。
4. 实现 Node Registry。
5. 实现 Device Registry。
6. 实现 Message Router。
7. 实现基础 Auth。
8. 实现 Event Cache。
9. 实现 Snapshot 同步。

验收标准：

- 多个 Node 可以同时在线。
- 多个 Device 可以同时在线。
- 不同 Device 可以访问被授权的 Node。
- 消息能正确路由。
- 不同设备之间不会串消息。
- 重连后可以恢复基础状态。

## Phase 2：Node 与 Adapter 框架

目标：让 Node 真正成为 Agent 的本地接入层。

开发内容：

1. 实现独立 Node 可执行程序。
2. 实现配置管理。
3. 实现 AgentInstance 模型。
4. 实现 Adapter Interface。
5. 实现 Adapter Manager。
6. 实现 Event Normalizer。
7. 实现 Command Executor。
8. 实现 Capability 检查。
9. 实现本地安全策略。

第一版只接入 OpenCode。

验收标准：

- Node 能够连接 OpenCode。
- Node 能获取 Agent 状态。
- Node 能获取 Session 列表。
- Node 能订阅 OpenCode 事件。
- Node 能向指定 Session 发送 Prompt。
- Node 能中断指定 Session。
- Agent 事件能够转换为统一事件。

## Phase 3：完整远程交互

目标：打通 Cloud、Node、Agent 的实际业务链路。

开发内容：

1. 远程获取 Agent 列表。
2. 远程获取 Session 列表。
3. 实现 Prompt。
4. 实现 Abort / Interrupt。
5. 实现 Activity 事件流。
6. 实现权限请求转发。
7. 实现权限响应。
8. 实现命令执行状态。
9. 实现 Snapshot + Event Replay。

验收标准：

- 浏览器可以远程操作 OpenCode。
- Prompt 能够正确发送到目标 Session。
- Agent 运行事件能够实时返回。
- Interrupt 能够中断目标任务。
- 权限请求可以远程处理。
- 断线恢复后能够恢复当前 Agent 状态。

此阶段使用浏览器测试客户端，不需要 ESP32。

## Phase 4：ESP32 Terminal

目标：将已经验证的远程控制能力迁移到便携设备。

开发内容：

1. ESP-IDF 工程初始化。
2. Wi-Fi 管理。
3. WSS 连接。
4. TLS 证书校验。
5. Device 身份认证。
6. JSON 协议解析。
7. FreeRTOS 任务设计。
8. LVGL 页面实现。
9. Agent / Session 列表。
10. Activity 页面。
11. Prompt 输入。
12. Abort 操作。
13. Approval 页面。
14. 断线重连和状态恢复。

验收标准：

- ESP32 能够稳定连接 Cloud。
- 能够显示在线 Node 和 Agent。
- 能够切换 Session。
- 能够查看运行事件。
- 能够发送 Prompt。
- 能够中断 Agent。
- 能够处理权限请求。
- Wi-Fi 断开后能够恢复。

## Phase 5：系统完善

第一版跑通后，再考虑：

- Codex Adapter。
- Claude Code Adapter。
- Pi Adapter。
- 多 Node 管理。
- 多终端管理。
- Web Dashboard。
- 完整设备配对管理。
- 配置管理界面。
- 更完善的权限模型。
- PostgreSQL 持久化。
- Adapter 独立进程机制。

---

# 14. MVP 验收标准汇总

MVP 完成的标志不是 ESP32 页面开发完成，而是整个系统的端到端链路稳定。

| 验收项目 | 结果要求 |
|---|---|
| 双端公网连接 | 不依赖端口映射 |
| Node 注册 | 能够正确注册并维持在线状态 |
| Device 注册 | 能够正确认证并注册 |
| 多连接管理 | 支持多个 Node / Device |
| 消息路由 | 能够正确找到目标 Node |
| Agent 接入 | OpenCode 接入成功 |
| Agent 状态 | 能够远程获取 |
| Session 管理 | 能够远程查询和切换 |
| Prompt | 能够远程发送 |
| Interrupt | 能够远程中断 |
| Activity | 能够实时显示 |
| Approval | 能够在 Agent 原生机制允许的情况下远程处理 |
| 断线恢复 | 能够重新同步状态 |
| 安全 | 不能通过终端执行任意 Shell |

---

# 15. 关键设计决策

本项目在 MVP 阶段需要坚持以下原则。

### 15.1 Cloud 不等于 Agent Runtime

Cloud 只负责连接、路由、状态和协调。

Agent 始终运行在用户自己的机器上。

这样可以避免云端承担 Agent 执行环境，也减少用户将项目代码、Prompt 和工具输出上传到云端的必要性。

### 15.2 Node 是独立进程

Node 不作为 MCP Server 实现。

它是独立常驻进程，负责本地 Agent 生命周期和协议接入。

MCP 可以在未来成为 Agent 的一种工具接入能力，但不是 MeetAgent 的通信主体。

### 15.3 Adapter 是扩展边界

新增 Agent 不应该修改 Cloud 的业务逻辑。

新增 Agent 的主要工作应该是实现对应 Adapter，并将原生能力映射为统一领域模型。

### 15.4 能力声明优先于强制统一

不同 Agent 的能力并不完全相同。

平台应当通过 Capabilities 告知客户端当前 Agent 支持什么操作。

不能为了统一 UI 而假设每种 Agent 都支持相同的功能。

### 15.5 远程命令必须有明确语义

不开放任意 Shell。

每个远程操作必须有明确的 Action、目标、参数、权限和执行状态。

### 15.6 先验证链路，再做终端

先使用 Web Client 验证完整通信架构。

如果双端出站 WSS、消息路由和重连机制没有跑通，就不进入 ESP32 UI 开发阶段。

### 15.7 模块化单体优先

第一版不使用微服务，不使用复杂消息队列，不提前引入 Kubernetes。

先保证系统边界清晰、模块职责明确、接口稳定。

---

# 16. 第一阶段具体开发任务

建议立即开始 Phase 0。

第一阶段只创建以下三个组件：

```text
meetagent/
│
├── cmd/
│   ├── cloud/
│   └── node/
│
├── internal/
│   ├── cloud/
│   │   ├── connection/
│   │   └── router/
│   │
│   └── node/
│       └── connection/
│
├── pkg/
│   └── protocol/
│
└── web/
    └── test-client/
```

第一阶段只需要实现：

1. Cloud 启动 WebSocket Server。
2. Node 连接 Cloud。
3. Web Client 连接 Cloud。
4. Node 和 Web Client 完成身份注册。
5. Cloud 能够识别两种连接。
6. Web Client 向 Node 发送测试命令。
7. Node 返回响应。
8. Cloud 将响应转发给 Web Client。
9. 实现心跳和重连。

暂时不实现 Agent Adapter、数据库、复杂权限、ESP32 和完整领域模型。

第一阶段完成后，再正式进入协议和领域模型的扩展。

**最终开发原则：先建立可靠的远程通信基础设施，再建立 Agent 接入能力，最后实现多端交互体验。**
