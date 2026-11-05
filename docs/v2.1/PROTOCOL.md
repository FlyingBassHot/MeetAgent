# MeetAgent V2.1 Client Protocol

> 状态：M0 定义 / M1 实现中
> 代码源：`pkg/clientproto/protocol.go`
> 传输：HTTPS + WSS（本地开发可用 HTTP/WS；公网必须 TLS）

## 1. 连接与配对

Node 启动时生成 6 位配对码，TTL 10 分钟，打印在 Node 日志中。

### `GET /api/v2/info`

无需认证，用于发现和诊断：

```json
{
  "schema_version": 1,
  "protocol_version": 1,
  "node": {
    "node_id": "node-home",
    "name": "developer-machine",
    "hostname": "perch-omarchy",
    "platform": "linux/amd64",
    "version": "2.1.0",
    "protocol_version": 1
  },
  "adapters": [
    {
      "id": "opencode",
      "name": "OpenCode Adapter",
      "version": "2.1.0",
      "api_version": 1,
      "capabilities": ["session.list", "session.prompt", "session.interrupt"],
      "permissions": ["session.read", "session.control"]
    }
  ],
  "pairing_required": true,
  "pairing_code_ttl_ms": 600000,
  "ws_path": "/ws"
}
```

### `POST /api/v2/pair`

首次配对可走 HTTP，也可直接在 WS `hello` 中带 `pairing_code`。

请求：

```json
{
  "client_type": "mobile",
  "client_version": "0.1.0",
  "name": "Pixel 8",
  "capabilities": ["ui.full", "input.text", "notification.system"],
  "pairing_code": "123456"
}
```

响应：

```json
{
  "schema_version": 1,
  "client_id": "cli_...",
  "token": "长期设备 Token，仅此次返回",
  "permissions": ["session.read", "session.control", "permission.respond"],
  "node": { "...": "NodeInfo" },
  "server_time": "2026-11-05T12:00:00Z"
}
```

Token 只保存哈希；设备端必须安全保存（Mobile 用 Keychain/Keystore，Terminal 用 NVS + flash encryption）。

### 管理端点

`server.admin_token` 非空时启用：

- `GET /api/v2/clients`：列出已配对客户端（不含 Token 哈希）
- `DELETE /api/v2/clients/{client_id}`：撤销客户端并关闭其在线连接

请求头：`Authorization: Bearer <admin_token>`。未配置 admin_token 时端点返回 401。

## 2. WebSocket 握手

连接 `ws://<node>:8787/ws`（公网 `wss://`）。

客户端第一条消息必须是 `hello`：

```json
{
  "schema_version": 1,
  "type": "hello",
  "message_id": "cmsg_...",
  "payload": {
    "client_type": "terminal",
    "client_version": "0.1.0",
    "name": "Desk Terminal",
    "token": "已配对 Token；首次可空",
    "pairing_code": "首次配对时填写",
    "capabilities": ["ui.compact", "display.touch", "input.button", "input.encoder", "notification.local"],
    "last_sequence": 128
  }
}
```

Node 回复 `welcome`：

```json
{
  "schema_version": 1,
  "type": "welcome",
  "message_id": "cmsg_...",
  "payload": {
    "client_id": "cli_...",
    "client_type": "terminal",
    "token": "仅首次配对返回",
    "node": { "...": "NodeInfo" },
    "snapshot": { "...": "Snapshot" },
    "server_time": "2026-11-05T12:00:00Z"
  }
}
```

握手失败返回 `error` 并关闭连接。

## 3. Snapshot

`welcome.payload.snapshot` 与 `GET /api/v2/snapshot` 使用同一结构：

```json
{
  "schema_version": 1,
  "node": { "...": "NodeInfo" },
  "adapters": [{ "...": "AdapterInfo" }],
  "agents": [
    {
      "agent_id": "opencode-main",
      "node_id": "node-home",
      "type": "opencode",
      "name": "OpenCode Main",
      "status": "busy",
      "capabilities": { "send_prompt": true, "interrupt": true },
      "sessions": [
        {
          "session_id": "ses_...",
          "agent_id": "opencode-main",
          "node_id": "node-home",
          "title": "MeetAgent",
          "status": "running",
          "updated_at": "2026-11-05T12:00:00Z"
        }
      ]
    }
  ],
  "events": [{ "...": "Event" }],
  "last_sequence": 128,
  "pending_permissions": [
    {
      "permission_id": "per_...",
      "node_id": "node-home",
      "agent_id": "opencode-main",
      "session_id": "ses_...",
      "description": "read /path/file",
      "status": "pending",
      "created_at": "2026-11-05T12:00:00Z"
    }
  ],
  "server_time": "2026-11-05T12:00:00Z"
}
```

