# MeetAgent

面向 Coding Agent 的远程监控、交互与控制平台。

当前进度：**Phase 3 — 完整远程交互（真机闭环）**。本阶段不新增协议字段，而是用真实 OpenCode 把 Phase 2 已实现的 Prompt、Interrupt、权限审批与断线恢复四条链路跑成闭环，并根据真机结果加固。Phase 2 的可插拔 Adapter 层、统一事件、快照与补发继续沿用；Cloud 仍不直接接触任何 Agent 原生协议。

> 开发设计文档见 [`MeetAgent.md`](./MeetAgent.md)。

---

## 架构

```text
+---------------------+         WSS          +---------------------------+
|  Web Test Client    | <------------------> |                           |
|  Vue 3 + Vite       |                      |     MeetAgent Cloud       |
+---------------------+                      |                           |
                                             |  Auth / ACL               |
+---------------------+         WSS          |  Connection Manager       |
|  MeetAgent Node     | <------------------> |  Node / Device Registry   |
|  Go 常驻进程        |                      |  Message Router           |
|  Adapter Manager    |                      |  Event Cache / Sequence   |
+----------+----------+                      +---------------------------+
           | HTTP / SSE（本机）
+----------v----------+
|  OpenCode Server    |
+---------------------+
```

- **Cloud**：WebSocket 中枢，负责认证与授权、连接管理、Node / Device 注册表、消息路由、在途请求追踪、事件缓存与快照生成。
- **Node**：运行在 Agent 所在机器上的常驻进程，主动向 Cloud 建立出站连接。它通过 Adapter Manager 管理本机 Agent，并把 Agent 库存与归一化事件实时上报。
- **Adapter**：Node 内部的 Go 模块，把某种 Agent 产品的原生 HTTP / SSE 接口翻译为统一模型。Phase 2 提供 OpenCode Adapter；新增产品不需要修改 Cloud 或协议。
- **Web Test Client**：浏览器端测试终端，和 Node 一样主动连接 Cloud，注册后获得按 ACL 过滤的 Snapshot。

两端都是出站连接，所以 NAT、公司网络、手机热点后也能工作。

---

## 目录结构

```text
.
├── cmd/
│   ├── cloud/                 # meetagent-cloud 入口
│   └── node/                  # meetagent-node 入口
├── internal/
│   ├── cloud/
│   │   ├── auth/              # 静态凭据与 Device → Node ACL
│   │   ├── config/            # Cloud 配置
│   │   ├── connection/        # 连接、Node/Device 注册表、pending 请求
│   │   ├── eventcache/        # 有界事件缓存、sequence、TTL、权限派生
│   │   ├── router/            # 授权、消息分发、快照、事件补发
│   │   └── server/            # HTTP / WebSocket 服务
│   └── node/
│       ├── adapter/           # Adapter 接口、生命周期 Manager、事件归一化
│       │   └── opencode/      # OpenCode HTTP + SSE Adapter
│       ├── command/           # 白名单命令执行器 + 本地策略
│       ├── config/            # Node 配置、Agent 声明与安全策略
│       └── connection/        # 出站连接、心跳、重连、事件与库存上报
├── pkg/
│   └── protocol/              # 统一 Envelope 与 payload 类型
├── configs/
│   ├── cloud.yaml             # 本地开发配置（2 Node / 2 Device）
│   ├── node.yaml              # node-001
│   └── node-002.yaml          # node-002（多 Node 验收）
├── web/
│   └── test-client/           # Vue 3 + TypeScript + Vite
├── scripts/
│   └── e2e/                   # Phase 3 真机 E2E（Chrome DevTools Protocol）
├── Makefile
└── MeetAgent.md
```

---

## 快速开始

### 环境要求

- Go 1.24+
- Node.js 20.19+（Vite 8 要求）

### 1. 启动 Cloud

```bash
make run-cloud
# 等价于：go run ./cmd/cloud -config configs/cloud.yaml
```

默认监听 `127.0.0.1:8080`，WebSocket 路径 `/ws`。

### 2. 准备 OpenCode

Node 通过本机 HTTP + SSE 连接 OpenCode（v2 API），使用 HTTP Basic 认证：用户名固定为 `opencode`，密码来自配置或环境变量。

