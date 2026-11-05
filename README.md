# MeetAgent V2.1

面向 Coding Agent 的本地优先控制与管理平台。

**Node-Centric · Multi-Client · Plugin-Based · LAN First · Cloud Optional**

当前状态：**V2.1 大框架已落地**。Node 独立服务、V2.1 客户端协议、配对、事件中心、Mock Adapter 和协议模拟器已完成；Mobile M2（Mock）与 Terminal E0–E2（host tests）已完成，下一步进入真实 OpenCode 双端集成。

> 设计文档：
> - [`MeetAgentV2.1.md`](./MeetAgentV2.1.md) — 总体架构、双客户端、南向插件、阶段计划
> - [`TerminalUi.md`](./TerminalUi.md) — ESPUI 与 Terminal Aesthetic 主题系统
> - [`docs/v2.1/EXECUTION.md`](./docs/v2.1/EXECUTION.md) — 技术总监执行计划与模块边界
> - [`docs/v2.1/PROTOCOL.md`](./docs/v2.1/PROTOCOL.md) — Node ↔ Mobile/Terminal 协议
> - [`docs/v1/README.md`](./docs/v1/README.md) — 已归档的 V1 Cloud 方案

---

## 架构

```text
Mobile (Vue + Capacitor)        ESP32 Terminal (ESP-IDF + LVGL)
              \                /
               Client Protocol v2
                     |
                MeetAgent Node
           (standalone HTTP/WSS)
                     |
        ┌────────────┼────────────┐
        |            |            |
   Domain Core  Client Registry  Event Hub
        |
   Adapter Registry
        |
  OpenCode / Mock / Agent B
```

- **Node** 是核心：Agent 接入、Session、事件、命令、权限、配对、客户端注册
- **Mobile** 是完整远程入口：LAN 直连，后续 Tailscale/Capacitor
- **Terminal** 是局域网常驻终端：状态感知、物理交互、快捷控制，不承担完整聊天
- **Cloud** 不再是必需项；旧 Cloud 代码保留为可选 relay
- **Adapter** 通过统一 Manifest/Capability 接入，OpenCode 只是第一个 Adapter

---

## 模块

| 模块 | 目录 | 状态 |
|---|---|---|
| Core / Node | `cmd/node`, `internal/`, `pkg/`, `configs/` | M0/M1 完成 |
| Node 模拟器 | `cmd/simulator` | 可用 |
| Mobile | `mobile/` | M2 完成（Mock + 真实 Node 握手已验证） |
| ESP32 Terminal / ESPUI | `firmware/terminal/` | E0–E2 完成（host tests 620/620） |
| V1 Cloud / Web Console | `cmd/cloud`, `web/test-client` | 归档参考 |

---

## 快速开始（Mock）

不依赖 OpenCode、Cloud 或硬件：

```bash
# 1. 启动带 Mock Agent 的 Node
make run-node-mock

# 日志会打印配对码，例如：
#   msg="pairing code ready" code=394631

# 2. 连接协议模拟器（另开终端）
go run ./cmd/simulator -pair-code 394631 -prompt "hello mock"

# 输出的 token 可复用：
go run ./cmd/simulator -token <token> -last-sequence 10
```

默认端口：

- Node HTTP/WSS：`0.0.0.0:8787`
- WebSocket：`ws://127.0.0.1:8787/ws`
- 健康检查：`http://127.0.0.1:8787/healthz`
- 发现信息：`http://127.0.0.1:8787/api/v2/info`

---

## 快速开始（真实 OpenCode）

```bash
export OPENCODE_PASSWORD="$(opencode service get password)"
make run-node            # configs/node.yaml
```

`configs/node.yaml` 默认：

- 本地服务 `0.0.0.0:8787`
- Cloud relay `cloud.enabled: false`
- OpenCode Adapter `opencode-main`
- mDNS：`_meetagent._tcp`
- 配对存储：`./data/paired-clients.json`（已 gitignore）

---

## 配置

### `configs/node.yaml`

```yaml
node:
  id: "node-001"
  name: "developer-machine"
  agents:
    - id: "opencode-main"
      type: "opencode"
      connection:
        base_url: "http://127.0.0.1:49374"
        password_env: "OPENCODE_PASSWORD"

server:
  enabled: true
  addr: "0.0.0.0:8787"
  pairing_ttl: 10m
  pairing_store: "./data/paired-clients.json"
  mdns: true

cloud:
  enabled: false
```

### `configs/node-mock.yaml`

不依赖外部服务的 Mock 配置：

```yaml
agents:
  - id: "mock-1"
    type: "mock"
    sessions:
      - id: "session-1"
        title: "Mock Session"
```

---

## 协议与安全

- 客户端先通过 6 位配对码换取独立 Token；Token 只保存 SHA-256 哈希
- WebSocket 首包 `hello`，Node 返回 `welcome + snapshot`
- 事件带单调递增 `sequence`，重连时按 `last_sequence` 补发
- 命令带 `command_id`，Node 做幂等、权限、能力和超时校验
- 三种能力分离：Client Capability / Agent Capability / User Permission
- 终端首版只连 LAN；公网部署必须 `wss://`，并替换开发凭据
- 不要把真实 Token 写入仓库

详见 [`docs/v2.1/PROTOCOL.md`](./docs/v2.1/PROTOCOL.md)。

---

## 开发命令

```bash
make build            # 编译 meetagent-cloud / meetagent-node
make simulator        # 编译协议模拟器
make test             # go test ./... -race -count=1
make fmt
make vet
make run-node         # 真实 OpenCode 配置
make run-node-mock    # Mock Agent 配置
make run-cloud        # 旧 V1 Cloud（可选）
```

---

## V2.1 阶段

| 阶段 | 内容 | 状态 |
|---|---|---|
| M0 | 协议、Adapter Manifest、Mock、Simulator | 完成 |
| M1 | Node Standalone、配对、Client Registry、Snapshot/Event/Command | 完成 |
| M2 | Mobile M2 + Terminal E0–E2 | 完成（Mock / host tests） |
| M3 | 真实 OpenCode 双端集成 | 下一步 |
| M4 | 双端一致性与稳定性 | 待开始 |
| M5 | Mobile 远程（Tailscale/Capacitor） | 待开始 |
| M6 | 语音/STT、Cloud Relay、第二 Adapter、OTA | 暂缓 |

> 语音/STT 按当前要求暂缓，先完成 Node、Mobile、Terminal 大框架。