## 4. Event

Node 为每个事件分配单调递增 `sequence`。客户端按 `sequence` 去重和补发。

```json
{
  "schema_version": 1,
  "event_id": "evt_...",
  "node_id": "node-home",
  "sequence": 129,
  "type": "message.delta",
  "occurred_at": "2026-11-05T12:00:01Z",
  "agent_id": "opencode-main",
  "session_id": "ses_...",
  "payload": { "message_id": "msg_...", "delta": "hello" },
  "source": {
    "adapter_id": "opencode",
    "native_event_type": "session.text.delta"
  }
}
```

事件类型沿用现有标准化名称：

```text
agent.connected / agent.disconnected / agent.status_changed / agent.error
session.created / session.updated / session.deleted / session.status_changed
message.started / message.delta / message.completed
tool.started / tool.completed / tool.failed
permission.requested / permission.resolved
```

客户端必须忽略未知的非关键事件；协议版本不兼容、认证失败、连接关闭必须显式处理。

## 5. Command

```json
{
  "schema_version": 1,
  "type": "command",
  "message_id": "cmsg_...",
  "payload": {
    "command_id": "cmd_...",
    "node_id": "node-home",
    "agent_id": "opencode-main",
    "session_id": "ses_...",
    "type": "session.prompt",
    "payload": { "text": "fix the failing test" },
    "created_at": "2026-11-05T12:00:02Z"
  }
}
```

命令类型：

| type | payload | 需要能力 | 需要权限 |
|---|---|---|---|
| `ping` | `{}` | — | — |
| `node.status` | `{}` | — | `session.read` |
| `session.list` | `{ "agent_id": "..." }` | — | `session.read` |
| `session.prompt` | `{ "text": "..." }` | `input.text` 或 `input.voice` | `session.control` |
| `session.interrupt` | `{}` | — | `session.control` |
| `permission.respond` | `{ "permission_id": "...", "decision": "once|always|reject" }` | — | `permission.respond` |

结果：

```json
{
  "schema_version": 1,
  "type": "command.result",
  "message_id": "cmsg_...",
  "payload": {
    "schema_version": 1,
    "command_id": "cmd_...",
    "status": "completed",
    "data": { "message_id": "msg_..." },
    "duration_ms": 12,
    "completed_at": "2026-11-05T12:00:02Z"
  }
}
```

规则：

- `command_id` 幂等；重复提交返回同一结果
- 同一 Session 的并发控制由 Node/Adapter 负责
- 多客户端审批只有一个最终有效决策
- 命令超时默认 15 秒（由 Node 配置）

## 6. 心跳

客户端可发送：

```json
{ "schema_version": 1, "type": "heartbeat", "payload": { "nonce": "..." } }
```

Node 回复：

```json
{ "schema_version": 1, "type": "heartbeat", "payload": { "nonce": "...", "server_time": "..." } }
```

WebSocket 层也可使用 ping/pong；JSON 心跳用于 UI 显示 RTT。

## 7. 重连

1. 客户端重新连接并发送 `hello`，带 `last_sequence`
2. Node 返回 `welcome` + 最新 Snapshot
3. Node 补发 `sequence > last_sequence` 的事件；如果超出缓存范围，客户端以 Snapshot 为准
4. 客户端清空本地旧状态，应用 Snapshot，再应用补发事件

## 8. 安全

- LAN 内也建议启用 WSS；至少公网必须 TLS
- 配对码 TTL 10 分钟；Token 只返回一次
- Token 存哈希；客户端安全存储
- 设备可单独撤销；不得与 Mobile 共享长期 Token
- 终端首版只连 LAN；Cloud/VPN 由 Mobile 负责