```bash
# 若使用 OpenCode 后台服务
export OPENCODE_PASSWORD="$(opencode service get password)"

# 若手动运行 opencode serve，密码见其启动输出
# export OPENCODE_PASSWORD="<serve 输出的 password>"
```

`configs/node.yaml` 默认连接 `http://127.0.0.1:49374`。请按实际监听地址修改 `agents[].connection.base_url`。

### 3. 启动 Node（各开一个终端）

```bash
make run-node
# 等价于：go run ./cmd/node -config configs/node.yaml

go run ./cmd/node -config configs/node-002.yaml
```

两个 Node 都应输出 `node online` 与 `agent adapter connected`。Cloud 会为每次上线生成 `node.online` 事件，并广播给有权限的 Device。

> `OPENCODE_PASSWORD` 需要在启动 Node 的 shell 中导出；`password_env` 在启动时解析，未设置时配置校验会直接失败。

### 4. 启动 Web 测试客户端（新终端）

```bash
make web-install   # 首次运行
make web-dev
```

打开 <http://127.0.0.1:5173>。开发凭据：

| Device ID | Token | 可访问 Node |
|---|---|---|
| `web-001` | `dev-device-token-change-me` | 全部（`"*"`） |
| `web-002` | `dev-device-002-token-change-me` | 仅 `node-001` |

Cloud URL 使用 `ws://127.0.0.1:8080/ws`。

用 `web-001` 登录可以看到两个 Node；用 `web-002` 登录只能看到 `node-001`。选择 Node 后，界面会展示该 Node 上报的实时 Agent 与 Session：

- `ping` / `echo` / `get_status`：Node 级基础动作。
- `list_sessions`：让 Node 通过 Adapter 重新拉取所选 Agent 的 Session。
- `send_prompt`：向所选 Session 发送 Prompt（Ctrl / Cmd + Enter 发送）。
- `interrupt`：中断所选 Session 的当前执行。
- 权限面板：当 OpenCode 发起 `permission.asked` 时出现，可允许一次、总是允许或拒绝。

`node-002` 与本地开发配置 `configs/node.yaml`（`node-001`）都显式开启了 `permission_response`，因此权限审批按钮对它们的 Session 生效。该动作在协议层仍是可选项，生产配置应按需评估后再开启。

### 5. 验证

```bash
curl -s http://127.0.0.1:8080/healthz
curl -s -H "Authorization: Bearer dev-admin-token-change-me" http://127.0.0.1:8080/api/v1/status
curl -s -H "Authorization: Bearer dev-admin-token-change-me" http://127.0.0.1:8080/api/v1/snapshot
```

`/healthz` 是唯一无需认证的探活端点。`/api/v1/status` 会返回在线 Node / Device 数量、Device 的 `allowed_nodes` 与当前缓存事件数；`/api/v1/snapshot` 返回未过滤的运维视图。两者都需要 `server.admin_token` 对应的 `Authorization: Bearer` 头，缺失或错误返回 `401`。

`make test` 运行完整 Go 测试（含 race detector）；`make web-build` 做 TypeScript 类型检查并构建前端。

真机 E2E（Prompt / Interrupt / 权限批准与拒绝 / 断线恢复）见 [`scripts/e2e/README.md`](./scripts/e2e/README.md)，需要 Chromium、运行中的 Cloud / Node 与 `vite preview`。

---

## 配置

### Cloud：`configs/cloud.yaml`

```yaml
server:
  addr: "127.0.0.1:8080"
  ws_path: "/ws"
  admin_token: "dev-admin-token-change-me"
  allowed_origins:
    - "http://localhost:5173"
    - "http://127.0.0.1:5173"
    - "http://localhost:4173"
    - "http://127.0.0.1:4173"
  read_timeout: 90s
  ping_interval: 25s
  register_timeout: 10s
  pending_ttl: 2m
  max_pending: 10000
  rate_limit_per_second: 100
  rate_limit_burst: 200

auth:
  nodes:
    node-001:
      token: "dev-node-token-change-me"
    node-002:
      token: "dev-node-002-token-change-me"
  devices:
    web-001:
      token: "dev-device-token-change-me"
      allowed_nodes: ["*"]
    web-002:
      token: "dev-device-002-token-change-me"
      allowed_nodes: ["node-001"]

event_cache:
  capacity: 5000
  ttl: 10m
  snapshot_events: 50
  replay_limit: 1000

log:
  level: info
```

- `auth.nodes` / `auth.devices`：身份 ID → 静态 Token。Phase 1 仍是简化认证；配对码与每设备凭据在后续阶段实现。
- `allowed_nodes`：Device 可访问的 Node 白名单，支持 `"*"`。**必须显式配置且不能为空**，未列出的 Node 默认拒绝。
- `allowed_origins`：浏览器 WebSocket 的 Origin 白名单。Node 不发 Origin，直接放行。
- `pending_ttl` / `max_pending`：在途命令的过期时间与数量上限。
- `admin_token`：管理端点 `/api/v1/status`、`/api/v1/snapshot` 的 Bearer Token，**必填**。公网部署必须替换为强随机值。
- `rate_limit_per_second` / `rate_limit_burst`：单连接入站消息限流（令牌桶）。默认 100/s、突发 200；设为 `0` 可关闭。超限消息会被丢弃，并每秒最多返回一次 `rate_limited` 错误。
- 当 Node 断开时，Cloud 会立即把它名下所有在途请求以 `failed` 状态回给 Device，不必等客户端 15 秒超时。
- `event_cache`：
  - `capacity`：全局保留事件条数，超出后淘汰最旧事件。
  - `ttl`：事件保留时长。
  - `snapshot_events`：Snapshot 中携带的最近事件数量。
  - `replay_limit`：单次 `sync/events` 补发上限。

### Node：`configs/node.yaml`

```yaml
node:
  id: "node-001"
  name: "developer-machine"
  token: "dev-node-token-change-me"

  # Node 级本地安全策略：只能收窄协议白名单，不能扩展。
  policy:
    allowed_actions:
      - ping
      - echo
      - get_status
      - send_prompt
      - interrupt
      - list_sessions
    max_prompt_bytes: 32768

  agents:
    - id: "opencode-main"
      type: "opencode"
      name: "OpenCode Main"
      enabled: true
      connection:
        base_url: "http://127.0.0.1:49374"
        username: "opencode"
        password_env: "OPENCODE_PASSWORD"
        directory: ""          # 可选：只暴露某个 OpenCode 项目目录
        timeout: 15s
      capabilities:
        send_prompt: true
        interrupt: true
        list_sessions: true
      # 可选：覆盖 Node 级策略。非空列表会整体替换 Node 级 allowed_actions。
      # policy:
      #   allowed_actions: ["ping", "send_prompt"]
      #   max_prompt_bytes: 8192
      # 可选：静态 Session，仅用于兼容或占位；Adapter 在线时以实时列表为准。
      # sessions:
      #   - id: "session-demo"
      #     title: "Demo session"
      #     working_directory: "/home/dev/project"
      #     status: "idle"

cloud:
  url: "ws://127.0.0.1:8080/ws"
  register_timeout: 10s
  heartbeat_interval: 20s
  read_timeout: 60s
  request_timeout: 15s
  reconnect:
    initial_delay: 1s
    max_delay: 30s
    multiplier: 2
    jitter: 0.2

log:
  level: info
```

- `node.agents[].connection`：OpenCode 连接参数。`base_url` 必须是 `http(s)://`；密码使用 `password` 或 `password_env` 二选一，推荐后者。
- `enabled: false` 会保留声明但跳过启动与连接校验，方便临时关闭某个 Agent。
- `capabilities` 只能关闭 Adapter 已经声明支持的能力，不能凭空开启。
- 本地策略默认允许 `ping`、`echo`、`get_status`、`send_prompt`、`interrupt`、`list_sessions`。`permission_response` 在协议层默认**不开放**，必须显式加入 `allowed_actions`；`configs/node.yaml` 与 `configs/node-002.yaml` 这两个开发配置已开启。
- Agent 级 `policy` 非空时会整体替换 Node 级 `allowed_actions`，`max_prompt_bytes` 同理。
- 重连采用指数退避 + 抖动：`1s → 2s → 4s → 8s → 16s → 30s`（封顶）。
- `read_timeout` 必须至少为 `heartbeat_interval` 的两倍。

**多个 Agent 实例**：一个 Node 可以同时管理多个 Agent，每个 `node.agents[]` 条目对应一个独立的 Adapter 实例，`id` 必须唯一，并各自配置 `base_url`、`password_env` 与可选的 `directory`。Node **不会自动发现**本机的 OpenCode 进程，需要把每个 OpenCode Server 显式写进配置：

```yaml
agents:
  - id: "opencode-main"
    type: "opencode"
    name: "OpenCode Main"
    connection:
      base_url: "http://127.0.0.1:49374"
      password_env: "OPENCODE_PASSWORD"
  - id: "opencode-second"
    type: "opencode"
    name: "OpenCode Second"
    connection:
      base_url: "http://127.0.0.1:4096"
      password_env: "OPENCODE_SECOND_PASSWORD"
```

多个 TUI / 终端窗口如果连接的是同一个 OpenCode Server，只会表现为该 Agent 下的多个 Session，不会变成多个 Agent；只有独立的 OpenCode Server（不同端口/进程）才需要新增 `agents[]` 条目。

---

## Adapter 层

Adapter 是 Node 内部模块（不是 Go Plugin），每个实例实现统一接口：

```go
type AgentAdapter interface {
    Type() string
    Connect(ctx context.Context) error
    Disconnect(ctx context.Context) error
    Capabilities() Capabilities
    ListSessions(ctx context.Context) ([]Session, error)
    SendPrompt(ctx context.Context, sessionID, prompt string) (PromptResult, error)
    Interrupt(ctx context.Context, sessionID string) (InterruptResult, error)
    SubscribeEvents(ctx context.Context) (<-chan Event, error)
}

// 可选能力：没有权限模型的 Agent 可以不实现。
type PermissionResponder interface {
    RespondPermission(ctx context.Context, sessionID, permissionID, decision, message string) error
}
```

Adapter Manager 负责：

- 为每个实例启动独立生命周期：连接、刷新 Session、订阅事件、断线后指数退避重连。
- 维护 Agent 状态（`offline` / `connecting` / `online` / `busy` / `error`）与 Session 清单。
- 每 20 秒刷新一次 Session，修复 SSE 易失流造成的状态缺口。
- 在执行 `send_prompt` / `interrupt` / `permission_response` 前校验能力与目标 Session。
- 把 Session 差异转换为 `session.created` / `session.updated` / `session.status_change` / `session.deleted` 事件。
- 通过 `sync/agent_state` 把库存变化推给 Cloud；Cloud 更新 Node View 后广播 Snapshot。

### OpenCode 事件映射

| OpenCode 事件 | 统一事件 |
|---|---|
| `session.created` | `session.created` |
| `session.updated` / `session.renamed` | `session.updated` |
| `session.deleted` / `session.removed` | `session.deleted` |
| `session.execution.started` | `session.status_change`（running） |
| `session.execution.succeeded` / `interrupted` / `session.idle` | `session.status_change`（idle） |
| `session.execution.failed` | `agent.error` |
| `session.text.started` | `message.started` |
| `session.text.delta` | `message.delta`（`delta` 为增量文本） |
| `session.text.ended` | `message.completed` |
| `session.tool.called` | `tool.started` |
| `session.tool.success` | `tool.completed` |
| `session.tool.failed` / `session.tool.error` | `tool.failed` |
| `permission.asked` | `permission.requested` |
| `permission.replied` | `permission.resolved` |

未知事件会被忽略，因此 OpenCode 小版本升级不会打断 Node。SSE 断线由 Adapter 自动重连，Manager 的周期刷新负责补齐缺口。

---

## 协议概要

### Envelope

所有消息使用统一 Envelope：

```json
{
  "version": "1",
  "message_id": "msg_...",
  "type": "command",
  "action": "ping",
  "request_id": "req_...",
  "source": { "type": "device", "id": "web-001" },
  "target": { "node_id": "node-001" },
  "timestamp": "2026-11-04T10:00:00Z",
  "payload": { "nonce": "n1" }
}
```

消息类型：`register`、`heartbeat`、`command`、`response`、`event`、`sync`、`error`。
Peer 类型：`node`、`device`、`cloud`。

### 身份模型

| 实体 | 说明 |
|---|---|
| Node | 一台运行 `meetagent-node` 的机器，注册时声明 `agents` / `sessions` |
| Agent | Node 上托管的一个 Coding Agent 实例，归属某个 Node |
| Session | Agent 下的会话，归属某个 Agent |
| Device | 浏览器 / 移动端等控制端，必须配置 `allowed_nodes` |

目标校验规则：

- Device 只能操作 `allowed_nodes` 内的 Node。
- 指定 `agent_id` / `session_id` 时，Cloud 会校验其确实属于目标 Node，避免跨 Node 伪造目标。
- 未授权或未知目标一律拒绝，不会泄露其存在性。

### 命令白名单

| Action | 说明 |
|---|---|
| `ping` | 往返探测，返回 `pong`、`nonce`、Node 时间 |
| `echo` | 回显 `text` 字段 |
| `get_status` | 返回 Node 版本、平台、主机名、运行时长、实时 Agent 清单 |
| `list_sessions` | 刷新并返回目标 Agent 的 Session；不带 `agent_id` 时返回全部 |
| `send_prompt` | 向目标 Agent 的指定 Session 发送 Prompt |
| `interrupt` | 中断目标 Session 的当前执行 |
| `permission_response` | 回复 OpenCode 权限请求；协议层默认关闭，需显式加入本地策略（开发配置已开启） |

`send_prompt` / `interrupt` / `permission_response` 必须同时指定 `agent_id` 与 `session_id`：

```json
{
  "type": "command",
  "action": "send_prompt",
  "request_id": "req_...",
  "source": { "type": "device", "id": "web-001" },
  "target": {
    "node_id": "node-001",
    "agent_id": "opencode-main",
    "session_id": "ses_..."
  },
  "payload": { "text": "Fix the failing test" }
}
```

权限回复的 payload：

```json
{
  "permission_id": "per_...",
  "decision": "once"
}
```

`decision` 接受 `once` / `always` / `reject`（也接受 `allow`、`approved`、`yes`，归一化为 `once`）。

Node 对合法命令先回 `accepted`，执行后回 `completed` / `failed` / `rejected`。Cloud 会校验 Device ACL 与目标 Agent / Session 归属，Node 再校验协议白名单、本地策略、Adapter 能力与目标 Session。**不存在任意 Shell 执行入口。**

> `send_prompt` 的 `completed` 只表示 OpenCode 已接受入队。真正的模型输出通过 `message.*` / `tool.*` 事件流返回。

### 事件与 sequence

- Node 通过 `event` 消息上报 `AgentEvent`；Cloud 忽略 Node 提供的 sequence。
- Cloud 为每个 Node 维护独立、单调递增的权威 `sequence`，并生成 `event_id` 与时间戳。
- 事件进入 Event Cache 后，按 Device 的 Node ACL 过滤并广播。
- Cloud 自身生成 `node.online` / `node.offline` 生命周期事件；Node 注册后立即上报一次 `sync/agent_state`，随后 Adapter 的库存变化也会触发 `sync/agent_state`，Cloud 更新 Node View 并广播 Snapshot。
- Node 还会上报 `agent.connected` / `agent.disconnected` / `agent.status_changed`，以及 Adapter 归一化后的 `session.*`、`message.*`、`tool.*`、`permission.*` 事件。

### Snapshot 与事件补发

Device 注册成功后，Cloud 返回按 ACL 过滤的 `Snapshot`：

```json
{
  "nodes": [],
  "events": [],
  "last_sequences": { "node-001": 42, "node-002": 7 },
  "pending_permissions": [],
  "server_time": "2026-11-04T10:00:00Z"
}
```

- `nodes`：NodeView 列表，含其 Agent / Session。
- `events`：最近 `snapshot_events` 条可见事件。
- `last_sequences`：每个可见 Node 的最新 sequence。
- `pending_permissions`：由 `permission.requested` / `permission.resolved` 派生的待审批权限。

断线重连时，Device 在 `register` 中带上旧的 `last_sequences`，注册成功后再发送：

```json
{
  "type": "sync",
  "action": "events",
  "payload": {
    "last_sequences": { "node-001": 42 },
    "limit": 1000
  }
}
```

Cloud 返回 `sync/events`：

```json
{
  "events": [],
  "last_sequences": { "node-001": 57 },
  "truncated": [],
  "server_time": "2026-11-04T10:00:05Z"
}
```

- `truncated` 列出无法补齐的 Node（请求位置早于缓存起点，或请求位置超过当前最新 sequence）。
- 客户端收到截断信号后，应清空对应 Node 的本地事件并重新获取 Snapshot。
- Web 客户端会先应用 Snapshot，再用保留的 `last_sequences` 请求补发，并按 `node_id:sequence` 去重。

### 路由与状态

- Cloud 按 `request_id` 记录 pending 请求，并把 Node 的响应路由回发起 Device，不同设备之间不会串消息。
- 同一个在途 `request_id` 重复提交会被拒绝。
- Node 上下线、Device 注册时，Cloud 按 ACL 逐 Device 推送过滤后的 `sync/snapshot`。
- 同一身份重新连接会替换旧连接（旧连接以 `4001` 关闭）。
- 注册失败返回 `response` 并以 `4003` 断开。

---

## Phase 2 验收清单

| 验收项 | 状态 | 验证方式 |
|---|---|---|
| Node 连接 OpenCode | ✅ | `opencode.Adapter.Connect` 调用 `/api/info`，Basic Auth 实测 |
| 获取 Agent 状态 | ✅ | Manager 状态机 `offline/connecting/online/busy/error` + 集成测试 |
| 获取 Session 列表 | ✅ | `/api/session` + `/api/session/active` 合并，单元测试覆盖 |
| 订阅 OpenCode 事件 | ✅ | SSE `/api/event`，自动重连，Normalizer 表驱动测试 |
| 向指定 Session 发送 Prompt | ✅ | `send_prompt` → `POST /api/session/{id}/prompt` |
| 中断指定 Session | ✅ | `interrupt` → `POST /api/session/{id}/interrupt` |
| Agent 事件转换为统一事件 | ✅ | `normalizeEvent` 覆盖 session/message/tool/permission |
| 权限审批 | ✅ | `permission.asked` / `permission.replied` + `permission_response` |
| 本地安全策略 | ✅ | 默认白名单、Agent 覆盖、非法 action 拒绝，配置测试覆盖 |
| Cloud 库存同步 | ✅ | `sync/agent_state` → Node View → Snapshot 广播，集成测试覆盖 |
| 管理端点鉴权 | ✅ | `/api/v1/status`、`/api/v1/snapshot` 需要 `admin_token`，集成测试覆盖 |
| 网关限流 | ✅ | 单连接令牌桶，超限丢弃并返回 `rate_limited`，集成测试覆盖 |
| 断线补偿 | ✅ | Node 断开时把在途请求以 `failed` 回给 Device，集成测试覆盖 |
| 自动化测试 | ✅ | `go test ./... -race -count=1` |
| 前端构建 | ✅ | `npm run build` |

---

## Phase 3 验收清单

Phase 3 的范围是**真机闭环与加固**：不新增协议字段，用真实 OpenCode（v2.2.5）+ Cloud + Node + Web 测试客户端跑通四条链路。

验证环境：`configs/cloud.yaml` + `configs/node.yaml`（`node-001`，本地策略显式开启 `permission_response`）、`vite preview`（`http://127.0.0.1:4173`）、真实 OpenCode Server。E2E 通过 Chrome DevTools Protocol 驱动真实浏览器，脚本见 `scripts/e2e/`。

| 验收项 | 状态 | 真机证据 |
|---|---|---|
| Prompt 闭环 | ✅ | 9/9：浏览器向指定 Session 发送 `Reply with exactly one line: PONG-PHASE3`，`send_prompt` 返回 `accepted`/`completed`，事件流出现 `message.started` → `message.delta` → `message.completed`，`text` 为 `PONG-PHASE3` |
| Interrupt | ✅ | 7/7：`sleep 30` 生成中发送 `interrupt`，响应 `"interrupted": true`，Session 回到 `idle`，其后该 Session 无残留事件 |
| 权限审批（批准） | ✅ | 8/8：`read *.env` 触发 `permission.requested`，UI 点击「允许一次」后 `permission.resolved=approved`，工具执行并回传 `E2E_PERMISSION_PROBE` |
| 权限审批（拒绝） | ✅ | 9/9：点击「拒绝」后 `permission.resolved` 为 `rejected`，随后 `tool.failed`，回复未泄漏文件内容 |
| 断线恢复 | ✅ | 9/9：生成中断开 Device，离线窗口内新增事件在重连后全部补发，缺失 0，事件流无缺口，在途请求残留 0 |
| 事件归一化 | ✅ | `session.*` / `message.*` / `tool.*` / `permission.*` / `agent.*` 均在真机事件流中出现且顺序正确 |
| 版本与健康 | ✅ | `/api/v1/status` 返回 `version: 0.4.0-phase3`；`/healthz` 公开可用 |
| 回归 | ✅ | `gofmt -l .` 无输出、`go vet ./...`、`go test ./... -race -count=1`、`make build`、前端 `npm run typecheck` / `npm run build` 全部通过 |

**范围说明**

- 本阶段没有新增 Session 创建、多设备同时控制同一 Session 的冲突处理、ACL 管理 UI；这些留给后续阶段。
- 权限审批动作在协议层仍是可选项；本地开发配置为验证链路而开启，生产部署需显式评估（见「安全说明」）。
- Cloud 重启时，重启前的在途请求只能以 `failed` 回报 Device，这是既定行为，不是 Phase 3 缺陷。

---

## 安全说明

Phase 3 仍是**本地开发验证版本**，不可直接暴露到公网：

- 使用明文 `ws://`，生产环境必须通过反向代理提供 `wss://`。
- 使用配置中的静态 Token，生产环境必须替换，并在后续阶段引入配对流程与每设备凭据。
- 管理端点（`/api/v1/status`、`/api/v1/snapshot`）需要 `server.admin_token`，未配置时 Cloud 拒绝启动；`/healthz` 保持公开供探活使用。
- 每个 WebSocket 连接都有入站消息限流，避免单个客户端刷爆路由与事件缓存。
- Device 凭据必须显式配置 `allowed_nodes`；未授权 Node 默认拒绝。
- 测试客户端的 Token 保存在浏览器 `localStorage`。
- OpenCode 密码推荐通过 `password_env` 注入，不要提交到版本库；Node 日志不会输出密码。
- Node 只执行显式白名单动作，且受本地策略二次约束；Cloud 不执行任何 Agent 或 Shell 命令。
- `permission_response` 在协议层默认关闭。开启它意味着远程设备可以批准本机文件读写等权限请求，请只对可信 Device 开放；开发配置为验证链路已开启，生产部署应显式评估。
- Event Cache 位于内存中，服务重启会丢失短期事件；客户端可重新获取 Snapshot。
- 建议仅在 `127.0.0.1` 或受信网络内运行。

---

## 常用命令

```bash
make build        # 编译 bin/meetagent-cloud 与 bin/meetagent-node
make test         # go test ./... -race -count=1
make fmt          # gofmt
make vet          # go vet
make run-cloud    # 启动 Cloud
make run-node     # 启动 node-001（NODE_CONFIG 可覆盖）
make web-install  # 安装前端依赖
make web-dev      # 启动 Vite 开发服务器
make web-build    # 类型检查 + 生产构建
make clean        # 清理 bin/ 与前端 dist/
```

启动第二个 Node：

```bash
OPENCODE_PASSWORD="$(opencode service get password)" go run ./cmd/node -config configs/node-002.yaml
```

---

## 下一步

Phase 4 将把已验证的远程控制能力迁移到 ESP32 Terminal：ESP-IDF 工程、Wi-Fi 管理、WSS + TLS 证书校验、Device 身份认证、JSON 协议解析、FreeRTOS 任务与 LVGL 页面。Cloud / Node / 协议保持不变，ESP32 作为新的 Device 接入。